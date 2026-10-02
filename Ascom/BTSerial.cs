using System;
using System.Collections.Generic;
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

    BluetoothLEDevice device;
    GattSession session;
    GattDeviceService service;
    GattCharacteristic rxChar, txChar;

    readonly object bufLock = new object();
    readonly StringBuilder buffer = new StringBuilder();
    readonly Decoder decoder = Encoding.UTF8.GetDecoder();
    volatile bool connected;

    // Background scanner for EQ devices
    readonly Dictionary<ulong, (string name, DateTime lastSeen)> eqDevices = new Dictionary<ulong, (string, DateTime)>();
    readonly object eqDevicesLock = new object();
    BluetoothLEAdvertisementWatcher eqWatcher;
    CancellationTokenSource eqScannerCts;
    Task eqScannerTask;

    /// <summary>Max time to look for the device while scanning.</summary>
    public int ScanTimeoutMs { get; set; } = 10000;

    /// <summary>Timeout in seconds for EQ devices to be removed if they stop advertising. Default: 60 seconds.</summary>
    public int EQDeviceTimeoutSeconds { get; set; } = 10;

    /// <param name="nameOrAddress">Advertised device name, or MAC address like "AA:BB:CC:DD:EE:FF".</param>
    public BTSerial()
    {
        // Start background scanner for EQ devices
        StartEQScanner();
    }

    // ---------------------------------------------------------------- public API

    public void connect(string nameOrAddress)
    {
            if (connected) return;
        try
        {
            nameOrAddress= nameOrAddress.Trim();
            ulong? addressFromDict = null;
            lock (eqDevicesLock)
            {
                var entry = eqDevices.FirstOrDefault(kvp => kvp.Value.name == nameOrAddress);
                if (entry.Value.name != null) addressFromDict = entry.Key;
            }
            ulong address = addressFromDict ?? Scan(nameOrAddress);

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

    /// <summary>
    /// Returns the names of all currently discovered BT advertisers starting with "EQ".
    /// This list is continuously maintained in the background and stale devices are automatically removed.
    /// </summary>
    public string[] GetEQDevices()
    {
        lock (eqDevicesLock)
        {
            return eqDevices.Values.Select(v => v.name).Distinct().OrderBy(s => s).ToArray();
        }
    }

    // ---------------------------------------------------------------- internals

    void StartEQScanner()
    {
        eqScannerCts = new CancellationTokenSource();
        eqScannerTask = Task.Run(() => EQScannerLoop(eqScannerCts.Token));
    }

    void EQScannerLoop(CancellationToken ct)
    {
        try
        {
            eqWatcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };

            eqWatcher.Received += (w, a) =>
            {
                string name = a.Advertisement.LocalName;
                if (!string.IsNullOrEmpty(name) && name.StartsWith("EQ"))
                {
                    lock (eqDevicesLock)
                    {
                        eqDevices[a.BluetoothAddress] = (name, DateTime.UtcNow);
                    }
                }
            };

            eqWatcher.Start();

            // Keep scanning until cancelled, periodic cleanup of stale devices
            while (!ct.IsCancellationRequested)
            {
                Thread.Sleep(1000);
                CleanupStaleEQDevices();
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine("EQ Scanner error: " + ex.Message);
        }
        finally
        {
            try { eqWatcher?.Stop(); } catch { }
        }
    }

    void CleanupStaleEQDevices()
    {
        lock (eqDevicesLock)
        {
            DateTime cutoff = DateTime.UtcNow.AddSeconds(-EQDeviceTimeoutSeconds);
            var staleKeys = eqDevices.Where(kvp => kvp.Value.lastSeen < cutoff).Select(kvp => kvp.Key).ToList();
            foreach (var key in staleKeys)
            {
                eqDevices.Remove(key);
            }
        }
    }

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

    ulong Scan(string nameOrAddress)
    {
        var tcs = new TaskCompletionSource<ulong>();
        var watcher = new BluetoothLEAdvertisementWatcher { ScanningMode = BluetoothLEScanningMode.Active };
        watcher.Received += (w, a) =>
        {
            // Match on name (name may be in the advertisement or the scan response)
            if (a.Advertisement.LocalName == nameOrAddress)
                tcs.TrySetResult(a.BluetoothAddress);
        };
        watcher.Start();
        try
        {
            if (!tcs.Task.Wait(ScanTimeoutMs))
                throw new TimeoutException("BLE device '" + nameOrAddress + "' not found");
            return tcs.Task.Result;
        }
        finally { watcher.Stop(); }
    }

    void cleanup()
    {
        connected = false;

        // Stop EQ scanner
        try { eqScannerCts?.Cancel(); } catch { }
        try { eqWatcher?.Stop(); } catch { }
        try { eqScannerTask?.Wait(1000); } catch { }
        eqWatcher = null;
        eqScannerCts = null;
        eqScannerTask = null;

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