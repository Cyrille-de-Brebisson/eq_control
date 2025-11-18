using ASCOM.Astrometry;
using ASCOM.Astrometry.NOVAS;
using ASCOM.DeviceInterface;
using ASCOM.LocalServer;
using ASCOM.Utilities;
using ASCOM.Utilities.Interfaces;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Security.Cryptography;
using System.Windows.Forms;

namespace ASCOM.BrebissonV1.Telescope
{
    /// ASCOM Telescope hardware class for BrebissonV1.
    [HardwareClass()] // Class attribute flag this as a device hardware class that needs to be disposed by the local server when it exits.
    internal static class TelescopeHardware
    {
        // Constants used for Profile persistence
        internal const string comPortProfileName = "COM Port";
        internal const string comPortDefault = "COM1";
        private static string DriverProgId = ""; // ASCOM DeviceID (COM ProgID) for this driver, the value is set by the driver's class initialiser.

        /// Initializes a new instance of the device Hardware class.
        static TelescopeHardware()
        {
            try
            {
                // DriverProgId has to be set here because it used by ReadProfile to get the TraceState flag.
                DriverProgId = Telescope.DriverProgId; // Get this device's ProgID so that it can be used to read the Profile configuration values
                using (Profile driverProfile = new Profile())
                {
                    driverProfile.DeviceType = "Telescope";
                    SharedResources.comPort = driverProfile.GetValue(DriverProgId, comPortProfileName, string.Empty, comPortDefault);
                    SharedResources.guideAfterSlew = driverProfile.GetValue(DriverProgId, "guideAfterSlew", "", "0")!="0";
                    SharedResources.yellOnPower= driverProfile.GetValue(DriverProgId, "yellOnPower", "", "0")!="0";
                    SharedResources.focusInmm= driverProfile.GetValue(DriverProgId, "focusInmm", "", "0")!="0";
                    double v;
                    if (double.TryParse(driverProfile.GetValue(DriverProgId, "guideRaAgressivity", "", "1.0"), NumberStyles.Float, CultureInfo.InvariantCulture, out v))
                        SharedResources.guideRaAgressivity = v;
                    if (double.TryParse(driverProfile.GetValue(DriverProgId, "guideDecAgressivity", "", "1.0"), NumberStyles.Float, CultureInfo.InvariantCulture, out v))
                        SharedResources.guideDecAgressivity = v;
                    SharedResources.isstle1= driverProfile.GetValue(DriverProgId, "isstle1", "", "");
                    SharedResources.isstle2= driverProfile.GetValue(DriverProgId, "isstle2", "", "");
                    SharedResources.locations= driverProfile.GetValue(DriverProgId, "locations", "", "");
                    SharedResources.scopes= driverProfile.GetValue(DriverProgId, "scopes", "", "");
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"{ex.Message}", "Exception creating ASCOM.BrebissonV1.Telescope", MessageBoxButtons.OK, MessageBoxIcon.Error);
                throw;
            }
        }
        public static void saveProfile()
        {
            using (Profile driverProfile = new Profile())
            {
                driverProfile.DeviceType = "Telescope";
                driverProfile.WriteValue(DriverProgId, comPortProfileName, SharedResources.comPort.ToString());
                driverProfile.WriteValue(DriverProgId, "guideAfterSlew", SharedResources.guideAfterSlew ? "1" : "0");
                driverProfile.WriteValue(DriverProgId, "yellOnPower", SharedResources.yellOnPower ? "1" : "0");
                driverProfile.WriteValue(DriverProgId, "focusInmm", SharedResources.focusInmm ? "1" : "0");
                driverProfile.WriteValue(DriverProgId, "guideRaAgressivity", SharedResources.guideRaAgressivity.ToString(CultureInfo.InvariantCulture));
                driverProfile.WriteValue(DriverProgId, "guideDecAgressivity", SharedResources.guideDecAgressivity.ToString(CultureInfo.InvariantCulture));
            }
        }
        public static void saveisstls(string isstle1, string isstle2)
        {
            SharedResources.isstle1 = isstle1; SharedResources.isstle2 = isstle2;
            using (Profile driverProfile = new Profile())
            {
                driverProfile.DeviceType = "Telescope";
                driverProfile.WriteValue(DriverProgId, "isstle1", isstle1);
                driverProfile.WriteValue(DriverProgId, "isstle2", isstle2);
            }
        }
        public static void savelocations(string locations)
        {
            SharedResources.locations = locations; 
            using (Profile driverProfile = new Profile())
            {
                driverProfile.DeviceType = "Telescope";
                driverProfile.WriteValue(DriverProgId, "locations", locations);
            }
        }
        public static void savescopes(string scopes)
        {
            SharedResources.scopes = scopes; 
            using (Profile driverProfile = new Profile())
            {
                driverProfile.DeviceType = "Telescope";
                driverProfile.WriteValue(DriverProgId, "scopes", scopes);
            }
        }
        public static void SetupDialog()
        {
            (new SetupDialogForm()).ShowDialog();
        }

        public static void Dispose() { }
        public static bool Connected
        {
            get { return SharedResources.Connected; }
            set { SharedResources.Connected = value; }
        }
        public static string Description { get { return Telescope.DriverDescription; } }
        public static string DriverInfo /// Descriptive and version information about this ASCOM driver.
        {
            get
            {
                Version version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
                return $"Information about the driver itself. Version: {version.Major}.{version.Minor}";
            }
        }
        public static string DriverVersion { get { return "1.0"; } }
        public static short InterfaceVersion { get { return 3; } }
        public static string Name { get { return "Brebisson Combined Mount Focus V1"; } }
        internal static void AbortSlew() { SharedResources.SendSerialCommand(":Q#", 0); SharedResources.doLog("Mount stop", 0); }
        internal static AlignmentModes AlignmentMode { get{ return AlignmentModes.algPolar; } } /// The alignment mode of the mount (Alt/Az, Polar, German Polar).
        internal static void FindHome() { throw new MethodNotImplementedException("FindHome"); }
        internal static bool AtHome { get { return false; } } /// True if the telescope is stopped in the Home position. Set only following a <see cref="FindHome"></see> operation,
        internal static bool CanFindHome { get { return false; } }
        internal static void parkPos(out int ra, out int dec)
        {
            ra = (int)(((Int64)(SharedResources.raMaxPos)) * SharedResources.raAmplitude / 360 / 2);
            dec = SharedResources.decMaxPos / 2;
        }
        internal static void Park()
        {
            SharedResources.doLog("park", 0);
            SharedResources.TrackingDisabled = true;
            int ra, dec; parkPos(out ra, out dec);
            goToMotor(ra, dec);
        }
        public static void goToMotor(int ra, int dec)
        {
            SharedResources.SendSerialCommand(":Mg" + ra.ToString("X8") + dec.ToString("X8") + "#", 0);
            SharedResources._ScopeMoving = true;
        }
        internal static void Unpark() 
        {
            SharedResources.doLog("Unpark", 0);
            SharedResources.TrackingDisabled = false;
            if (SharedResources.Declinaison>89.9f) SharedResources.setToTrueNorth();
        }
        internal static void SetPark() { throw new MethodNotImplementedException("SetPark"); }
        internal static bool AtPark { get {
                if (!SharedResources.TrackingDisabled || !SharedResources.hasHWPos) return false;
                int ra, dec; parkPos(out ra, out dec);
                ra = Math.Abs(ra-SharedResources.raPos); dec = Math.Abs(dec-SharedResources.decPos);
                return ra+dec<400; 
            } } /// True if the telescope has been put into the parked state by the seee <see cref="Park" /> method
        internal static bool CanPark { get { return true; } }
        internal static bool CanUnpark { get { return true; } }
        internal static bool CanSetPark { get { return false; } }
        internal static IAxisRates AxisRates(TelescopeAxes Axis) { return new AxisRates(Axis); } /// Determine the rates at which the telescope may be moved about the specified axis by the <see cref="MoveAxis" /> method.
        internal static bool CanMoveAxis(TelescopeAxes Axis) /// True if this telescope can move the requested axis
        {
            switch (Axis)
            {
                case TelescopeAxes.axisPrimary: return true;
                case TelescopeAxes.axisSecondary: return true;
                case TelescopeAxes.axisTertiary: return false;
                default: throw new InvalidValueException("CanMoveAxis", Axis.ToString(), "0 to 2");
            }
        }
        internal static bool CanSetDeclinationRate { get { return false; } } /// True if the <see cref="DeclinationRate" /> property can be changed to provide offset tracking in the declination axis.
        internal static bool CanPulseGuide { get { SharedResources.doLog("CanPulseGuide", 0); return true; } } /// True if this telescope is capable of software-pulsed guiding (via the <see cref="PulseGuide" /> method)
        internal static bool CanSetGuideRates { get { SharedResources.doLog("CanSetGuideRates", 1); return true; } } /// True if the guide rate properties used for <see cref="PulseGuide" /> can ba adjusted.
        internal static bool CanSetPierSide { get { return true; } } /// True if the <see cref="SideOfPier" /> property can be set, meaning that the mount can be forced to flip.
        internal static bool CanSetRightAscensionRate { get { return false; } } /// True if the <see cref="RightAscensionRate" /> property can be changed to provide offset tracking in the right ascension axis.
        internal static bool CanSetTracking { get { SharedResources.doLog("CanSetTracking", 1); return true; } } /// True if the <see cref="Tracking" /> property can be changed, turning telescope sidereal tracking on and off.
        internal static bool CanSlew { get { return true; } } /// True if this telescope is capable of programmed slewing (synchronous or asynchronous) to equatorial coordinates
        internal static bool CanSlewAltAz { get { return false; } } /// True if this telescope is capable of programmed slewing (synchronous or asynchronous) to local horizontal coordinates
        internal static bool CanSlewAltAzAsync { get { return false; } } /// True if this telescope is capable of programmed asynchronous slewing to local horizontal coordinates
        internal static bool CanSlewAsync { get { return true; } } /// True if this telescope is capable of programmed asynchronous slewing to equatorial coordinates.
        internal static bool CanSync { get { return true; } } /// True if this telescope is capable of programmed synching to equatorial coordinates.
        internal static bool CanSyncAltAz { get { return false; } } /// True if this telescope is capable of programmed synching to local horizontal coordinates
        internal static double Declination { get { return SharedResources.Declinaison; } } /// The declination (degrees) of the telescope's current equatorial coordinates, in the coordinate system given by the <see cref="EquatorialSystem" /> property.
        internal static double DeclinationRate { get { return 0.0; } set { } } /// The declination tracking rate (arcseconds per SI second, default = 0.0)

        /// Predict side of pier for German equatorial mounts at the provided coordinates
        internal static PierSide DestinationSideOfPier(double RightAscension, double Declination)
        {
            // RA.pos are always growing... so if minPos is > 12, then maxPos is >24!
            // Easiest way to handle it is to see how far targetRa is from midpoint... >6h and we are not on the same side!
            int TODO; // change now with overshoot!
            int mid = SharedResources.midOfraRealPos;
            int targetRa= (int)(RightAscension*3600.0);
            if (Math.Abs(targetRa - mid) < 6 * 3600L) return SideOfPier;
            while (mid >= 24 * 3600L) mid -= 24 * 3600;
            while (mid < 0) mid += 24 * 3600;
            if (Math.Abs(targetRa - mid) <= 6 * 3600) return SideOfPier;
            return SideOfPier;
        }
        internal static bool DoesRefraction { get { return false; } set { } } /// True if the telescope or driver applies atmospheric refraction to coordinates.
        internal static EquatorialCoordinateType EquatorialSystem { get { return EquatorialCoordinateType.equJ2000; } } /// Equatorial coordinate system used by this telescope (e.g. Topocentric or J2000).
        /// The current Right Ascension movement rate offset for telescope guiding (degrees/sec) <summary>
        internal static double GuideRateDeclination // deg/s
        {
            get { SharedResources.doLog("get GuideRateDeclination "+ SharedResources.guideRateDecf().ToString(), 1); return SharedResources.guideRateDecf(); }
            set { SharedResources.doLog("set GuideRateDeclination "+GuideRateDeclination.ToString(), 1); }
        }
        /// The current Right Ascension movement rate offset for telescope guiding (degrees/sec)

        internal static double GuideRateRightAscension // deg/s
        {
            get { SharedResources.doLog("get GuideRateRa " + SharedResources.guideRateRaf().ToString(), 1); return SharedResources.guideRateRaf(); }
            set { return; }
        }
        /// Moves the scope in the given direction for the given interval or time at
        /// the rate given by the corresponding guide rate property
        /// <param name="Direction">The direction in which the guide-rate motion is to be made</param>
        /// <param name="Duration">The duration of the guide-rate motion (milliseconds)</param>
        internal static void PulseGuide(GuideDirections Direction, int Duration)
        {
            double rate; int steps; char d= ' ';
            double movement;
            if (Direction == GuideDirections.guideNorth || Direction == GuideDirections.guideSouth) 
            { 
                if ((SharedResources.guidingBits&32)!=0) return;
                rate= GuideRateDeclination; steps= SharedResources.raMaxPos;
                d = 'd';
                if (Direction == GuideDirections.guideNorth) steps= -steps;
                if ((SharedResources.guidingBits & 16) != 0) steps = -steps;
                if (((SharedResources.guidingBits & 8) != 0) && SideOfPier == PierSide.pierEast) steps = -steps;
                movement = rate * Duration * SharedResources.guideDecAgressivity / 1000 / 360; // in turns (0 to 1)...
            }
            else if (Direction == GuideDirections.guideEast || Direction == GuideDirections.guideWest) 
            { 
                if ((SharedResources.guidingBits & 8) != 0) return;
                rate= GuideRateRightAscension; steps= SharedResources.decMaxPos; 
                d = 'r';
                if (Direction == GuideDirections.guideEast) steps = -steps;
                if ((SharedResources.guidingBits & 2) != 0) steps = -steps;
                if (((SharedResources.guidingBits & 1) != 0) && SideOfPier==PierSide.pierEast) steps = -steps;
                SharedResources.raGuideIssued += steps;
                movement = rate * Duration * SharedResources.guideRaAgressivity / 1000 / 360; // in turns (0 to 1)...
            }
            else return;
            // rate in deg/s, duration in ms
            int speed= (int)(steps*rate/360); if (speed<0) speed= -speed;
            steps = (int)(steps*movement+0.5);

            SharedResources.doLog("Mount guide "+Direction.ToString()+" for "+Duration.ToString()+" is "+steps.ToString()+" steps at "+speed.ToString()+" steps/s", 3); 
            SharedResources.SendSerialCommand(":p"+d+ steps.ToString("X8") + speed.ToString("X8") + '#', 0);
            SharedResources.SetScopeGuiding();
        }
        /// True if a <see cref="PulseGuide" /> command is in progress, False otherwise
        internal static bool IsPulseGuiding { get { SharedResources.doLog("IsPulseGuiding "+SharedResources._ScopeGuiding.ToString(), 1); return SharedResources._ScopeGuiding; } }
        /// Move the telescope in one axis at the given rate.
        /// <param name="Axis">The physical axis about which movement is desired</param>
        /// <param name="Rate">The rate of motion (deg/sec) about the specified axis</param>
        internal static void MoveAxis(TelescopeAxes Axis, double Rate)
        {
            SharedResources.doLog("Mount move "+Axis.ToString()+" at "+Rate.ToString(), 0);
            if (Rate == 0) { SharedResources.SendSerialCommand(":Q#", 0); return; }
            if (Axis == TelescopeAxes.axisPrimary)
                SharedResources.SendSerialCommand(":Mr" + ((int)(Rate * 3600/15)).ToString("X8") + '#', 0); // in hour in mount!
            if (Axis == TelescopeAxes.axisSecondary)
                SharedResources.SendSerialCommand(":Md" + ((int)(Rate * 3600)).ToString("X8") + '#', 0); // in degree in mount
            SharedResources.SetScopeMoving();
        }

        /// The right ascension (hours) of the telescope's current equatorial coordinates,
        /// in the coordinate system given by the EquatorialSystem property
        internal static double RightAscension
        { get { return SharedResources.RightAssension; } }

        /// The right ascension tracking rate offset from sidereal (seconds per sidereal second, default = 0.0)
        internal static double RightAscensionRate
        {
            get { return 0.0; }
            set { throw new PropertyNotImplementedException("RightAscensionRate", true); }
        }

        /// Indicates the pointing state of the mount. Read the articles installed with the ASCOM Developer
        /// Components for more detailed information.
        internal static PierSide SideOfPier
        {
            get { return SharedResources.SideOfPier==0 ? PierSide.pierEast : PierSide.pierWest; }
            set { if (value != SideOfPier) SharedResources.SendSerialCommand(":$P#", 0); }
        }

        /// The local apparent sidereal time from the telescope's internal clock (hours, sidereal)
        internal static double SiderealTime
        {
            get
            {
                double siderealTime = 0.0; // Sidereal time return value
                // Use NOVAS 3.1 to calculate the sidereal time
                using (var novas = new NOVAS31())
                {
                    double julianDate = SharedResources.utilities.DateUTCToJulian(DateTime.UtcNow);
                    novas.SiderealTime(julianDate, 0, novas.DeltaT(julianDate), GstType.GreenwichApparentSiderealTime, Method.EquinoxBased, Accuracy.Full, ref siderealTime);
                }
                // Adjust the calculated sidereal time for longitude using the value returned by the SiteLongitude property, allowing for the possibility that this property has not yet been implemented
                // Reduce sidereal time to the range 0 to 24 hours
                return SharedResources.astroUtilities.ConditionRA(siderealTime+SiteLongitude / 360.0 * 24.0);
            }
        }

        internal static double Azimuth { get { return SharedResources.Azimuth; } } /// The azimuth at the local horizon of the telescope's current position (degrees, North-referenced, positive East/clockwise).
        internal static double Altitude { get { return SharedResources.Altitude; } } /// The Altitude above the local horizon of the telescope's current position (degrees, positive up)
        internal static double FocalLength { get { return SharedResources.FocalLength / 1000.0f; } set { SharedResources.FocalLength = (int)(value * 1000); } } /// The telescope's focal length, meters
        internal static double ApertureArea { get { return SharedResources.Area_cm2/10000.0f; } set { SharedResources.Area_cm2= (int)(value*10000); } } /// The area of the telescope's aperture, taking into account any obstructions (square meters)
        internal static double ApertureDiameter { get { return SharedResources.Diameter_mm/1000.0f; } set { SharedResources.Diameter_mm= (int)(value*1000); } } /// The telescope's effective aperture diameter (meters)
        internal static double SiteElevation { get { return SharedResources.SiteAltitude; } set { SharedResources.SiteAltitude = (int)value; } } /// The elevation above mean sea level (meters) of the site at which the telescope is located
        internal static double SiteLatitude { get { return SharedResources.Latitude/36000.0f;  } set { SharedResources.Latitude = (int)(value*36000); } } // /// The geodetic(map) latitude (degrees, positive North, WGS84) of the site at which the telescope is located.
        internal static double SiteLongitude { get { return SharedResources.Longitude/36000.0f;  } set { SharedResources.Longitude= (int)(value*36000); } } /// The longitude (degrees, positive East, WGS84) of the site at which the telescope is located.

        /// Specifies a post-slew settling time (sec.).
        internal static short SlewSettleTime
        {
            get { throw new PropertyNotImplementedException("SlewSettleTime", false); }
            set { throw new PropertyNotImplementedException("SlewSettleTime", true); }
        }

        /// Move the telescope to the given local horizontal coordinates, return when slew is complete
        internal static void SlewToAltAz(double Azimuth, double Altitude) { throw new MethodNotImplementedException("SlewToAltAz"); }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "VSTHRD200:Use \"Async\" suffix for async methods", Justification = "internal static method name used for many years.")]
        internal static void SlewToAltAzAsync(double Azimuth, double Altitude) { throw new MethodNotImplementedException("SlewToAltAzAsync"); }

        /// This Method must be implemented if <see cref="CanSlewAltAzAsync" /> returns True.
        /// It does not return to the caller until the slew is complete.
        internal static void SlewToCoordinates(double RightAscension, double Declination)
        {
            SlewToCoordinatesAsync(RightAscension, Declination);
            while (SharedResources.ScopeMoving) System.Threading.Thread.Sleep(200);
        }

        /// Move the telescope to the given equatorial coordinates, return with Slewing set to True immediately after starting the slew.
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "VSTHRD200:Use \"Async\" suffix for async methods", Justification = "internal static method name used for many years.")]
        internal static void SlewToCoordinatesAsync(double RightAscension, double Declination) { SharedResources.SlewToCoordinatesAsync(RightAscension, Declination, false); SharedResources.SetScopeMoving(); }

        /// Move the telescope to the <see cref="TargetRightAscension" /> and <see cref="TargetDeclination" /> coordinates, return when slew complete.
        internal static void SlewToTarget() { SlewToCoordinates(_TargetRightAscension, _TargetDeclination); }

        /// Move the telescope to the <see cref="TargetRightAscension" /> and <see cref="TargetDeclination" />  coordinates,
        /// returns immediately after starting the slew with Slewing set to True.
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "VSTHRD200:Use \"Async\" suffix for async methods", Justification = "internal static method name used for many years.")]
        internal static void SlewToTargetAsync() { SlewToCoordinatesAsync(_TargetRightAscension, _TargetDeclination); }
        internal static bool Slewing { get { return SharedResources.ScopeMoving; } } /// True if telescope is in the process of moving in response to one of the
        internal static void SyncToAltAz(double Azimuth, double Altitude) { throw new MethodNotImplementedException("SyncToAltAz"); } /// Matches the scope's local horizontal coordinates to the given local horizontal coordinates.
        /// Matches the scope's equatorial coordinates to the given equatorial coordinates.
        internal static void SyncToCoordinates(double RightAscension, double Declination) { SharedResources.SlewToCoordinatesAsync(RightAscension, Declination, true); }
        /// Matches the scope's equatorial coordinates to the target equatorial coordinates.
        internal static void SyncToTarget() { SyncToCoordinates(_TargetRightAscension, _TargetDeclination); }

        /// The declination (degrees, positive North) for the target of an equatorial slew or sync operation
        static double _TargetDeclination = 0.0;
        internal static double TargetDeclination { get { return _TargetDeclination; } set { _TargetDeclination= value; } }
        /// The right ascension (hours) for the target of an equatorial slew or sync operation
        static double _TargetRightAscension = 0.0;
        internal static double TargetRightAscension { get { return _TargetRightAscension; } set { _TargetRightAscension = value; } }

        // The state of the telescope's sidereal tracking drive.
        internal static bool Tracking { get { return !SharedResources.TrackingDisabled; } 
            set { SharedResources.doLog("set Tracking "+value.ToString(), 0); SharedResources.TrackingDisabled= !value; } }

        /// The current tracking rate of the telescope's sidereal drive
        internal static DriveRates TrackingRate
        {
            get {   if (SharedResources.ascomtrackspd==0) return DriveRates.driveSidereal;
                    if (SharedResources.ascomtrackspd==1) return DriveRates.driveLunar;
                    if (SharedResources.ascomtrackspd==2) return DriveRates.driveSolar;
                    if (SharedResources.ascomtrackspd==3) return DriveRates.driveKing;
                    return DriveRates.driveSidereal; // pb here!!!
                }
            set { SharedResources.doLog("set TrackingRate "+value.ToString(), 0); 
                    if (value==DriveRates.driveSidereal) SharedResources.Track(Tracking, 0);  
                    else if (value==DriveRates.driveLunar) SharedResources.Track(Tracking, 1);  
                    else if (value==DriveRates.driveSolar) SharedResources.Track(Tracking, 2);  
                    else if (value==DriveRates.driveKing) SharedResources.Track(Tracking, 3);  
                    else throw new InvalidValueException("TrackingRate", value.ToString(), "0 to 2");
                }
        }

        /// Returns a collection of supported <see cref="DriveRates" /> values that describe the permissible
        public class MyTrackingRates : ITrackingRates
            {
                private readonly List<DriveRates> supportedRates = new List<DriveRates>();
                public MyTrackingRates()
                {
                    supportedRates.Add(DriveRates.driveSidereal);
                    supportedRates.Add(DriveRates.driveLunar);
                    supportedRates.Add(DriveRates.driveSolar);
                    supportedRates.Add(DriveRates.driveKing);
                }
                public int Count => supportedRates.Count;
                public IEnumerator GetEnumerator() => supportedRates.GetEnumerator();
                public DriveRates this[int index] => supportedRates[index];
                public void Dispose() { }
            }
        internal static ITrackingRates TrackingRates { get { return new MyTrackingRates(); } }

        /// The UTC date/time of the telescope's internal clock
        internal static DateTime UTCDate
        {
            get { return DateTime.UtcNow; }
            set { throw new PropertyNotImplementedException("UTCDate", true); }
        }
        private static bool IsConnected { get { return SharedResources.Connected; } }
    }
}