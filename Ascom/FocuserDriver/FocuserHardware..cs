using ASCOM.LocalServer;
using ASCOM.Utilities;
using System;
using System.Globalization;
using System.Windows.Forms;

namespace ASCOM.BrebissonV1.Focuser
{
    [HardwareClass()] // Class attribute flag this as a device hardware class that needs to be disposed by the local server when it exits.
    internal static class FocuserHardware
    {
        // Constants used for Profile persistence
        internal const string comPortProfileName = "COM Port";
        internal const string comPortDefault = "COM1";
        internal const string StepSizeProfileName = "StepSize";
        private static string DriverProgId = ""; // ASCOM DeviceID (COM ProgID) for this driver, the value is set by the driver's class initialiser.
        public static int slowSpeed = 10, fastSpeed = 2000;
        static FocuserHardware()
        {
            DriverProgId = Focuser.DriverProgId; // Get this device's ProgID so that it can be used to read the Profile configuration values
            try
            {
                using (Profile driverProfile = new Profile())
                {
                    driverProfile.DeviceType = "Focuser";
                    SharedResources.comPort = driverProfile.GetValue(DriverProgId, comPortProfileName, string.Empty, comPortDefault);
                    int i;
                    if (int.TryParse(driverProfile.GetValue(DriverProgId, "slowSpeed", "10"), out i)) slowSpeed = i;
                    if (int.TryParse(driverProfile.GetValue(DriverProgId, "fastSpeed", "2000"), out i)) fastSpeed = i;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"{ex.Message}", "Exception creating ASCOM.BrebissonV1.Focuser", MessageBoxButtons.OK, MessageBoxIcon.Error);
                throw;
            }
        }

        public static void SetupDialog()
        {
            // Don't permit the setup dialogue if already connected
            if (SharedResources.Connected) MessageBox.Show("Already connected, just press OK");
            using (SetupDialogForm F = new SetupDialogForm())
            {
                var result = F.ShowDialog();
                if (result == DialogResult.OK) saveProfile();
            }
        }
        public static void saveProfile()
        {
            using (Profile driverProfile = new Profile())
            {
                driverProfile.DeviceType = "Focuser";
                driverProfile.WriteValue(DriverProgId, comPortProfileName, SharedResources.comPort.ToString());
                driverProfile.WriteValue(DriverProgId, "slowSpeed", slowSpeed.ToString());
                driverProfile.WriteValue(DriverProgId, "fastSpeed", fastSpeed.ToString());
            }
        }
        public static void Dispose() { }
        public static bool Connected
        {
            get { return SharedResources.Connected; }
            set { SharedResources.Connected = value;  }
        }
        internal static bool Link /// State of the connection to the focuser.
        {
            get { return Connected; } // Direct function to the connected method, the Link method is just here for backwards compatibility
            set { Connected = value; } // Direct function to the connected method, the Link method is just here for backwards compatibility
        }
        public static string Description { get { return "Brebisson combined mount focusser"; } }
        public static string DriverInfo { get { return "Information about the driver itself. Version: 1.0"; } }
        public static string DriverVersion { get { return "1.0"; } }
        public static short InterfaceVersion { get { return 3; } }
        public static string Name { get { return "Brebisson Combined Mount Focus V1"; } }
        internal static bool Absolute { get { return true; } } // This is an absolute focuser
        internal static void Halt() { SharedResources.SendSerialCommand(":Q#", 0); SharedResources.doLog("Foc stop", 0); } /// Immediately stop any focuser motion due to a previous <see cref="Move" /> method call.
        internal static bool IsMoving { get { return SharedResources.FocusMoving; } } /// True if the focuser is currently moving to a new position. False if the focuser is stationary.
        internal static int MaxIncrement { get { return 0x7fffffff; } } /// Maximum increment size allowed by the focuser; i.e. the maximum number of steps allowed in one move operation.
        internal static int MaxStep { get { return 0x7fffffff; } } /// Maximum step position permitted.
        /// Moves the focuser by the specified amount or to the specified position depending on the value of the <see cref="Absolute" /> property.
        /// <param name="Position">Step distance or absolute position, depending on the value of the <see cref="Absolute" /> property.</param>
        internal static void Move(int Position)
        {
            SharedResources.SendSerialCommand(":FG"+Position.ToString("X8")+"#", 0);
            SharedResources.doLog("Foc move to "+Position.ToString()+" from "+SharedResources.FocusserPosition.ToString()+" delta "+(Position-SharedResources.FocusserPosition).ToString(), 0); 
        }
        public static void moveIn(int spd) { SharedResources.SendSerialCommand(":FM" + (-spd).ToString("X8") + '#'); }
        public static void moveOut(int spd) { SharedResources.SendSerialCommand(":FM" + spd.ToString("X8") + '#'); }
        internal static void Stop() { SharedResources.SendSerialCommand(":Q#", 0); SharedResources.doLog("Foc stop", 0); }
        internal static int Position { get { return SharedResources.FocusserPosition; } } // Return the focuser position
        internal static double StepSize /// Step size (microns) for the focuser.
        {
            get { return SharedResources.FocStepdum/10.0; }
            set { SharedResources.FocStepdum = (int)(value*10); }
        }
        internal static bool TempComp { get { return false; } set { } }
        internal static bool TempCompAvailable { get { return false; } } // Temperature compensation is not available in this driver
        internal static double Temperature { get { return 20.0; } } /// Current ambient temperature in degrees Celsius as measured by the focuser.
    }
}

