using ASCOM.Astrometry.AstroUtils;
using ASCOM.Astrometry.Transform;
using ASCOM.Utilities;
using System;
using System.Diagnostics;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace ASCOM.LocalServer
{
    [HardwareClass]
    public static class SharedResources
    {
        public interface ILog { void log(string message, int source); };
        public static ILog log= null;
        public static void doLog(string msg, int source) 
        { 
            if (log==null) return; log.log(msg, source);
        }

        public static bool appClosing= false;

        // Shared serial port. This will allow multiple drivers to use one single serial port.
        private static Serial SharedSerial = new Serial(); // Shared serial port
        public static string _comPort= "COM1";
        internal static Util utilities = new Util(); // ASCOM Utilities object for use as required
        internal static AstroUtils astroUtilities = new AstroUtils(); // ASCOM AstroUtilities object for use as required
        public static void Dispose()
        {
            workerThreadRequest= -1;
        }

        [DllImport("kernel32.dll")]
        static extern uint SetThreadExecutionState(uint esFlags);
        const uint ES_CONTINUOUS        = 0x80000000;
        const uint ES_SYSTEM_REQUIRED   = 0x00000001;
        const uint ES_DISPLAY_REQUIRED  = 0x00000002;

        public static Transform azimutal = new Transform();
        public static double Altitude { get { try { azimutal.JulianDateUTC= utilities.DateUTCToJulian(DateTime.UtcNow); return azimutal.ElevationTopocentric; } catch { return 90.0f; } } }
        public static double Azimuth { get { try { azimutal.JulianDateUTC = utilities.DateUTCToJulian(DateTime.UtcNow); return azimutal.AzimuthTopocentric; } catch { return 180.0f; } } }

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
        public static DateTime trackingStopTime; // time when tracking was stopped... used for satelite tracking...
        public static bool TrackingDisabled { get { return _trackingDisabled; } set { ascomtrack= !value; _trackingDisabled= value; if (value) trackingStopTime= DateTime.UtcNow; sendTrackSpeed(); } }
        public static void Track(bool running, int rate) { _trackingDisabled=!running; ascomtrack= running; TrackingRate= rate; }

        public struct Ttimes { public long pctime, HWtime, uncountedSteps; };
        private static Ttimes[] times= new Ttimes[120];
        private static int timesPos= 0;
        private static long startpcTime;
        public static double guideRaAgressivity= 1.0, guideDecAgressivity = 1.0;
        public static void SetScopeMoving() { _ScopeMoving= true; } // used when you tell the scope to move to set the var to move until we get more info from the scope itself...
        public static void SetScopeGuiding() { _ScopeGuiding= true; } // used when you tell the scope to move to set the var to move until we get more info from the scope itself...
        public static bool _ScopeMoving = false, _ScopeGuiding= false;
        public static bool ScopeMoving { get { if (_ScopeGuiding) return false; return _ScopeMoving; } }
        private static long _spanPC = 0;
        public static long timeSpanPC { get { return _spanPC; } }
        private static long _spanHW = 0;
        public static long timeSpanHW { get { return _spanHW; } }
        private static long _uncountedSteps= 0;
        public static long timeSpanUncountedSteps { get { return _uncountedSteps; } }
        private static bool _FocusMoving = false;
        public static bool FocusMoving { get { return _FocusMoving; } }
        private static int _sideOfPier = 0;
        public static int SideOfPier { get { return _sideOfPier; } }
        private static bool _meridianFlip = false;
        public static bool meridianFlip { get { return _meridianFlip; } }
        private static bool _flipDisabled= false;
        public static bool FlipDisabled { get { return _flipDisabled; } set { SendSerialCommand(":$f"+(value?"0":"1")+"#", 0); } }
        private static bool _trackingDisabled= false;
        private static double _Declinaison = 0;
        public static double Declinaison { get { return _Declinaison; } }
        private static double _RightAssension = 0;
        public static double RightAssension { get { return _RightAssension; } }
        public static int _FocusserPosition = 0;
        public static int FocusserPosition { get { return _FocusserPosition; } }
        public static bool hasHWData = false, hasHWPos= false;
        public static bool dataDisplayed = false;
        public static int raMaxPos=0, raMaxSpeed=0, ramsToSpeed=0, decMaxPos=0, decMaxSpeed=0, decmsToSpeed=0, timeComp=0;
        public static int Latitude=0, Longitude=0, SiteAltitude=0, FocalLength=0, Diameter_mm=0, Area_cm2=0, FocStepdum=0;
        public static int focMaxStp= 0, focMaxSpd= 0, focAcc= 0;
        public static int decBacklash = 0, raAmplitude = 0, guideRateRA=0, guideRateDec=0;
        public static int raBacklash = 0, raSettle = 0, focBacklash = 0;
        public static int raPos= 0, decPos= 0;
        public static int invertAxes = 0, guidingBits=0;
        public static bool hasPowerCount= false;
        public static bool powerBit= false;
        public static int powerCount= 0;
        public static bool hasGpsInfo= false;
        public static bool guideAfterSlew = false, yellOnPower= false, focusInmm= false, reconnectOnDrop= false, parkAtSunrise= true, SyncRAHW= false;
        public static int midOfraRealPos = 6 * 3600; // stores the mid point of the RA axis in real coordinates. Used to check if somehting will need a meridial flip...
        public static string hwconfstring= "";
        public static string latestResponse1= "", latestResponse2= "";
        public static int responceCount= 0;
        public static bool haswifi= false;
        public static string wifi= "", wifip= "";
        public static uint ipaddr= 0;
        public static double BNOw=0.0, BNOx=0.0, BNOy=0.0, BNOz=0.0;
        public static int BNOTemp= 1000;
        public static bool BNOhas= false, BNOhasOffset1= false, BNOhasOffset2= false, BNOscopeEast= false, BNOCalHere= false;
        public static double BNOscopeaz= 0.0, BNOscopealt= 0.0, BNObnoaz= 0.0, BNObnoalt= 0.0, BNOlst= 0.0f;

        public static void updateAzimutal()
        { 
            try { 
                azimutal.SiteLatitude = Latitude / 36000.0f;
                azimutal.SiteLongitude = Longitude / 36000.0f;
                azimutal.SiteElevation = SiteAltitude;
            } catch (Exception) { }
        }

        public static void requestDisconnect() { workerThreadRequest= -1; }
        private static void Disconnect() // force disconnect. setting connected to false will NOT disconnect as multiple clients might be asking for a disconnection
        {
            try { 
                serialCrahed= false;
                responceCount= 0;
                timesPos = 0;
                connectionLive = false; hasHWPos= false; hasHWData = false; hasGpsInfo= false; dataDisplayed= false; hasPowerCount= false;
                raMaxPos = 0; raMaxSpeed = 0; ramsToSpeed = 0; decMaxPos = 0; decMaxSpeed = 0; decmsToSpeed = 0;
                hasBeenParked= false;
                BNOhas= false;
                if (SharedSerial!=null) SharedSerial.Connected = false;
                tcpdisconnect(); 
                bTSerial.disconnect();
                doLog("Disconnect", -1);
            } catch (Exception) { }
        }
        public static string comPort 
        {
            get { return _comPort;  }
            set
            {
                if (value == _comPort || connectionLive) return; // no changes? or are we connected?
                _comPort = value;
                doLog("Set com to "+value, -1);
            }
        }
        static public void readHWString()
        {
            int i = 0; string v= hwconfstring;
            if (v.Length != 154 && v.Length != 154+32*4+8 && v.Length!=154+32*4+8+9*4*2) return;
            haswifi= v.Length == 154+32*4+8;
            dataDisplayed= false;
            raMaxPos = readHex2(v, ref i, i+8); raMaxSpeed = readHex2(v, ref i, i + 8); ramsToSpeed = readHex2(v, ref i, i + 8);
            decMaxPos = readHex2(v, ref i, i + 8); decMaxSpeed = readHex2(v, ref i, i + 8); decmsToSpeed = readHex2(v, ref i, i + 8);
            timeComp= readHex2(v, ref i, i + 8);

            Latitude = readHex2(v, ref i, i + 8); Longitude = readHex2(v, ref i, i + 8); SiteAltitude = readHex2(v, ref i, i + 4);
            updateAzimutal();
            
            FocalLength= readHex2(v, ref i, i + 4); Diameter_mm= readHex2(v, ref i, i + 4); Area_cm2= readHex2(v, ref i, i + 4); FocStepdum= readHex2(v, ref i, i + 4);
            focMaxStp = readHex2(v, ref i, i + 4); focMaxSpd = readHex2(v, ref i, i + 4); focAcc = readHex2(v, ref i, i + 4);
            decBacklash = readHex2(v, ref i, i + 4); raAmplitude = readHex2(v, ref i, i + 4);
            guideRateRA = readHex2(v, ref i, i + 2); guideRateDec =  readHex2(v, ref i, i + 2);
            invertAxes = readHex2(v, ref i, i + 2);
            guidingBits =  readHex2(v, ref i, i + 2);
            raBacklash = readHex2(v, ref i, i + 4);
            focBacklash = readHex2(v, ref i, i + 4);
            raSettle = readHex2(v, ref i, i + 2);

            wifi= ""; wifip= ""; ipaddr= 0;
            if (haswifi)
            { 
                i= 154; for (int j=0; j<32; j++) { int c= readHex2(v, ref i, i+2); if (c==0) break; wifi+= (char)c; }
                i= 154+32*2; for (int j=0; j<32; j++) { int c= readHex2(v, ref i, i+2); if (c==0) break; wifip+= (char)c; }
                i= 154+32*4; ipaddr= (uint)readHex2(v, ref i, i + 8);
            }

            // Assumes crc is correct and ignore extra for the moment...!!!
            hasHWData = true;
            resetSunRaiseTime();
        }

        static Thread workerThread= new Thread(loop) { Name = "PersistentWorkerThread", IsBackground = true };
        static int workerThreadRequest= 0; // set to 1 to connect, -1 to disconnect
        static public bool serialCrahed= false;
        public static DateTime lastHeartBeat;
        private static bool connectionLive = false;
        public static bool Connected
        {
            set
            {
                if (!workerThread.IsAlive) workerThread.Start(); // might be needed...
                if (!phd2Thread.IsAlive) phd2Thread.Start(); // might be needed...

                doLog("Connect", -1);
                if (!value) return; // We actually do NOT disconnect when asked by a client... just when asked by the main app!
                workerThreadRequest= value ? 1 : -1; // ok, -1 never used here...
            }
            get { return connectionLive; }
        }

        static public BTSerial bTSerial= new BTSerial();
        static void loop()
        {
            SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
            while (!appClosing)
            {
                Thread.Sleep(500);
                if (!connectionLive && workerThreadRequest==1) // Connection request? handle it!
                { 
                    Disconnect(); // This will not actually disconnect as they are already not connected. But it will reinit variables...
                    workerThreadRequest= 0;
                    if (SharedSerial.Connected || tcpstream!=null) continue; // already connected...
                    if (comPort.StartsWith("EQ")) bTSerial.connect(comPort); //tcpconnect();
                    else if (comPort=="tcp") tcpconnect();
                    else { 
                        SharedSerial.PortName = comPort;
                        SharedSerial.Speed = ASCOM.Utilities.SerialSpeed.ps38400;
                        try { SharedSerial.Connected = true; } catch (Exception) { doLog("could not connect", -1); continue; }
                        SharedSerial.ReceiveTimeout = 1;
                    }
                    startpcTime = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond;
                }

                if (workerThreadRequest==-1) { Disconnect(); workerThreadRequest = 0; continue; } // disconnect request? handle it!
                if (!SharedSerial.Connected && tcpstream==null && !bTSerial.isConnected()) continue; // not connected? do nothing...

                string v = SendSerialCommand("!"); if (v.Length<38) continue;

                connectionLive = true;
                latestResponse1= v; latestResponse2= ""; responceCount++;
                int i= 0;
                DateTime oldlastHeartBeat= lastHeartBeat;
                lastHeartBeat= DateTime.Now;
                int dec= readHex(v, ref i, 6); if (dec>=0x800000) dec|= unchecked((int)0xff000000);
                int ra = readHex(v, ref i, i+6);
                double decf= dec/ 3600.0, raf= ra/ 3600.0;
                if (decf!=_Declinaison || raf!=_RightAssension) { _Declinaison = decf; _RightAssension= raf; try { azimutal.SetTopocentric(_RightAssension, _Declinaison); } catch (Exception) { } }
                _FocusserPosition = readHex(v, ref i, i+6);
                int bits= readHex(v, ref i, i+2);
                bool old_ScopeMoving = _ScopeMoving;
                _FocusMoving = (bits & 2) != 0;
                _ScopeGuiding= (bits & 128)!=0;
                if (_ScopeGuiding) _ScopeMoving= false; else _ScopeMoving = (bits & 1) != 0;
                if (old_ScopeMoving && !_ScopeMoving && guideAfterSlew) phd2reguide();
                _sideOfPier = ((bits & 4) != 0)?1:0; // 0 is east
                _meridianFlip= (bits & 8) != 0;
                _flipDisabled= (bits & 16) != 0;
                _trackingDisabled= (bits & 32) != 0;
                powerBit= (bits & 64) != 0;
                long timems = readHex(v, ref i, i + 6);
                midOfraRealPos = readHex(v, ref i, i + 6);
                long uncountedSteps= readHex(v, ref i, i+6);

                latestResponse2= "dec:"+dec.ToString() + " ra:"+ra.ToString()+
                                 " foc:"+_FocusserPosition.ToString() + " bits:"+bits.ToString("X") +
                                 " timems:"+timems.ToString() + " midOfra:"+midOfraRealPos.ToString() + " uncountedSteps:"+uncountedSteps.ToString();

                //Console.WriteLine("Uncounted Steps "+uncountedSteps.ToString());
                times[timesPos]= new Ttimes { pctime = DateTime.Now.Ticks / TimeSpan.TicksPerMillisecond - startpcTime, HWtime = timems, uncountedSteps= uncountedSteps};
                // used to check timing of the device clock...
                if (timesPos!=119) timesPos++;
                else
                {
                    doLog("Times (PC HW steps)\t"+times[timesPos].pctime.ToString()+"\t"+times[timesPos].HWtime.ToString()+"\t"+times[timesPos].uncountedSteps.ToString(), 5);
                    timesPos = 0;
                    _spanPC= times[119].pctime - times[0].pctime;
                    _spanHW= times[119].HWtime - times[0].HWtime;
                    _uncountedSteps= times[119].uncountedSteps - times[0].uncountedSteps;
                }

                if (v.Length>=54) // HW step positions
                {
                    hasHWPos= true;
                    raPos = readHex(v, ref i, i + 8);
                    decPos = readHex(v, ref i, i + 8);
                    latestResponse2+= " raPos:"+raPos.ToString();
                    latestResponse2+= " decPos:"+decPos.ToString();
                }
                if (v.Length>=56) // power GPS and tracking status
                {
                    hasPowerCount= true;
                    int tmp = readHex(v, ref i, i + 2);
                    powerCount= tmp&0x0f;
                    if (hasGpsInfo != ((tmp&0x10)!=0))
                    {
                        hasGpsInfo= (tmp&0x10)!=0;
                        if (hasGpsInfo) hasHWData= false; // force a reask of HW data to get new GPS data...
                    }
                    tmp>>= 5;
                    if (tmp==0) ascomtrack= false;
                    else { ascomtrack= true; ascomtrackspd= tmp-1; }
                    latestResponse2+= " powerCount:"+powerCount.ToString();
                }
                //Console.WriteLine("len "+v.Length.ToString());
                if (v.Length>=128) // BNO data...
                {
                    BNOw= readFloat(v, ref i, i+8);BNOx= readFloat(v, ref i, i+8);BNOy= readFloat(v, ref i, i+8);BNOz= readFloat(v, ref i, i+8);
                    BNOTemp= readHex2(v, ref i, i+2);
                    int tmp= readHex2(v, ref i, i+6);
                    BNOhasOffset1= (tmp&2)!= 0; BNOhasOffset2= (tmp&4)!= 0; BNOscopeEast= (tmp&8)!= 0; BNOCalHere= (tmp&256)!=0;
                    BNOscopeaz= readFloat(v, ref i, i+8)*180.0f/Math.PI; BNOscopealt= readFloat(v, ref i, i+8)*180.0f/Math.PI;
                    BNObnoaz= readFloat(v, ref i, i+8)*180.0f/Math.PI; BNObnoalt= readFloat(v, ref i, i+8)*180.0f/Math.PI;
                    BNOlst= readFloat(v, ref i, i+8);
                    if (!BNOhas) // just founda BNO. send it the current time to get stuff calculating OK...
                    {
                        DateTime epoch2024 = new DateTime(2024, 1, 1, 0, 0, 0, DateTimeKind.Utc);
                        TimeSpan difference = DateTime.UtcNow - epoch2024;
                        SendSerialCommand(":B000000002"+((int)difference.TotalSeconds).ToString("X8")+"#", 0); // send set UTC time command... one more 0 because first one is ignored!!!
                    }
                    BNOhas= true;
                }

                if (!hasHWData)
                {
                    v = SendSerialCommand("&");
                    if (v.Length == 154 || v.Length == 154+32*4+8)
                    { 
                        hwconfstring= v;
                        readHWString();
                        if (_Declinaison>89.9f && _RightAssension>5.59f && _RightAssension<6.01f) setToTrueNorth();
                    }
                }

                if (parkAtSunrise && lastHeartBeat>getSunRaiseTime() && oldlastHeartBeat.Year>2026 && oldlastHeartBeat<getSunRaiseTime()) // send park if sunrise happens!
                { 
                    resetSunRaiseTime();
                    Park();
                }
            }
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
        public static double readFloat(string s, ref int i, int l)
        {
            int j=readHex2(s, ref i, l);
            byte[] bytes = BitConverter.GetBytes(j);
            return BitConverter.ToSingle(bytes, 0);
        }
        public static int readDec(string s, ref int i)
        {
            int v = 0;
            while (i < s.Length)
                if (s[i] >= '0' && s[i] <= '9') v = v * 10 + s[i++] - '0';
                else break;
            return v;
        }
        public static double fromHms2(string s, out bool ok)
        {
            ok = true; if (s.Length==0) {  ok= false; return 0; }
            int v = 0;  int i = 0; int neg = 1;
            if (s.Length > i) if (s[i] == '-') { i++; neg = -1; } else if (s[i] == '+') i++;
            v = readDec(s, ref i)*3600;
            if (i >= s.Length || (s[i]!=':' && s[i]!='*')) return v*neg;
            i++;
            v+= readDec(s, ref i) * 60;
            if (i >= s.Length || (s[i] != ':' && s[i] != '*')) return v*neg;
            i++;
            v += readDec(s, ref i);
            if (i >= s.Length || s[i] != '.') return v*neg;
            i++; int j= i;
            double v2= readDec(s, ref i);
            while (j<i) { v2/=10; j++; }
            return (v+v2)*neg;
        }
        public static int fromHms(string s, out bool ok)
        {
            return (int)(fromHms2(s, out ok)+0.5);
        }
        static private readonly object lockObject = new object();
        public static string SendSerialCommand(string command, int waitReturn=1) // 0: is no wait return, 1 is wait for '#'
        {
            lock (lockObject)
            {
                if (command!="!") doLog("-> "+command, 2);

                try
                {
                    if (comPort.StartsWith("com")) // com port case....
                    { 
                        if (!SharedSerial.Connected) { Disconnect(); return String.Empty; }
                        try
                        {
                            SharedSerial.ClearBuffers();
                            SharedSerial.Transmit(command);
                        }
                        catch (Exception) { Disconnect(); serialCrahed=true; return String.Empty; } // end of work here...
                        if (waitReturn==0) return String.Empty;
                        return SharedSerial.ReceiveTerminated("#").Replace("#", String.Empty);
                    }

                    if (comPort.StartsWith("tcp"))
                    { 
                        if (tcpstream==null) { Disconnect(); return String.Empty; } // no connections
                        byte[] buffer = new byte[1024]; 
                        try
                        {
                            while (tcpstream.DataAvailable) tcpstream.Read(buffer, 0, buffer.Length); // flush
                            byte[] dataToSend = Encoding.UTF8.GetBytes(command); tcpstream.Write(dataToSend, 0, dataToSend.Length); // send data
                        }
                        catch (Exception) { Disconnect(); return String.Empty; } // end of work here...
                        if (waitReturn==0) return String.Empty;
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

                    if (comPort.StartsWith("EQ"))
                    {
                        if (!bTSerial.isConnected()) { Disconnect(); return String.Empty; }
                        byte[] buffer = new byte[1024]; 
                        try
                        {
                            while (bTSerial.read(20).Length!=0) Thread.Sleep(10); // flush
                            bTSerial.write(command);
                        }
                        catch (Exception) { Disconnect(); return String.Empty; } // end of work here...
                        if (waitReturn==0) return String.Empty;
                        string ret= "";
                        for (int i=0; i<20; i++)
                        {
                            Thread.Sleep(50); 
                            ret+= bTSerial.read(20);
                            int p= ret.IndexOf("#"); 
                            if (p>=0) return ret.Substring(0, p);
                        }
                        return String.Empty;
                    }

                    return String.Empty;
                } catch (Exception) { return String.Empty; } // timeout... not a deadly error
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
            // if sync and motor physical position not within 2° of where it is supposed to be, resync motor
            if (sync && hasHWPos && SyncRAHW)
            {
                double MRAAngle= GetLocalSiderealTime()-RightAscension; // 
                if (_sideOfPier==0) MRAAngle-= 6.0f; else MRAAngle+= 6.0f; // setup ra depending on side of pier!
                while (MRAAngle<0.0f) MRAAngle+= 24.0f; while (MRAAngle>24.0f) MRAAngle-= 24.0f;
                MRAAngle*=360.0f/24.0f; // in °. but where is top
                double pos= (double)raPos*360/raMaxPos-raAmplitude/2; // 0° at top
                if (Math.Abs(MRAAngle-pos)>2)
                {
                    // More than 2° off, resync motor
                    double newA= (MRAAngle+raAmplitude/2)*raMaxPos/360;
                    doLog("resync physical ra motor position to "+MRAAngle.ToString("F1") + "° = " + ((int)newA).ToString()+" (was " + pos.ToString("F1") + "° = "+((int)raPos).ToString()+")", 0);
                    SharedResources.SendSerialCommand(":Ms" + ((int)newA).ToString("X8")+ "#", 0);
                }
            }
            // if goto, handle phd2 if there...
            if (!sync) 
            {
                byte[] data = Encoding.UTF8.GetBytes("{\"method\": \"stop_capture\", \"id\":3 }\r\n");
                if (phd2SendData(data)) doLog("phd2 stop guide", 3);
            }
        }

        public static double GetSiderealTime(DateTime t, double longitudeDegrees)
        {
            DateTime utc= t;
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
        public static double GetLocalSiderealTime()
        {
            return GetSiderealTime(DateTime.UtcNow, Longitude/36000.0f);
        }
                public static string isstle1="", isstle2="", locations= "", scopes="";

        public static void setToTrueNorth()
        {  // set RA if at default position...
            double sd= GetLocalSiderealTime();
            if (_sideOfPier==0) sd-= 6.0f; else sd+= 6.0f; // setup ra depending on side of pier!
            while (sd<0.0f) sd+= 24.0f; while (sd>24.0f) sd-= 24.0f;
            SlewToCoordinatesAsync(sd, 90.0f, true);
            //if (BNOhas)
            //{
            //    double lstJan24= GetSiderealTime(new DateTime(2024, 1, 1), 0);
            //    int secDif= (int)((sd-lstJan24)*24*2300);
            //    SharedResources.SendSerialCommand(":B000000002"+secDif.ToString("X08")+"#", 0);
            //}
        }


        ///////////////////////////////////
        // PHD2 stuff...
        ///////////////////////////////////
        static Thread phd2Thread = new Thread(phd2loop) { Name = "PersistentWorkerThread", IsBackground = true };
        static public int phd2GuideDelay= 20;
        static DateTime phd2guide= DateTime.MinValue;
        static private readonly SemaphoreSlim phd2writeLock = new SemaphoreSlim(1, 1);
        static private NetworkStream stream = null;

        static public bool phd2SendData(byte[] data)
        {
            if (stream==null) return false;
            phd2writeLock.Wait();
            try
            {
                stream.Write(data, 0, data.Length); stream.Flush();
                phd2guide = DateTime.MinValue;
                return true;
            }
            finally
            {
                phd2writeLock.Release();
            }
        }
        static void phd2loop()
        {
            byte[] sendData = Encoding.UTF8.GetBytes("Hello Server\n");

            while (!appClosing)
            {
                TcpClient client = null;
                if (!guideAfterSlew) { Thread.Sleep(1000); continue; }

                try
                {
                    client = new TcpClient();
                    client.Connect("127.0.0.1", 4400);
                    doLog("phd2 Connected", 3);
                    stream = client.GetStream();
                    string received= "";

                    while (!appClosing)
                    {
                        if (phd2guide!=DateTime.MinValue && phd2guide<DateTime.Now)
                        { 
                            // acording to the phd data, calling guide will do a loop and find_star if needed...
                            // doLog("phd2 reguide loop", 3);
                            // byte[] data = Encoding.UTF8.GetBytes("{\"method\": \"loop\", \"id\": 1}\n");
                            // stream.Write(data, 0, data.Length); Thread.Sleep(4000);
                            // doLog("phd2 reguide find", 3);
                            // data = Encoding.UTF8.GetBytes("{\"method\": \"find_star\", \"id\": 2}\n");
                            // stream.Write(data, 0, data.Length); Thread.Sleep(7000);
                            doLog("phd2 reguide guide", 3);
                            byte[] data = Encoding.UTF8.GetBytes("{\"method\": \"guide\", \"params\": {\"settle\": {\"pixels\": 3, \"time\": 8, \"timeout\": 40}}, \"id\": 3}\r\n");
                            phd2SendData(data); 
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
                        Thread.Sleep(200);
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
            phd2guide =  DateTime.Now.AddSeconds(phd2GuideDelay);
        }

        ///////////////////
        /// TCP connection stuff
        ///////////////////
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






        ///////////////////
        /// get sunraise time...
        ///////////////////
        public static double SolarAltitudeUtc(DateTime utcTime, double latitude, double longitude)
        {
            // Force UTC kind to avoid accidental local conversion errors
            if (utcTime.Kind != DateTimeKind.Utc) utcTime = DateTime.SpecifyKind(utcTime, DateTimeKind.Utc);
            // 1. Convert UTC DateTime to Julian Day & Julian Century
            double julianDay = GetJulianDay(utcTime);
            double jc = (julianDay - 2451545.0) / 36525.0;

            // 2. Solar coordinates (Geometric Mean Longitude, Anomaly, Eccentricity)
            double geomMeanLongSun = (280.46646 + jc * (36000.76983 + jc * 0.0003032)) % 360.0;
            double geomMeanAnomSun = 357.52911 + jc * (35999.05029 - 0.0001537 * jc);
            double eccentEarthOrbit = 0.016708634 - jc * (0.000042037 + 0.0000001267 * jc);

            // Sun Equation of the Center
            double radAnom = ToRadians(geomMeanAnomSun);
            double sunEqOfCtr = Math.Sin(radAnom) * (1.914602 - jc * (0.004817 + 0.000014 * jc))
                              + Math.Sin(2 * radAnom) * (0.019993 - 0.000101 * jc)
                              + Math.Sin(3 * radAnom) * 0.000289;

            double sunTrueLong = geomMeanLongSun + sunEqOfCtr;
            double sunAppLong = sunTrueLong - 0.00569 - 0.00478 * Math.Sin(ToRadians(125.04 - 1934.13 * jc));

            // Mean and Obliquity of Ecliptic
            double meanObliqEcliptic = 23.0 + (26.0 + (21.448 - jc * (46.815 + jc * (0.00059 - jc * 0.001813))) / 60.0) / 60.0;
            double obliqCorr = meanObliqEcliptic + 0.00256 * Math.Cos(ToRadians(125.04 - 1934.13 * jc));

            // Declination of the Sun
            double sunDeclin = ToDegrees(Math.Asin(Math.Sin(ToRadians(obliqCorr)) * Math.Sin(ToRadians(sunAppLong))));

            // Equation of Time (in minutes)
            double vary = Math.Tan(ToRadians(obliqCorr / 2.0)) * Math.Tan(ToRadians(obliqCorr / 2.0));
            double radMeanLong = ToRadians(geomMeanLongSun);
            double eqOfTime = 4.0 * ToDegrees(vary * Math.Sin(2.0 * radMeanLong) 
                              - 2.0 * eccentEarthOrbit * Math.Sin(radAnom) 
                              + 4.0 * eccentEarthOrbit * vary * Math.Sin(radAnom) * Math.Cos(2.0 * radMeanLong) 
                              - 0.5 * vary * vary * Math.Sin(4.0 * radMeanLong) 
                              - 1.25 * eccentEarthOrbit * eccentEarthOrbit * Math.Sin(2.0 * radAnom));

            // 3. True Solar Time & Hour Angle
            double timeOffset = eqOfTime + (4.0 * longitude);
            double trueSolarTime = (utcTime.TimeOfDay.TotalMinutes + timeOffset + 1440.0) % 1440.0;
        
            double hourAngle = (trueSolarTime / 4.0 < 0) ? (trueSolarTime / 4.0 + 180.0) : (trueSolarTime / 4.0 - 180.0);

            // 4. Zenith and Altitude
            double radLat = ToRadians(latitude);
            double radDeclin = ToRadians(sunDeclin);
            double radHourAngle = ToRadians(hourAngle);

            double solarZenith = ToDegrees(Math.Acos(Math.Sin(radLat) * Math.Sin(radDeclin) 
                               + Math.Cos(radLat) * Math.Cos(radDeclin) * Math.Cos(radHourAngle)));

            double altitude = 90.0 - solarZenith;

            // 5. Azimuth
            double azNumerator = -(Math.Sin(radHourAngle));
            double azDenominator = (Math.Cos(radHourAngle) * Math.Sin(radLat)) - Math.Tan(radDeclin) * Math.Cos(radLat);
            double azimuth = ToDegrees(Math.Atan2(azNumerator, azDenominator));
            if (azimuth < 0.0) azimuth += 360.0;

            return altitude;
        }

        private static double GetJulianDay(DateTime utc)
        {
            int year = utc.Year;
            int month = utc.Month;
            int day = utc.Day;
            if (month <= 2) { year -= 1; month += 12; }
            int a = year / 100;
            int b = 2 - a + (a / 4);
            double dayFraction = (utc.TimeOfDay.TotalSeconds) / 86400.0;
            return Math.Floor(365.25 * (year + 4716)) + Math.Floor(30.6001 * (month + 1)) + day + dayFraction + b - 1524.5;
        }

        private static double ToRadians(double degrees) => degrees * (Math.PI / 180.0);
        private static double ToDegrees(double radians) => radians * (180.0 / Math.PI);

        // Find the UTC time on the given UTC date when solar altitude crosses targetAltDegrees upwards.
        // Returns null if no crossing on that UTC calendar day.
        public static DateTime FindRiseCrossingUtc(double latDeg, double lonDeg, double targetAltDegrees = -5.0)
        {
            DateTime t = DateTime.UtcNow.Date; // midnight UTC of that day
            double prevAlt = SolarAltitudeUtc(t, latDeg, lonDeg);
            while (true)
            {
                DateTime t2= t.AddMinutes(5);
                double alt = SolarAltitudeUtc(t2, latDeg, lonDeg);
                if (prevAlt<targetAltDegrees && alt>=targetAltDegrees) return t; // time when altitude first reaches target (UTC)
                t= t2;
                prevAlt= alt;
            }
        }
        public static DateTime sunRaiseTime= DateTime.MinValue;
        public static void resetSunRaiseTime() { sunRaiseTime= DateTime.MinValue; }
        public static DateTime getSunRaiseTime()
        {
            if (sunRaiseTime!=DateTime.MinValue) return sunRaiseTime;
            return sunRaiseTime= FindRiseCrossingUtc(Latitude/36000.0f, Longitude/36000.0f).ToLocalTime();
        }






        ///////////////////
        /// Parking stuff
        ///////////////////
        internal static void parkPos(out int ra, out int dec)
        {
            ra = (int)(((Int64)(raMaxPos)) * raAmplitude / 360 / 2);
            dec = decMaxPos / 2;
        }
        static public bool hasBeenParked= false;
        internal static void Park()
        {
            hasBeenParked= true;
            doLog("park", 0);
            TrackingDisabled = true;
            int ra, dec; parkPos(out ra, out dec);
            goToMotor(ra, dec);
        }
        public static void goToMotor(int ra, int dec)
        {
            SendSerialCommand(":Mg" + ra.ToString("X8") + dec.ToString("X8") + "#", 0);
            _ScopeMoving = true;
        }
    }
}
