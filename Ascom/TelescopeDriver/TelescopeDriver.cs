using ASCOM;
using ASCOM.Astrometry;
using ASCOM.Astrometry.AstroUtils;
using ASCOM.Astrometry.NOVAS;
using ASCOM.BrebissonV1.Focuser;
using ASCOM.DeviceInterface;
using ASCOM.LocalServer;
using ASCOM.Utilities;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace ASCOM.BrebissonV1.Telescope
{
    // This code is mostly a presentation layer for the functionality in the TelescopeHardware class. You should not need to change the contents of this file very much, if at all.
    // Most customisation will be in the TelescopeHardware class, which is shared by all instances of the driver, and which must handle all aspects of communicating with your device.
    [ComVisible(true)]
    [Guid("9cfbd404-ad34-45da-839b-c1c216e1c93b")]
    [ProgId("ASCOM.BrebissonV1.Telescope")]
    [ServedClassName("ASCOM Telescope Driver for BrebissonV1")] // Driver description that appears in the Chooser, customise as required
    [ClassInterface(ClassInterfaceType.None)]
    public class Telescope : ReferenceCountedObjectBase, ITelescopeV3, IDisposable
    {
        internal static string DriverProgId; // ASCOM DeviceID (COM ProgID) for this driver, the value is retrieved from the ServedClassName attribute in the class initialiser.
        internal static string DriverDescription; // The value is retrieved from the ServedClassName attribute in the class initialiser.

        // connectedState holds the connection state from this driver instance's perspective, as opposed to the local server's perspective, which may be different because of other client connections.
        internal bool connectedState= false; // The connected state from this driver's perspective)

        /// Initializes a new instance of the <see cref="BrebissonV1"/> class. Must be public to successfully register for COM.
        public Telescope()
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
                MessageBox.Show($"{ex.Message}", "Exception creating ASCOM.BrebissonV1.Telescope", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        ~Telescope(){ }
        public void Dispose() { }
        public void SetupDialog() { TelescopeHardware.SetupDialog(); }
        public ArrayList SupportedActions { get { return new ArrayList(); } }
        public string Action(string actionName, string actionParameters) { return ""; }
        public void CommandBlind(string command, bool raw) { }
        public bool CommandBool(string command, bool raw) { return false; }
        public string CommandString(string command, bool raw) { return ""; }
        public bool Connected
        {
            get {  return connectedState; }
            set { SharedResources.doLog("set connected "+value.ToString(), 0); if (connectedState == value) return; TelescopeHardware.Connected = value; if (!value) connectedState = false; else connectedState = TelescopeHardware.Connected; }
        }

        public string Description { get { SharedResources.doLog("get Description "+TelescopeHardware.Description, 1); return TelescopeHardware.Description; } }
        public string DriverInfo { get { SharedResources.doLog("get DriverInfo", 1); return TelescopeHardware.DriverInfo; } }
        public string DriverVersion { get { SharedResources.doLog("get DriverVersion", 1); return TelescopeHardware.DriverVersion; } }
        public short InterfaceVersion { get { SharedResources.doLog("get InterfaceVersion", 1); return TelescopeHardware.InterfaceVersion; } }
        public string Name { get { SharedResources.doLog("get Name", 1); return TelescopeHardware.Name; } }
        public void AbortSlew() { TelescopeHardware.AbortSlew(); }
        public AlignmentModes AlignmentMode { get { SharedResources.doLog("get AlignmentMode", 1); return TelescopeHardware.AlignmentMode; } }
        public double Altitude { get { SharedResources.doLog("get Altitude", 1); return TelescopeHardware.Altitude; } }
        public double ApertureArea { get { SharedResources.doLog("get ApertureArea", 1); return TelescopeHardware.ApertureArea; } set { TelescopeHardware.ApertureArea = value; } }
        public double ApertureDiameter { get { SharedResources.doLog("get ApertureDiameter", 1); return TelescopeHardware.ApertureDiameter; } set { TelescopeHardware.ApertureDiameter = value; } }
        public bool AtHome { get { SharedResources.doLog("get AtHome", 1); return TelescopeHardware.AtHome; } }
        public bool AtPark { get { SharedResources.doLog("get AtPark", 1); return TelescopeHardware.AtPark; } }
        public IAxisRates AxisRates(TelescopeAxes axis) { return TelescopeHardware.AxisRates(axis); }
        public double Azimuth { get { SharedResources.doLog("get Azimuth", 1); return TelescopeHardware.Azimuth; } }
        public bool CanFindHome { get { SharedResources.doLog("get CanFindHome", 1); return TelescopeHardware.CanFindHome; } }
        public bool CanMoveAxis(TelescopeAxes axis) { SharedResources.doLog("get CanMoveAxis", 1); return TelescopeHardware.CanMoveAxis(axis); }
        public bool CanPark { get { SharedResources.doLog("get CanPark", 1); return TelescopeHardware.CanPark; } }
        public bool CanPulseGuide { get { SharedResources.doLog("get CanPulseGuide", 1); return TelescopeHardware.CanPulseGuide; } }
        public bool CanSetDeclinationRate { get { SharedResources.doLog("get CanSetDeclinationRate", 1); return TelescopeHardware.CanSetDeclinationRate; } }
        public bool CanSetGuideRates { get { SharedResources.doLog("get CanSetGuideRates", 1); return TelescopeHardware.CanSetGuideRates; } }
        public bool CanSetPark { get { SharedResources.doLog("get CanSetPark", 1); return TelescopeHardware.CanSetPark; } }
        public bool CanSetPierSide { get { SharedResources.doLog("get CanSetPierSide", 1); return TelescopeHardware.CanSetPierSide; } }
        public bool CanSetRightAscensionRate { get { SharedResources.doLog("get CanSetRightAscensionRate", 1); return TelescopeHardware.CanSetRightAscensionRate; } }
        public bool CanSetTracking { get { SharedResources.doLog("get CanSetTracking", 1); return TelescopeHardware.CanSetTracking; } }
        public bool CanSlew { get { SharedResources.doLog("get CanSlew", 1); return TelescopeHardware.CanSlew; } }
        public bool CanSlewAltAz { get { SharedResources.doLog("get CanSlewAltAz", 1); return TelescopeHardware.CanSlewAltAz; } }
        public bool CanSlewAltAzAsync { get { SharedResources.doLog("get CanSlewAltAzAsync", 1); return TelescopeHardware.CanSlewAltAzAsync; } }
        public bool CanSlewAsync { get { SharedResources.doLog("get CanSlewAsync", 1); return TelescopeHardware.CanSlewAsync; } }
        public bool CanSync { get { SharedResources.doLog("get CanSync", 1); return TelescopeHardware.CanSync; } }
        public bool CanSyncAltAz { get { SharedResources.doLog("get CanSyncAltAz", 1); return TelescopeHardware.CanSyncAltAz; } }
        public bool CanUnpark { get { SharedResources.doLog("get CanUnpark", 1); return TelescopeHardware.CanUnpark; } }
        public double Declination { get { SharedResources.doLog("get Declination", 1); return TelescopeHardware.Declination; } }
        public double DeclinationRate { get { SharedResources.doLog("get DeclinationRate", 1); return TelescopeHardware.DeclinationRate; } set { TelescopeHardware.DeclinationRate = value; } }
        public PierSide DestinationSideOfPier(double rightAscension, double declination) { SharedResources.doLog("DestinationSideOfPier", 1); return TelescopeHardware.DestinationSideOfPier(rightAscension, declination); }
        public bool DoesRefraction { get { SharedResources.doLog("get DoesRefraction", 1); return TelescopeHardware.DoesRefraction; } set { TelescopeHardware.DoesRefraction = value; } }
        public EquatorialCoordinateType EquatorialSystem { get { SharedResources.doLog("get EquatorialSystem", 1); return TelescopeHardware.EquatorialSystem; } }
        public void FindHome() { SharedResources.doLog("FindHome", 0); TelescopeHardware.FindHome(); }
        public double FocalLength { get { SharedResources.doLog("get FocalLength", 1); return TelescopeHardware.FocalLength; } set { TelescopeHardware.FocalLength = value; }  }
        public double GuideRateDeclination { get { SharedResources.doLog("get GuideRateDeclination", 1); return TelescopeHardware.GuideRateDeclination; } set { TelescopeHardware.GuideRateDeclination = value; } }
        public double GuideRateRightAscension { get { SharedResources.doLog("get GuideRateRightAscension", 1); return TelescopeHardware.GuideRateRightAscension; } set { TelescopeHardware.GuideRateRightAscension = value; } }
        //public double GuideRateDeclinationBacklash { get { return TelescopeHardware.GuideRateDeclinationBacklash; } set { TelescopeHardware.GuideRateDeclinationBacklash= value; } }
        public bool IsPulseGuiding { get { SharedResources.doLog("get IsPulseGuiding", 1); return TelescopeHardware.IsPulseGuiding; } }
        public void MoveAxis(TelescopeAxes axis, double rate) { SharedResources.doLog("MoveAxis", 0); TelescopeHardware.MoveAxis(axis, rate); }
        public void Park() { SharedResources.doLog("Park", 0); TelescopeHardware.Park(); }
        public void PulseGuide(GuideDirections direction, int duration) { SharedResources.doLog("PulseGuide", 2); TelescopeHardware.PulseGuide(direction, duration); }
        public double RightAscension { get { SharedResources.doLog("get RightAscension", 1);  return TelescopeHardware.RightAscension; } }
        public double RightAscensionRate { get { SharedResources.doLog("get RightAscensionRate", 1); return TelescopeHardware.RightAscensionRate; } set { TelescopeHardware.RightAscensionRate = value; } }
        public void SetPark() { SharedResources.doLog("SetPark", 0); TelescopeHardware.SetPark(); }
        public PierSide SideOfPier { get { SharedResources.doLog("get SideOfPier", 1); return  TelescopeHardware.SideOfPier; } set { TelescopeHardware.SideOfPier = value; } }
        public double SiderealTime { get { SharedResources.doLog("get SiderealTime", 1); return TelescopeHardware.SiderealTime; } }
        public double SiteElevation { get { SharedResources.doLog("get SiteElevation", 1); return TelescopeHardware.SiteElevation; } set { TelescopeHardware.SiteElevation = value; } }
        public double SiteLatitude { get { SharedResources.doLog("get SiteLatitude", 1); return TelescopeHardware.SiteLatitude; } set { TelescopeHardware.SiteLatitude = value; } }
        public double SiteLongitude { get { SharedResources.doLog("get SiteLongitude", 1); return TelescopeHardware.SiteLongitude; } set { TelescopeHardware.SiteLongitude = value; } }
        public short SlewSettleTime { get { SharedResources.doLog("get SlewSettleTime", 1); return TelescopeHardware.SlewSettleTime; } set { TelescopeHardware.SlewSettleTime = value; } }
        public void SlewToAltAz(double azimuth, double altitude) { SharedResources.doLog("SlewToAltAz", 0); TelescopeHardware.SlewToAltAz(azimuth, altitude); }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "VSTHRD200:Use \"Async\" suffix for async methods", Justification = "Public method name used for many years.")]
        public void SlewToAltAzAsync(double azimuth, double altitude) { SharedResources.doLog("SlewToAltAzAsync", 0); TelescopeHardware.SlewToAltAzAsync(azimuth, altitude);}
        public void SlewToCoordinates(double rightAscension, double declination) { TelescopeHardware.SlewToCoordinates(rightAscension, declination); }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "VSTHRD200:Use \"Async\" suffix for async methods", Justification = "Public method name used for many years.")]
        public void SlewToCoordinatesAsync(double rightAscension, double declination) { TelescopeHardware.SlewToCoordinatesAsync(rightAscension, declination); }
        public void SlewToTarget() { SharedResources.doLog("SlewToTarget", 0); TelescopeHardware.SlewToTarget(); }
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Style", "VSTHRD200:Use \"Async\" suffix for async methods", Justification = "Public method name used for many years.")]
        public void SlewToTargetAsync() { SharedResources.doLog("SlewToTargetAsync", 0); TelescopeHardware.SlewToTargetAsync(); }
        public bool Slewing { get { /*SharedResources.doLog("get Slewing "+TelescopeHardware.Slewing.ToString()); */ return TelescopeHardware.Slewing; } }
        public void SyncToAltAz(double azimuth, double altitude) { SharedResources.doLog("SyncToAltAz", 0); TelescopeHardware.SyncToAltAz(azimuth, altitude); }
        public void SyncToCoordinates(double rightAscension, double declination) { SharedResources.doLog("SyncToCoordinates", 0); TelescopeHardware.SyncToCoordinates(rightAscension, declination); }
        public void SyncToTarget() { SharedResources.doLog("SyncToTarget", 0); TelescopeHardware.SyncToTarget(); }
        public double TargetDeclination { get { SharedResources.doLog("get TargetDeclination", 1); return TelescopeHardware.TargetDeclination; } set { TelescopeHardware.TargetDeclination = value; } }
        public double TargetRightAscension { get { SharedResources.doLog("get TargetRightAscension", 1); return TelescopeHardware.TargetRightAscension; } set { TelescopeHardware.TargetRightAscension = value; } }
        public bool Tracking { get { SharedResources.doLog("get Tracking", 1);  return TelescopeHardware.Tracking; } set { TelescopeHardware.Tracking = value; } }
        public DriveRates TrackingRate { get { SharedResources.doLog("get TrackingRate", 1); return TelescopeHardware.TrackingRate; } set { TelescopeHardware.TrackingRate = value; } }
        public ITrackingRates TrackingRates { get{ SharedResources.doLog("get TrackingRates", 0); return TelescopeHardware.TrackingRates; } }
        public DateTime UTCDate { get { SharedResources.doLog("get UTCDate", 1); return TelescopeHardware.UTCDate; } set { TelescopeHardware.UTCDate = value; } }
        public void Unpark() { SharedResources.doLog("Unpark", 0); TelescopeHardware.Unpark(); }
    }
}
