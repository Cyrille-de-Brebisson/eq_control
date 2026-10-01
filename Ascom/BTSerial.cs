using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.Advertisement;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Foundation;
using Windows.Storage.Streams;

/// <summary>
/// Serial-like link over BLE Nordic UART Service (NUS) to an ESP32-C3.
///   - ESP32 ble_nus_send_data()  -> notifications on TX char (6E400003) -> read()
///   - write()                    -> writes to RX char (6E400002)         -> ESP32 receive callback
/// Requires NuGet: Microsoft.Windows.SDK.Contracts (PackageReference style csproj), Windows 10 1709+.
/// </summary>
public class BTSerial : IDisposable
{
    static readonly Guid NusService = new Guid("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
    static readonly Guid NusRx      = new Guid("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"); // central -> device
    static readonly Guid NusTx      = new Guid("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"); // device -> central (notify)

    readonly string target;          // advertised name or "AA:BB:CC:DD:EE:FF"
    readonly ulong? targetAddress;

    BluetoothLEDevice device;
    GattSession session;
    GattDeviceService service;
    GattCharacteristic rxChar, txChar;

    readonly object bufLock = new object();
    readonly StringBuilder buffer = new StringBuilder();
    readonly Decoder decoder = Encoding.UTF8.GetDecoder();
    volatile bool connected;

    /// <summary>Max time to look for the device while scanning.</summary>
    public int ScanTimeoutMs { get; set; } = 10000;

    /// <param name="nameOrAddress">Advertised device name, or MAC address like "AA:BB:CC:DD:EE:FF".</param>
    public BTSerial(string nameOrAddress)
    {
        if (string.IsNullOrWhiteSpace(nameOrAddress))
            throw new ArgumentException("Device name or address required", nameof(nameOrAddress));

        target = nameOrAddress.Trim();
        string hex = target.Replace(":", "");
        if (target.Contains(":") && hex.Length == 12 &&
            ulong.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out ulong addr))
            targetAddress = addr;
    }

    // ---------------------------------------------------------------- public API

    public void connect()
    {
        if (connected) return;
        try
        {
            ulong address = targetAddress ?? Scan();

            device = Await(BluetoothLEDevice.FromBluetoothAddressAsync(address));
            if (device == null) throw new IOException("Unable to open BLE device");

            // Keep the link alive and let us query the negotiated MTU
            session = Await(GattSession.FromDeviceIdAsync(device.BluetoothDeviceId));
            session.MaintainConnection = true;

            var svcRes = Await(device.GetGattServicesForUuidAsync(NusService, BluetoothCacheMode.Uncached));
            if (svcRes.Status != GattCommunicationStatus.Success || svcRes.Services.Count == 0)
                throw new IOException("Nordic UART service not found (" + svcRes.Status + ")");
            service = svcRes.Services[0];

            var rxRes = Await(service.GetCharacteristicsForUuidAsync(NusRx, BluetoothCacheMode.Uncached));
            var txRes = Await(service.GetCharacteristicsForUuidAsync(NusTx, BluetoothCacheMode.Uncached));
            if (rxRes.Status != GattCommunicationStatus.Success || rxRes.Characteristics.Count == 0 ||
                txRes.Status != GattCommunicationStatus.Success || txRes.Characteristics.Count == 0)
                throw new IOException("Nordic UART characteristics not found");
            rxChar = rxRes.Characteristics[0];
            txChar = txRes.Characteristics[0];

            lock (bufLock) { buffer.Clear(); }

            txChar.ValueChanged += OnValueChanged;

            var all = Await(service.GetCharacteristicsAsync(BluetoothCacheMode.Uncached));

            var mode = (txChar.CharacteristicProperties & GattCharacteristicProperties.Notify) != 0
                ? GattClientCharacteristicConfigurationDescriptorValue.Notify
                : GattClientCharacteristicConfigurationDescriptorValue.Indicate;

            var st = Await(txChar.WriteClientCharacteristicConfigurationDescriptorAsync(mode));
            var back = Await(txChar.ReadClientCharacteristicConfigurationDescriptorAsync());

            if (st != GattCommunicationStatus.Success)
                throw new IOException("Unable to enable notifications (" + st + ")");

            device.ConnectionStatusChanged += OnConnectionStatusChanged;
            connected = true;
        }
        catch
        {
            cleanup();
            throw;
        }
    }

    public void disconnect()
    {
        cleanup();
    }

    public bool isConnected()
    {
        return connected && device != null && device.ConnectionStatus == BluetoothConnectionStatus.Connected;
    }

    /// <summary>
    /// Waits up to <paramref name="timeout"/> ms for data and returns everything received so far.
    /// Returns "" on timeout. Use Timeout.Infinite (-1) to wait forever.
    /// </summary>
    public string read(int timeout)
    {
        var sw = Stopwatch.StartNew();
        lock (bufLock)
        {
            while (buffer.Length == 0 && connected)
            {
                int wait = Timeout.Infinite;
                if (timeout >= 0)
                {
                    wait = timeout - (int)sw.ElapsedMilliseconds;
                    if (wait <= 0) break;
                }
                Monitor.Wait(bufLock, wait);
            }
            string s = buffer.ToString();
            buffer.Clear();
            return s;
        }
    }

    public void write(string text)
    {
        if (!isConnected()) throw new IOException("BTSerial is not connected");
        if (string.IsNullOrEmpty(text)) return;

        byte[] data = Encoding.UTF8.GetBytes(text);

        // ATT payload = MTU - 3. Fall back to the 20-byte default if MTU is unknown.
        int chunk = 20;
        try { if (session != null && session.MaxPduSize > 23) chunk = session.MaxPduSize - 3; } catch { }

        bool noResp = (rxChar.CharacteristicProperties & GattCharacteristicProperties.WriteWithoutResponse) != 0;
        var option = noResp ? GattWriteOption.WriteWithoutResponse : GattWriteOption.WriteWithResponse;

        for (int offset = 0; offset < data.Length; offset += chunk)
        {
            int n = Math.Min(chunk, data.Length - offset);
            var w = new DataWriter();
            w.WriteBytes(data.Skip(offset).Take(n).ToArray());
            var res = Await(rxChar.WriteValueWithResultAsync(w.DetachBuffer(), option));
            if (res.Status != GattCommunicationStatus.Success)
                throw new IOException("BLE write failed (" + res.Status + ")");
        }
    }

    public void Dispose()
    {
        cleanup();
    }

    // ---------------------------------------------------------------- internals

    void OnValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args)
    {
        var reader = DataReader.FromBuffer(args.CharacteristicValue);
        byte[] bytes = new byte[reader.UnconsumedBufferLength];
        reader.ReadBytes(bytes);

        lock (bufLock)
        {
            // Stateful decoder handles multi-byte UTF-8 characters split across packets
            char[] chars = new char[decoder.GetCharCount(bytes, 0, bytes.Length)];
            int n = decoder.GetChars(bytes, 0, bytes.Length, chars, 0);
            buffer.Append(chars, 0, n);
            Monitor.PulseAll(bufLock);
        }
    }

    void OnConnectionStatusChanged(BluetoothLEDevice sender, object args)
    {
        if (sender.ConnectionStatus != BluetoothConnectionStatus.Connected)
        {
            connected = false;
            lock (bufLock) { Monitor.PulseAll(bufLock); }
        }
    }

    ulong Scan()
    {
        var tcs = new TaskCompletionSource<ulong>();
        var watcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };
        watcher.Received += (w, a) =>
        {
            // Match on name (name may be in the advertisement or the scan response)
            if (a.Advertisement.LocalName == target)
                tcs.TrySetResult(a.BluetoothAddress);
        };
        watcher.Start();
        try
        {
            if (!tcs.Task.Wait(ScanTimeoutMs))
                throw new TimeoutException("BLE device '" + target + "' not found");
            return tcs.Task.Result;
        }
        finally { watcher.Stop(); }
    }

    void cleanup()
    {
        connected = false;
        try
        {
            if (txChar != null)
            {
                txChar.ValueChanged -= OnValueChanged;
                try
                {
                    Await(txChar.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.None));
                }
                catch { }
            }
        }
        catch { }
        try { if (device != null) device.ConnectionStatusChanged -= OnConnectionStatusChanged; } catch { }
        try { service?.Dispose(); } catch { }
        try { session?.Dispose(); } catch { }
        try { device?.Dispose(); } catch { }
        service = null; session = null; device = null; rxChar = null; txChar = null;
        lock (bufLock) { Monitor.PulseAll(bufLock); }
    }

    // Runs the WinRT async op on the thread pool so it can't deadlock on an STA/UI thread
    static T Await<T>(IAsyncOperation<T> op)
    {
        return Task.Run(() => op.AsTask()).GetAwaiter().GetResult();
    }
}