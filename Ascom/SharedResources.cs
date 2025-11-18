using ASCOM.Utilities;
using System;
using System.Linq;
using ASCOM.Astrometry.Transform;
using ASCOM.Astrometry.AstroUtils;
using System.Diagnostics;
using System.Threading;
using System.Runtime.InteropServices;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.InteropServices.ComTypes;
using ASCOM.BrebissonV1.Telescope;

namespace ASCOM.LocalServer
{
    [HardwareClass]
    public static class SharedResources
    {
        private static readonly object lockObject = new object(); // Object used for locking to prevent multiple drivers accessing common code at the same time

        public interface ILog { void log(string message, int source); };
        public static ILog log= null;
        public static void doLog(string msg, int source) 
        { 
            if (log==null) return; log.log(msg, source);
        }
        // Shared serial port. This will allow multiple drivers to use one single serial port.
        private static Serial SharedSerial = new Serial(); // Shared serial port
        public static string _comPort= "COM1";
        internal static Util utilities = new Util(); // ASCOM Utilities object for use as required
        internal static AstroUtils astroUtilities = new AstroUtils(); // ASCOM AstroUtilities object for use as required
        public static void Dispose()
        {
            Disconnect();
            try { if (SharedSerial != null) SharedSerial.Dispose(); } catch { }
        }
        public static int FocusserPosition
        {
            get { return _FocusserPosition; }
            //set { _FocusserPosition = value; doLog("Foc sync to "+value.ToString()); }
        }
        public static double Declinaison
        {
            get { return _Declinaison; }
        }
        public static double RightAssension
        {
            get { return _RightAssension; }
        }

        public static Transform azimutal = new Transform();
        public static double Altitude { get { azimutal.JulianDateUTC= utilities.DateUTCToJulian(DateTime.UtcNow); return azimutal.ElevationTopocentric;  } }
        public static double Azimuth { get { azimutal.JulianDateUTC = utilities.DateUTCToJulian(DateTime.UtcNow); return azimutal.AzimuthTopocentric; } }
        public static bool FocusMoving { get { return _FocusMoving; } }
        public static bool ScopeMoving { get { if (_ScopeGuiding) return false; return _ScopeMoving; } }
        public static int SideOfPier { get { return _sideOfPier; } }
        public static bool FlipDisabled { get { return _flipDisabled; } set { SendSerialCommand(":$f"+(value?"0":"1")+"#", 0); } }

        static bool ascomtrack= true;
        public static int ascomtrackspd= 0; // 0: sideral, 1: moon, 2: sun, 3: king, 4: unknown
        static void sendTrackSpeed()
        { 
            int stt= 23*3600+56*60+4; if (ascomtrackspd==1) stt= 24*3600+50*60; if (ascomtrackspd==2) stt= 24*3600; if (ascomtrackspd==3) stt= 1299188;
            if (ascomtrack) SendSerialCommand(":$T"+stt.ToString("X6")+"#", 0);
            else SendSerialCommand(":$T000000#", 0);
        }
        public static int TrackingRate { 
            get { return ascomtrackspd; } 
            set { if (value<0 || value>3) return; ascomtrackspd= value; sendTrackSpeed(); } 
            }
        public static bool TrackingDisabled { get { return _trackingDisabled; } set { ascomtrack= _trackingDisabled= value; sendTrackSpeed(); } }
        public static void Track(bool running, int rate) { _trackingDisabled= ascomtrack= running; TrackingRate= rate; }
        public static bool meridianFlip { get { return _meridianFlip; } }
        public static long timeSpanPC { get { return _spanPC; } }
        public static long timeSpanHW { get { return _spanHW; } }
        public static long timeSpanUncountedSteps { get { return _uncountedSteps; } }


        [DllImport("kernel32.dll")]
        static extern uint SetThreadExecutionState(uint esFlags);

        // Flags
        const uint ES_CONTINUOUS        = 0x80000000;
        const uint ES_SYSTEM_REQUIRED   = 0x00000001;
        const uint ES_DISPLAY_REQUIRED  = 0x00000002;

        private static System.Timers.Timer timerPos;
        private static bool connectionLive = false;
        public struct Ttimes { public long pctime, HWtime, uncountedSteps; };
        private static Ttimes[] times;
        public static Ttimes[] savedTimes; public static int usedSavedTimes= 0;
        public static void clearSavedTimes() { usedSavedTimes= 0; }
        public static bool saveTimes= false;
        private static int timesPos= 0;
        private static long startpcTime;
        public static double guideRaAgressivity= 1.0, guideDecAgressivity = 1.0;
        public static void SetScopeMoving() { _ScopeMoving= true; } // used when you tell the scope to move to set the var to move until we get more info from the scope itself...
        public static void SetScopeGuiding() { _ScopeGuiding= true; } // used when you tell the scope to move to set the var to move until we get more info from the scope itself...
        public static bool _ScopeMoving = false, _ScopeGuiding= false;
        private static long _spanPC = 0;
        private static long _spanHW = 0;
        private static long _uncountedSteps= 0;
        private static bool _FocusMoving = false;
        private static int _sideOfPier = 0;
        private static bool _meridianFlip = false;
        private static bool _flipDisabled= false;
        private static bool _trackingDisabled= false;
        private static double _Declinaison = 0;
        private static double _RightAssension = 0;
        public static int _FocusserPosition = 0;
        public static bool hasHWData = false, hasHWPos= false;
        public static bool dataDisplayed = false;
        public static int raMaxPos=0, raMaxSpeed=0, ramsToSpeed=0, decMaxPos=0, decMaxSpeed=0, decmsToSpeed=0, timeComp=0;
        public static int Latitude=0, Longitude=0, SiteAltitude=0, FocalLength=0, Diameter_mm=0, Area_cm2=0, FocStepdum=0;
        public static int focMaxStp= 0, focMaxSpd= 0, focAcc= 0;
        public static int decBacklash = 0, raAmplitude = 0, guideRateRA=0, guideRateDec=0;
        public static int raBacklash = 0, raSettle = 0, focBacklash = 0;
        public static int raPos= 0, decPos= 0;
        private static long lastPCTime = 0, lastuncountedSteps = 0;
        public static int raGuideIssued = 0;
        public static int invertAxes = 0, guidingBits=0;
        public static bool hasPowerCount= false;
        public static bool powerBit= false;
        public static int powerCount= 0;
        public static bool guideAfterSlew = false, yellOnPower= false, focusInmm= false;
        public static double guideRateDecf() { return (360f * guideRateDec) / decMaxPos; }
        public static double guideRateRaf() { return (360f * guideRateRA) / raMaxPos; }

        public static int midOfraRealPos = 6 * 3600; // stores the mid point of the RA axis in real coordinates. Used to check if somehting will need a meridial flip...
        public static void Disconnect() // force disconnect. setting connected to false will NOT disconnect as multiple clients might be asking for a disconnection
        {
            lock (lockObject)
            {
                if (timerPos != null) { timerPos.Dispose(); timerPos = null; }
                Lock= 0;
                connectionLive = false; hasHWPos= false; hasHWData = false; dataDisplayed= false; hasPowerCount= false;
                raMaxPos = 0; raMaxSpeed = 0; ramsToSpeed = 0; decMaxPos = 0; decMaxSpeed = 0; decmsToSpeed = 0;
                if (SharedSerial!=null) SharedSerial.Connected = false;
                tcpdisconnect();
                doLog("Disconnect", -1);
                SetThreadExecutionState(ES_CONTINUOUS);
            }
        }
        public static string comPort 
        {
            get { return _comPort;  }
            set
            {
                if (value == _comPort) return;
                _comPort = value;
                if (SharedSerial == null) return;
                bool conn = connectionLive;
                Disconnect();
                if (conn) Connected = true;
                doLog("Set com to "+value, -1);
            }
        }
        static private int Lock= 0;
        static public string hwconfstring= "";
        static public string latestResponse1= "", latestResponse2= "";
        static public int responceCount= 0;
        static public bool haswifi= false;
        static public string wifi= "", wifip= "";
        static public uint ipaddr= 0;
        static public void readHWString()
        {
            int i = 0; string v= hwconfstring;
            if (v.Length != 154 && v.Length != 154+32*4+8) return;
            haswifi= v.Length == 154+32*4+8;
            dataDisplayed= false;
            raMaxPos = readHex2(v, ref i, i+8); raMaxSpeed = readHex2(v, ref i, i + 8); ramsToSpeed = readHex2(v, ref i, i + 8);
            decMaxPos = readHex2(v, ref i, i + 8); decMaxSpeed = readHex2(v, ref i, i + 8); decmsToSpeed = readHex2(v, ref i, i + 8);
            timeComp= readHex2(v, ref i, i + 8);

            Latitude = readHex2(v, ref i, i + 8); Longitude = readHex2(v, ref i, i + 8); SiteAltitude = readHex2(v, ref i, i + 4);
            FocalLength= readHex2(v, ref i, i + 4); Diameter_mm= readHex2(v, ref i, i + 4); Area_cm2= readHex2(v, ref i, i + 4); FocStepdum= readHex2(v, ref i, i + 4);
            focMaxStp = readHex2(v, ref i, i + 4); focMaxSpd = readHex2(v, ref i, i + 4); focAcc = readHex2(v, ref i, i + 4);
            decBacklash = readHex2(v, ref i, i + 4); raAmplitude = readHex2(v, ref i, i + 4);
            guideRateRA = readHex2(v, ref i, i + 2); guideRateDec =  readHex2(v, ref i, i + 2);
            invertAxes = readHex2(v, ref i, i + 2);
            guidingBits =  readHex2(v, ref i, i + 2);
            raBacklash = readHex2(v, ref i, i + 4);
            focBacklash = readHex2(v, ref i, i + 4);
            raSettle = readHex2(v, ref i, i + 2);

            azimutal.SiteLatitude = Latitude / 36000.0f;
            azimutal.SiteLongitude = Longitude / 36000.0f;
            azimutal.SiteElevation = SiteAltitude;

            
            wifi= ""; wifip= ""; ipaddr= 0;
            if (haswifi)
            { 
                i= 154; for (int j=0; j<32; j++) { int c= readHex2(v, ref i, i+2); if (c==0) break; wifi+= (char)c; }
                i= 154+32*2; for (int j=0; j<32; j++) { int c= readHex2(v, ref i, i+2); if (c==0) break; wifip+= (char)c; }
                i= 154+32*4; ipaddr= (uint)readHex2(v, ref i, i + 8);
            }

            // Assumes crc is correct and ignore extra for the moment...!!!
            hasHWData = true;
        }
        public static DateTime lastHeartBeat;

        public static bool Connected
        {
            set
            {
                lock (lockObject)
                {
                    doLog("Connect", -1);
                    if (!value) return; // We actually do NOT disconnect when asked by a client... just when asked by the main app!

                    if (SharedSerial.Connected || tcpstream!=null) return; // already connected...
                    if (comPort!="tcp")
                    { 
                        SharedSerial.PortName = comPort;
                        SharedSerial.Speed = ASCOM.Utilities.SerialSpeed.ps38400;
                        SharedSerial.Connected = true;
                        SharedSerial.ReceiveTimeout = 1;
                    } else tcpconnect();
                    responceCount= 0;
                    times= new Ttimes[120]; timesPos = 0;
                    startpcTime = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
                    timerPos = new System.Timers.Timer(500);
                    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
                    timerPos.Elapsed += (source, e) =>
                    {  // update status every 1/2 second
                        if (Interlocked.Exchange(ref Lock, 1)==0)
                        { 
                            try
                            {
                                string v = SendSerialCommand("!");
                                if (v.Length<38) { Lock= 0; return; }
                                latestResponse1= v; latestResponse2= ""; responceCount++;
                                int i= 0; double t;
                                lastHeartBeat= DateTime.UtcNow;
                                int dec= readHex(v, ref i, 6); if (dec>=0x800000) dec|= unchecked((int)0xff000000);
                                latestResponse2= "dec:"+dec.ToString();
                                t=  dec/ 3600.0;
                                if (t!=_Declinaison) { _Declinaison = t; try { azimutal.SetTopocentric(_RightAssension, _Declinaison); } catch (Exception) { } }
                                t = readHex(v, ref i, i+6);
                                latestResponse2+= " ra:"+t.ToString();
                                t= t/ 3600.0;
                                if (t!=_RightAssension) { _RightAssension= t; try { azimutal.SetTopocentric(_RightAssension, _Declinaison); } catch (Exception) { } }
                                _FocusserPosition = readHex(v, ref i, i+6);
                                latestResponse2+= " foc:"+_FocusserPosition.ToString();
                                int bits= readHex(v, ref i, i+2);
                                latestResponse2+= " bits:"+bits.ToString("X");
                                bool old_ScopeMoving = _ScopeMoving;
                                _FocusMoving = (bits & 2) != 0;
                                _ScopeGuiding= (bits & 128)!=0;
                                if (_ScopeGuiding) _ScopeMoving= false; else _ScopeMoving = (bits & 1) != 0;
                                if (old_ScopeMoving && !_ScopeMoving && guideAfterSlew)
                                    phd2reguide();
                                _sideOfPier = ((bits & 4) != 0)?1:0; // 0 is east
                                _meridianFlip= (bits & 8) != 0;
                                _flipDisabled= (bits & 16) != 0;
                                _trackingDisabled= (bits & 32) != 0;
                                powerBit= (bits & 64) != 0;
                                long timems = readHex(v, ref i, i + 6);
                                latestResponse2+= " timems:"+timems.ToString();
                                midOfraRealPos = readHex(v, ref i, i + 6);
                                latestResponse2+= " midOfra:"+midOfraRealPos.ToString();
                                long uncountedSteps= readHex(v, ref i, i+6);
                                latestResponse2+= " uncountedSteps:"+uncountedSteps.ToString();
                                //Console.WriteLine("Uncounted Steps "+uncountedSteps.ToString());
                                times[timesPos].pctime = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond - startpcTime;
                                times[timesPos].HWtime = timems;
                                times[timesPos].uncountedSteps = uncountedSteps;
                                doLog(times[timesPos].pctime.ToString() + "," + (times[timesPos].pctime - lastPCTime).ToString() + "," + 
                                      (times[timesPos].uncountedSteps-lastuncountedSteps).ToString() + "," + uncountedSteps.ToString() + (raGuideIssued == 0 ? "" : ("," + raGuideIssued.ToString())), 2);
                                raGuideIssued = 0;
                                lastPCTime= times[timesPos].pctime;
                                lastuncountedSteps= times[timesPos].uncountedSteps;

                                if (v.Length>=54)
                                {
                                    hasHWPos= true;
                                    raPos = readHex(v, ref i, i + 8);
                                    decPos = readHex(v, ref i, i + 8);
                                    latestResponse2+= " raPos:"+raPos.ToString();
                                    latestResponse2+= " decPos:"+decPos.ToString();
                                }
                                if (v.Length>=56)
                                {
                                    hasPowerCount= true;
                                    int tmp = readHex(v, ref i, i + 2);
                                    powerCount= tmp&0x1f;
                                    tmp>>= 5;
                                    if (tmp==0) ascomtrack= false;
                                    else { ascomtrack= true; ascomtrackspd= tmp-1; }
                                    latestResponse2+= " powerCount:"+powerCount.ToString();
                                }



                                if (saveTimes)
                                {
                                    if (savedTimes==null || savedTimes.Length>=usedSavedTimes)
                                        Array.Resize(ref savedTimes, usedSavedTimes+128*1024);
                                    savedTimes[usedSavedTimes++]= times[timesPos];
                                }
                                if (timesPos != 119) timesPos++;
                                else
                                {
                                    timesPos = 0;
                                    _spanPC= times[119].pctime - times[0].pctime;
                                    _spanHW= times[119].HWtime - times[0].HWtime;
                                    _uncountedSteps= times[119].uncountedSteps - times[0].uncountedSteps;
                                }
                                connectionLive = true;
                                if (!hasHWData)
                                {
                                    v = SendSerialCommand("&");
                                    if (v.Length != 154 && v.Length != 154+32*4+8) { Lock= 0; return; }// invalid...
                                    hwconfstring= v;
                                    readHWString();
                                    
                                    if (_Declinaison>89.9f) setToTrueNorth();
                                }
                            }
                            catch (NotConnectedException)
                            {
                                SharedSerial.Connected= false;
                                tcpdisconnect();
                                timerPos.Dispose();
                                timerPos= null;
                                connectionLive= false;
                                Lock= 0;
                            }
                            Lock= 0;
                        }
                    };
                    timerPos.Enabled = true;
                }
            }
            get { return connectionLive; }
        }
        static void addstr(ref int crc, ref string s, int v, int nb)
        {
            while (--nb >= 0)
            {
                crc += v;
                s += ((byte)v).ToString("X2");
                v >>= 8;
            }
        }
        public static void updateHW()
        {
            string s = "@"; 
            int crc = 0;
            addstr(ref crc, ref s, raMaxPos, 4);
            addstr(ref crc, ref s, raMaxSpeed, 4);
            addstr(ref crc, ref s, ramsToSpeed, 4);
            addstr(ref crc, ref s, decMaxPos, 4);
            addstr(ref crc, ref s, decMaxSpeed, 4);
            addstr(ref crc, ref s, decmsToSpeed, 4);
            addstr(ref crc, ref s, timeComp, 4);
            addstr(ref crc, ref s, Latitude, 4);
            addstr(ref crc, ref s, Longitude, 4);
            addstr(ref crc, ref s, SiteAltitude, 2);
            addstr(ref crc, ref s, FocalLength, 2);
            addstr(ref crc, ref s, Diameter_mm, 2);
            addstr(ref crc, ref s, Area_cm2, 2);
            addstr(ref crc, ref s, FocStepdum, 2);

            addstr(ref crc, ref s, focMaxStp, 2);
            addstr(ref crc, ref s, focMaxSpd, 2);
            addstr(ref crc, ref s, focAcc, 2);
            addstr(ref crc, ref s, decBacklash, 2);
            addstr(ref crc, ref s, raAmplitude, 2);
            addstr(ref crc, ref s, guideRateRA, 1);
            addstr(ref crc, ref s, guideRateDec, 1);
            addstr(ref crc, ref s, invertAxes, 1);
            addstr(ref crc, ref s, guidingBits, 1);
            addstr(ref crc, ref s, raBacklash, 2);
            addstr(ref crc, ref s, focBacklash, 2);
            addstr(ref crc, ref s, raSettle, 1);
            for (int i=0; i<11; i++) addstr(ref crc, ref s, 0, 1);
            s += ((byte)crc).ToString("X2");
            if (haswifi)
            {
                for (int i=0; i<32; i++) if (i<wifi.Length) addstr(ref crc, ref s, (int)wifi[i], 1); else addstr(ref crc, ref s, 0, 1); 
                for (int i=0; i<32; i++) if (i<wifip.Length) addstr(ref crc, ref s, (int)wifip[i], 1); else addstr(ref crc, ref s, 0, 1); 
            }
            s+='#';
            Debug.WriteLine(s);
            SendSerialCommand(s, 0);
        }

        public static int readHex(string s, ref int i, int l = 0)
        {
            int v = 0;
            if (l == 0) l = s.Length;
            while (i < l)
                if (s[i] >= '0' && s[i] <= '9') v = v * 16 + s[i++] - '0';
                else if (s[i] >= 'a' && s[i] <= 'f') v = v * 16 + s[i++] - 'a' + 10;
                else if (s[i] >= 'A' && s[i] <= 'F') v = v * 16 + s[i++] - 'A' + 10;
                else break;
            return v;
        }
        public static int readHex2(string s, ref int i, int l = 0)
        {
            int v = 0; int m = 1;
            if (l == 0) l = s.Length;
            while (i < l)
            {
                if (s[i] >= '0' && s[i] <= '9') v = v + (s[i++] - '0') * m*16;
                else if (s[i] >= 'a' && s[i] <= 'f') v = v + (s[i++] - 'a' + 10) * m * 16;
                else if (s[i] >= 'A' && s[i] <= 'F') v = v + (s[i++] - 'A' + 10) * m * 16;
                else break;
                if (s[i] >= '0' && s[i] <= '9') v = v + (s[i++] - '0') * m;
                else if (s[i] >= 'a' && s[i] <= 'f') v = v + (s[i++] - 'a' + 10) * m;
                else if (s[i] >= 'A' && s[i] <= 'F') v = v + (s[i++] - 'A' + 10) * m;
                else break;
                m *= 256;
            }
            return v;
        }
        public static int readDec(string s, ref int i)
        {
            int v = 0;
            while (i < s.Length)
                if (s[i] >= '0' && s[i] <= '9') v = v * 10 + s[i++] - '0';
                else break;
            return v;
        }
        public static int fromHms(string s, out bool ok)
        {
            ok = true; if (s.Length==0) {  ok= false; return 0; }
            int v = 0;  int i = 0; int neg = 1;
            if (s.Length > i) if (s[i] == '-') { i++; neg = -1; } else if (s[i] == '+') i++;
            v = readDec(s, ref i)*3600;
            if (i >= s.Length || (s[i]!=':' && s[i]!='*')) return v;
            i++;
            v+= readDec(s, ref i) * 60;
            if (i >= s.Length || (s[i] != ':' && s[i] != '*')) return v;
            i++;
            v += readDec(s, ref i);
            return v*neg;
        }
        public static int getHexFromDevice(string command, out bool ok, bool acceptHms= false)
        {
            ok = false;
            string hex = SendSerialCommand(command);
            if (hex.Length <= 0) return -1;
            if (acceptHms) return fromHms(hex, out ok);
            int i = 0;
            ok = true; return readHex(hex, ref i);
        }
        public static string SendSerialCommand(string command, int waitReturn=1) // 0: is no wait return, 1 is wait for '#'
        {
            if (comPort!="tcp")
            { 
                if (!SharedSerial.Connected) return String.Empty;
                lock (lockObject)
                {
                    try
                    {
                        // SharedSerial.ReceiveTimeoutMs= 10;
                        // try { doLog(SharedSerial.ReceiveTerminated("#")); } catch { }
                        // SharedSerial.ReceiveTimeoutMs= 1000;
                        SharedSerial.ClearBuffers();
                        // if (command.Length>1) doLog(command);
                        SharedSerial.Transmit(command);
                    }
                    catch (Exception) { Disconnect(); return String.Empty; } // end of work here...
                    if (waitReturn==0) return String.Empty;
                    try
                    {
                        return SharedSerial.ReceiveTerminated("#").Replace("#", String.Empty);
                    }
                    catch (Exception) { return String.Empty; } // timeout... not a deadly error
                }
            }
            else
            {
                if (tcpstream==null) return String.Empty;
                lock (lockObject)
                {
                    byte[] buffer = new byte[1024]; 
                    try
                    {
                        while (tcpstream.DataAvailable) tcpstream.Read(buffer, 0, buffer.Length); // flush
                        byte[] dataToSend = Encoding.UTF8.GetBytes(command); tcpstream.Write(dataToSend, 0, dataToSend.Length); // send data
                    }
                    catch (Exception) { Disconnect(); return String.Empty; } // end of work here...
                    if (waitReturn==0) return String.Empty;
                    try
                    {
                        string ret= "";
                        for (int i=0; i<20; i++)
                        {
                            Thread.Sleep(50); if (!tcpstream.DataAvailable) continue; // 1s wait total...
                            int bytesRead= tcpstream.Read(buffer, 0, buffer.Length);
                            ret += Encoding.UTF8.GetString(buffer, 0, bytesRead);
                            int p= ret.IndexOf("#"); if (p>=0) return ret.Substring(0, p);
                        }
                        return String.Empty;
                    }
                    catch (Exception) { return String.Empty; } // timeout... not a deadly error
                }
            }
        }
        public static string raToText(double v) { return raToText((int)(v * 3600)); }
        public static string raToText(int v)
        {
            string n = "";
            if (v < 0) { n = "-"; v = -v; }
            return n + (v / 3600).ToString() + ":" + ((v / 60) % 60).ToString("D2") + ":" + (v % 60).ToString("D2");
        }
        internal static void SlewToCoordinatesAsync(double RightAscension, double Declination, bool sync)
        {
            doLog("SlewToCoordinatesAsync "+(sync ? "Sync " : "Go ") + 
                  RightAscension.ToString()+":"+((int)(RightAscension*3600)).ToString() + " ("+ ((int)(RightAscension * 3600)).ToString("X8")+")   " +
                  Declination.ToString()+":"+((int)(Declination * 3600)).ToString() + " (" + ((int)(Declination * 3600)).ToString("X8") + ")", 0);
            SharedResources.SendSerialCommand(":M"+(sync?"S":"G") + ((int)(RightAscension*3600)).ToString("X8")+ ((int)(Declination*3600)).ToString("X8")+  "#", 0);
        }

        public static double GetLocalSiderealTime()
        {
            DateTime utc= DateTime.UtcNow;
            double longitudeDegrees= Longitude/36000.0f; // Verify???
            // Convert to Julian Date
            int Y = utc.Year;
            int M = utc.Month;
            double D = utc.Day + utc.Hour / 24.0 + utc.Minute / 1440.0 + utc.Second / 86400.0;
            if (M <= 2) { Y -= 1; M += 12; }
            int A = Y / 100;
            int B = 2 - A + (A / 4);
            double jd = Math.Floor(365.25 * (Y + 4716)) + Math.Floor(30.6001 * (M + 1)) + D + B - 1524.5;
            double d = jd - 2451545.0; // Days since J2000.0

            // Calculate GMST in hours
            double gmst = 18.697374558 + 24.06570982441908 * d;
            gmst = gmst % 24;
            if (gmst < 0) gmst += 24;

            // Convert longitude to hours and add to GMST
            double lst = gmst + (longitudeDegrees / 15.0);
            lst = lst % 24;
            if (lst < 0) lst += 24;

            return lst;
        }


        static Thread phd2threadout = null;
        static bool phd2guide= false;

        static void phd2Thread()
        {
            byte[] sendData = Encoding.UTF8.GetBytes("Hello Server\n");

            while (!finish)
            {
                TcpClient client = null;
                NetworkStream stream = null;

                try
                {
                    client = new TcpClient();
                    client.Connect("127.0.0.1", 4400);
                    doLog("phd2 Connected", 3);
                    stream = client.GetStream();
                    string received= "";

                    while (!finish)
                    {
                        if (phd2guide)
                        { 
                            // acording to the phd data, calling guide will do a loop and find_star if needed...
                            // doLog("phd2 reguide loop", 3);
                            // byte[] data = Encoding.UTF8.GetBytes("{\"method\": \"loop\", \"id\": 1}\n");
                            // stream.Write(data, 0, data.Length); Thread.Sleep(4000);
                            // doLog("phd2 reguide find", 3);
                            // data = Encoding.UTF8.GetBytes("{\"method\": \"find_star\", \"id\": 2}\n");
                            // stream.Write(data, 0, data.Length); Thread.Sleep(7000);
                            doLog("phd2 reguide guide", 3);
                            byte[] data = Encoding.UTF8.GetBytes("{\"method\": \"guide\", \"params\": {\"settle\": {\"pixels\": 3, \"time\": 8, \"timeout\": 40}}, \"id\": 3}\n");
                            stream.Write(data, 0, data.Length); phd2guide = false;
                            stream.Flush();
                            Thread.Sleep(200);
                        }
 
                        // Read all available data without blocking forever
                        if (stream.DataAvailable)
                        {
                            byte[] buffer = new byte[client.Available];
                            int bytesRead = stream.Read(buffer, 0, buffer.Length);
                            if (bytesRead == 0) { doLog("phd2 closed the connection", 3); break; }

                            received += Encoding.UTF8.GetString(buffer, 0, bytesRead);
                            while (true) // find strings in this mess and display them...
                            {
                                int posCR = received.IndexOf('\r');
                                int posLF = received.IndexOf('\n');
                                int endPos;
                                if (posCR == -1 && posLF == -1) break;
                                if (posCR == -1) endPos = posLF;
                                else if (posLF == -1) endPos = posCR;
                                else endPos = Math.Min(posCR, posLF);
                                string line = received.Substring(0, endPos);
                                doLog("phd2<- " + line, 3);
                                int skip = 1; // CR or LF
                                if (endPos < received.Length - 1 && received[endPos] == '\r' && received[endPos + 1] == '\n') skip = 2; // CRLF pair
                                received = received.Substring(endPos + skip);
                            }
                        }
                    }
                }
                catch (SocketException ex) { doLog("phd2 Socket error: " + ex.Message, 3); }
                catch (Exception ex)  { doLog("phd2 error: " + ex.Message, 3); }
                finally { if (stream != null) stream.Close(); if (client != null) client.Close(); }
                Thread.Sleep(1000);
            }
        }

        public static void phd2reguide()
        {
            if (phd2threadout == null) { phd2threadout = new Thread(new ThreadStart(phd2Thread)); phd2threadout.Start(); }
            phd2guide = true;
        }
        public static bool finish= false;

        // public static void StopGuiding()
        // {
        //     // SendCommandAsync("{\"method\": \"stop_capture\", \"id\": 2}");
        // }

        static NetworkStream tcpstream= null;
        static TcpClient tcpclient= null;
        static void tcpdisconnect()
        {
            if (tcpclient!=null) tcpclient.Dispose(); tcpclient= null; tcpstream= null;
        }
        static void tcpconnect()
        {
            if (tcpclient==null) tcpclient = new TcpClient();
            try { 
                if (!tcpclient.Connected) tcpclient.Connect("127.0.0.1", 8080);
                tcpstream = tcpclient.GetStream();
            } catch { }
        }

        public static string isstle1="", isstle2="", locations= "", scopes="";

        public static void setToTrueNorth()
        {  // set RA if at default position...
            double sd= GetLocalSiderealTime();
            if (_sideOfPier==0) sd-= 6.0f; else sd+= 6.0f; // setup ra depending on side of pier!
            while (sd<0.0f) sd+= 24.0f; while (sd>24.0f) sd-= 24.0f;
            SlewToCoordinatesAsync(sd, 90.0f, true);
        }
    }
}
