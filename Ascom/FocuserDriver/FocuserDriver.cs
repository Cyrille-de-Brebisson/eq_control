using ASCOM.DeviceInterface;
using ASCOM.LocalServer;
using System;
using System.Collections;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace ASCOM.BrebissonV1.Focuser
{
    // This code is mostly a presentation layer for the functionality in the FocuserHardware class. You should not need to change the contents of this file very much, if at all.
    // Most customisation will be in the FocuserHardware class, which is shared by all instances of the driver, and which must handle all aspects of communicating with your device.
    [ComVisible(true)]
    [Guid("ea8f2e77-6a97-4c38-a769-2a1b2123ae38")]
    [ProgId("ASCOM.BrebissonV1.Focuser")]
    [ServedClassName("ASCOM Focuser Driver for BrebissonV1")] // Driver description that appears in the Chooser, customise as required
    [ClassInterface(ClassInterfaceType.None)]
    public class Focuser : ReferenceCountedObjectBase, IFocuserV3, IDisposable
    {
        internal static string DriverProgId; // ASCOM DeviceID (COM ProgID) for this driver, the value is retrieved from the ServedClassName attribute in the class initialiser.
        internal static string DriverDescription; // The value is retrieved from the ServedClassName attribute in the class initialiser.
        // connectedState holds the connection state from this driver instance's perspective, as opposed to the local server's perspective, which may be different because of other client connections.
        internal bool connectedState= false; // The connected state from this driver's perspective
        public Focuser()
        {
            try
            {
                // Pull the ProgID from the ProgID class attribute.
                Attribute attr = Attribute.GetCustomAttribute(this.GetType(), typeof(ProgIdAttribute));
                DriverProgId = ((ProgIdAttribute)attr).Value ?? "PROGID NOT SET!";  // Get the driver ProgIDfrom the ProgID attribute.
                // Pull the display name from the ServedClassName class attribute.
                attr = Attribute.GetCustomAttribute(this.GetType(), typeof(ServedClassNameAttribute));
                DriverDescription = ((ServedClassNameAttribute)attr).DisplayName ?? "DISPLAY NAME NOT SET!";  // Get the driver description that displays in the ASCOM Chooser from the ServedClassName attribute.
            }
            catch (Exception ex)
            {
                MessageBox.Show($"{ex.Message}", "Exception creating ASCOM.BrebissonV1.Focuser", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        ~Focuser() { }
        public void Dispose() { }
        public void SetupDialog() { FocuserHardware.SetupDialog(); }
        public ArrayList SupportedActions { get { return new ArrayList(); } }
        public string Action(string actionName, string actionParameters) { return ""; }
        public void CommandBlind(string command, bool raw) { }
        public bool CommandBool(string command, bool raw) { return false;  }
        public string CommandString(string command, bool raw) { return "";  }
        public bool Connected
        {
            get { return connectedState; }
            set { if (connectedState==value) return; FocuserHardware.Connected= value; if (!value) connectedState = false; else connectedState = FocuserHardware.Connected; }
        }
        public string Description { get { return FocuserHardware.Description; } }
        public string DriverInfo { get { return FocuserHardware.DriverInfo; } }
        public string DriverVersion { get { return FocuserHardware.DriverVersion; } }
        public short InterfaceVersion { get { return FocuserHardware.InterfaceVersion; } }
        public string Name { get { return FocuserHardware.Name; } }
        public bool Absolute { get { return FocuserHardware.Absolute; } }
        public void Halt() { FocuserHardware.Halt(); }
        public bool IsMoving { get { return FocuserHardware.IsMoving; } }
        public bool Link { get{ return Connected; } set { Connected = value; } }
        public int MaxIncrement { get { return FocuserHardware.MaxIncrement; } }
        public int MaxStep { get { return FocuserHardware.MaxStep; } }
        public void Move(int position) { FocuserHardware.Move(position); }
        public void Stop() { FocuserHardware.Stop(); }
        public int Position { get { return FocuserHardware.Position; } }
        public double StepSize { get { return FocuserHardware.StepSize; } set { FocuserHardware.StepSize = value; } }
        public bool TempComp { get { return FocuserHardware.TempComp; } set {  FocuserHardware.TempComp= value;} }
        public bool TempCompAvailable { get { return FocuserHardware.TempCompAvailable; } }
        public double Temperature { get { return FocuserHardware.Temperature; } }
    }
}
