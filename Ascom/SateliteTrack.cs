using ASCOM.Astrometry.Transform;
using ASCOM.EQControl.Telescope.V1;
using ASCOM.LocalServer;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Media;
using System.Net;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using static System.Net.Mime.MediaTypeNames;

public partial class SateliteTrack : Form
{
    private const int EM_SETCUEBANNER = 0x1501;
    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    private static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, string lParam);
    public SateliteTrack()
    {
        int TODO; // Add correction!
        InitializeComponent();
        checkBox14.Enabled= false;
        SendMessage(textBox23.Handle, EM_SETCUEBANNER, (IntPtr)1, "Iss");
        Thread workerThread = new Thread(issLoop) { Name = "PersistentWorkerThread", IsBackground = true };
        workerThread.Start();
        labelCorrection.Text= labelErr.Text= labelPos.Text= "";
    }

    // trak if keys are pressed to apply corrections
    int keyState = 0;           // 1:up 2:down 3:right 4:left
    bool keyStateShift= false;  // Is shift up or down (shift speeds up movement)
    // satellite TLEs..
    string issl1, issl2;
    // record positions during tracking for display
    struct TTrackingInfo { public double ra, dec, cra, cdec, az, alt; public TTrackingInfo(double pra, double pdec, double pcra, double pcdec, double _az, double _alt) {  ra= pra; dec= pdec; cra= pcra; cdec= pcdec; az = _az; alt = _alt;  }  }
    List<TTrackingInfo> TrackingInfos;

    private void label51_Click(object sender, EventArgs e)
    {
        Process.Start(new ProcessStartInfo("cmd", "/c start https://www.n2yo.com") { CreateNoWindow = true });
    }

    // Display the satelite position in the sky, scope ra, dec and corrections
    void updateIssImage()
    {
        int w= pictureBox2.Width, h= pictureBox2.Height;
        Bitmap b= new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
        Graphics g = Graphics.FromImage(b);
        g.FillRectangle(new SolidBrush(Color.Black), 0, 0, w, h);
        g.DrawEllipse(new Pen(Color.Yellow), 0, 0, w, h);
        if (TrackingInfos.Count>=2)
        {
            double fx = Math.Cos(-(TrackingInfos[0].az+90)*Math.PI/180) * Math.Cos(TrackingInfos[0].alt*Math.PI/180)*w/2+w/2;
            double fy = Math.Sin(-(TrackingInfos[0].az+90)*Math.PI/180) * Math.Cos(TrackingInfos[0].alt*Math.PI/180) *h/2+h/2;
            PointF p = new PointF((float)fx, (float)fy);
            for (int i = 0; i < TrackingInfos.Count-1; i++)
            {
                fx = Math.Cos(-(TrackingInfos[i+1].az+90)*Math.PI/180) * Math.Cos(TrackingInfos[i+1].alt*Math.PI/180) * w / 2 + w / 2;
                fy = Math.Sin(-(TrackingInfos[i+1].az+90)*Math.PI/180) * Math.Cos(TrackingInfos[i+1].alt*Math.PI/180) * h / 2 + h / 2;
                PointF p2 = new PointF((float)fx, (float)fy);
                g.DrawLine(new Pen(Color.Yellow, 2), p, p2);
                p = p2;
            }
        }

        for (int i = 0; i < TrackingInfos.Count-1; i++)
        {
            float x= (float)i*w/TrackingInfos.Count;

            // spds
            double sra= TrackingInfos[i+1].ra-TrackingInfos[i].ra, sdec= TrackingInfos[i+1].dec-TrackingInfos[i].dec;
            float yra= ((float)sra+4)*h/8; g.FillRectangle(new SolidBrush(Color.Blue), x, yra, 1, 1);
            float ydec= ((float)sdec+4)*h/8; g.FillRectangle(new SolidBrush(Color.Green), x, ydec, 1, 1);

            // Corrections
            float cra= ((float)TrackingInfos[i].cra*15+4)*h/8; g.FillRectangle(new SolidBrush(Color.Red), x, cra, 1, 1);
            float cdec= ((float)TrackingInfos[i].cdec+4)*h/8; g.FillRectangle(new SolidBrush(Color.Red), x, cdec, 1, 1);
        }

        pictureBox2.Image= b;
    }

    bool makeIssVisible= false;
    // forever thread...
    private void issLoop()
    {
        int issTrackMode = 0; // 0: no tracking. 1: first goto was sent. 2: regular tracking
        DateTime lastTrackingInfo= DateTime.UtcNow; // used to save position every second...
        double lastDelayToISSPass = 0; // last time we checked in how long the ISS will pass overhead. used to find 5mn warning...
        double issTrackDeltaRa = 0.0, issTrackDeltaDec= 0.0;
        int addDeltaSecToUtc= 0; // used to give a fake time to test the system.
        int sleep= 1000;
        while (true)
        { 
            try { BeginInvoke((MethodInvoker)delegate () 
                { 
                    sleep= 1000;
                    if (!SharedResources.Connected || !SharedResources.hasHWData || nbPasses<=0) checkBox14.Checked= false; // verify if we can do anything...
                    if (!hasTLE()) // can not track if no TLE...
                    {
                        checkBox14.Checked= false; 
                        labelVisible.Text = "No TLE";
                        nbPasses= -1; issTrackMode= 0; return;
                    }

                    // get current iss pos and display it...
                    DateTime utc= DateTime.UtcNow;
                    var r= GetIssRaDecFromLocation(addDeltaSecToUtc);
                    labelPos.Text = "pos " + tohms(r.ra) + "/" + tohms(r.dec)+" az:"+tohms(r.az)+"/"+ tohms(r.alt);
                    //Console.WriteLine(labelPos.Text);

                    // if not visible, display time to wait...
                    if (r.alt<=0)
                    { 
                        string next = "NotVisible";
                        if (nbPasses==-1) nextPass();
                        if (nbPasses!=0)
                        {
                            double v= passes[0].timeTo_H-((utc-passesTime).TotalSeconds/3600.0);
                            if (lastDelayToISSPass>5.0/60.0 && v<5.0/60.0 && checkBox17.Checked)
                                try { SoundPlayer player = new SoundPlayer(@"iss.wav"); player.Play(); } catch { }
                            lastDelayToISSPass= v;
                            addDeltaSecToUtc= 0;
                            if (makeIssVisible) { makeIssVisible= false; addDeltaSecToUtc= (int)(lastDelayToISSPass*3600)+30; }
                            next= "visible in:";
                            if (v>=1.0)
                            { 
                                next += ((int)v).ToString() + "h";
                                v = (v - (int)(v)) * 60; next += ((int)v).ToString() + "m ";
                            } else
                            {
                                v = (v - (int)(v)) * 60; next += ((int)v).ToString() + "m ";
                                v = (v - (int)(v)) * 60; next += ((int)v).ToString() + "s ";
                            }
                            next+= "top:"+((int)passes[0].max_elevation_deg).ToString()+"° ";
                            next+= "for:"+((int)(passes[0].duration_mn*60)).ToString()+"mn ";
                            next+= "speed:"+issNextPassMaxRaSpd.ToString("N1")+"°/s "+issNextPassMaxDecSpd.ToString("N1")+"°/s ";
                        }
                        labelVisible.Text = next;
                    }

                    if (r.alt<=0) { if (issTrackMode!=0) { nbPasses= -1; issTrackMode= 0; }  return; } // no tracking to do... ask for passes recalc if just crossed over...

                    labelVisible.Text = "Visible!";
                    if (!checkBox14.Checked) return; // nothing to do!

                    if (SharedResources.meridianFlip) { issTrackMode=1; return; } // nothing we can do here...
                    if (issTrackMode==0) // first track. goto start coordinates...
                    {
                        issTrackMode= 1;        // state is: going to start position...
                        SharedResources.TrackingDisabled= true; // stop sideral movement...
                        TelescopeHardware.SlewToCoordinatesAsync(r.ra, r.dec); // go to current pos...
                        issTrackDeltaRa= 0.0; issTrackDeltaDec= 0.0;
                        TrackingInfos= new List<TTrackingInfo>(); 
                    } 
                    else if (issTrackMode==1) 
                    { 
                        if (SharedResources.ScopeMoving) return; // wait to complete movement...
                        // it took time to get to the start point. so we redo a goto to the new position...
                        // and init all tracking data...
                        issTrackMode= 2; // now enter "normal" tracking mode...
                        TelescopeHardware.SlewToCoordinatesAsync(r.ra, r.dec);
                        TrackingInfos.Add(new TTrackingInfo(r.ra, r.dec, issTrackDeltaRa, issTrackDeltaDec, r.az, r.alt));
                    } 
                    else { // tracking. to to new coordinates at speed equal to the delta between the last 2 coordinates...
                        r= GetIssRaDecFromLocation(1+addDeltaSecToUtc); // position in 1 second from now...
                        // generate track command
                        double nra= r.ra+issTrackDeltaRa, ndec= r.dec+issTrackDeltaDec;
                        int ra= (int)(nra*3600), dec= (int)(ndec*3600);
                        int time= 1000; // be there in 1000ms
                        int crc= ra+(ra>>8)+(ra>>16) + (dec)+(dec>>8)+(dec>>16) + (time)+(time>>8);
                        SharedResources.doLog("Track "+nra.ToString("N4")+" "+ndec.ToString("N4")+" "+time.ToString(), 4);
                        string cmd= ":T" + (ra&0xffffff).ToString("X6")+ (dec&0xffffff).ToString("X6")+(time&0xffff).ToString("X4") +  (crc&0xff).ToString("X2") + "#";
                        SharedResources.SendSerialCommand(cmd, 0);
                        // handle corrections
                        double multip= 2.0; // speed up keys which are too slow...
                        if ((keyState&1)!=0) issTrackDeltaDec+=(keyStateShift?5.0:1)*5.0/3600.0*multip;
                        if ((keyState&2)!=0) issTrackDeltaDec-=(keyStateShift?5.0:1)*5.0/3600.0*multip;
                        if ((keyState&4)!=0) issTrackDeltaRa+=(keyStateShift?5.0:1)*5.0/3600.0/15.0*multip;
                        if ((keyState&8)!=0) issTrackDeltaRa-=(keyStateShift?5.0:1)*5.0/3600.0/15.0*multip;
                        labelCorrection.Text= "correction "+(issTrackDeltaRa*3600*15).ToString("N0")+"/"+(issTrackDeltaDec*3600).ToString("N0");
                        if (keyState!=0) SharedResources.doLog(labelCorrection.Text, 4);
                        // add info for display...
                        if (utc.Subtract(lastTrackingInfo).TotalMilliseconds>1000)
                        { 
                            lastTrackingInfo= utc;
                            TrackingInfos.Add(new TTrackingInfo(nra, ndec, issTrackDeltaRa, issTrackDeltaDec, r.az, r.alt));
                            updateIssImage();
                        }
                        // do 10 times per second
                        sleep= 100; return;
                    }
                    
                }); 
                Thread.Sleep(sleep); 
            } catch { } 
        }
    }

    void writeISSLog()
    {
        if (TrackingInfos!=null && TrackingInfos.Count > 0 )
        {
            string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
            string filePath = Path.Combine(desktopPath, "iss_"+DateTime.Now.ToString("yyyy-MM-dd_HH_mm_ss") +".txt");
            string content = "ra\tdec\tcor ra\tcordec\r\n";
            for (int i= 0; i < TrackingInfos.Count; i++)
                content+= TrackingInfos[i].ra.ToString("N6")+"\t"+TrackingInfos[i].dec.ToString("N6")+"\t"+TrackingInfos[i].cra.ToString("N6")+"\t"+TrackingInfos[i].cdec.ToString("N6")+"\r\n";
            File.WriteAllText(filePath, content);
        }
    }
    private void checkBox14_CheckedChanged(object sender, EventArgs e)
    {
        if (checkBox14.Checked) return;
        writeISSLog();
    }

    // next passes data...
    int nbPasses= -1;     // if -1, then there is no data and it needs computing
    DateTime passesTime;  // time reference for pass calculations this+timeTo_H is the time of the pass...
    // next 5 passes...
    [StructLayout(LayoutKind.Sequential)] public struct PassDetails { public double timeTo_H, duration_mn, max_elevation_deg; }
    PassDetails[] passes= new PassDetails[5]; 
    [DllImport("issposdll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int GeneratePassList(double siteLat, double siteLong, double siteAltitude, [MarshalAs(UnmanagedType.LPStr)] string tle1, [MarshalAs(UnmanagedType.LPStr)] string tle2, [Out] PassDetails[] pass_list, int nbpassesMax);
    public void nextPass()
    {
        if (!hasTLE()) return;
        passesTime= DateTime.UtcNow;
        nbPasses= GeneratePassList(TelescopeHardware.SiteLatitude, TelescopeHardware.SiteLongitude, TelescopeHardware.SiteElevation/1000.0, issl1, issl2, passes, passes.Length);
        if (nbPasses==0) return;
        nbposes= FuturePos(TelescopeHardware.SiteLatitude, TelescopeHardware.SiteLongitude, TelescopeHardware.SiteElevation/1000.0, issl1, issl2, passes[0].timeTo_H, poses, poses.Length);
        var transform = new Transform();
        transform.SiteLatitude = TelescopeHardware.SiteLatitude;
        transform.SiteLongitude = TelescopeHardware.SiteLongitude;
        transform.SiteElevation = TelescopeHardware.SiteElevation;
        transform.Refraction = true;
        transform.JulianDateUTC = SharedResources.utilities.DateUTCToJulian(passesTime.AddHours(passes[0].timeTo_H));
        transform.SetAzimuthElevation(poses[0].az, poses[0].alt);
        double lra= transform.RATopocentric, ldec= transform.DECTopocentric;
        double mra=0, mdec=0;
        radecposes[0].az= lra; radecposes[0].alt= ldec; 
        for (int i=1; i<nbposes; i++)
        {
            transform.JulianDateUTC = SharedResources.utilities.DateUTCToJulian(passesTime.AddHours(passes[0].timeTo_H).AddSeconds(i));
            transform.SetAzimuthElevation(poses[i].az, poses[i].alt);
            double ra= transform.RATopocentric, dec= transform.DECTopocentric;
            double dra= Math.Abs(ra-lra); if (dra<20/15 && dra>mra) mra= dra;
            double ddec= Math.Abs(dec-ldec); if (ddec<20 && ddec>mdec) mdec= ddec;
            lra= ra; ldec= dec;
            radecposes[i].az= lra; radecposes[i].alt= ldec; 
        }
        issNextPassMaxRaSpd= mra*15; issNextPassMaxDecSpd= mdec;
    }
    private void button38_Click(object sender, EventArgs e) // ask for pass recalculation
    {
        nbPasses= -1;
    }



    [DllImport("issposdll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void currentPos(double siteLat, double siteLong, double siteAltitude, [MarshalAs(UnmanagedType.LPStr)] string tle1, [MarshalAs(UnmanagedType.LPStr)] string tle2, out double az, out double alt, out double dst, double inSecondsFromNow);
    [StructLayout(LayoutKind.Sequential)]
    public struct Tpos { public double az, alt, dst; };
    // get a list of coordinate for a pass. when is in h (get from pass list). typically 600 slots in pos is enough (10mn)
    [DllImport("issposdll.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern  int FuturePos(double siteLat, double siteLong, double siteAltitude, [MarshalAs(UnmanagedType.LPStr)] string tle1, [MarshalAs(UnmanagedType.LPStr)] string tle2, double when, [Out] Tpos[] pos, int nbpos);
    double issNextPassMaxRaSpd=0, issNextPassMaxDecSpd=0;
    Tpos[] poses= new Tpos[800]; int nbposes= 0;
    Tpos[] radecposes= new Tpos[800];

    public (double ra, double dec, double az, double alt, bool visible) GetIssRaDecFromLocation(double inHowLong_s)
    {
        if (!hasTLE())  return (0.0f, 0.0f, 0.0f, 0.0, false);
        //set calculation parameters StartTime, EndTime and caclulation steps in minutes
        double az, alt, range;
        currentPos(TelescopeHardware.SiteLatitude, TelescopeHardware.SiteLongitude, TelescopeHardware.SiteElevation/1000.0, issl1, issl2, out az, out alt, out range, inHowLong_s);

        var transform = new Transform();
        transform.SiteLatitude = TelescopeHardware.SiteLatitude;
        transform.SiteLongitude = TelescopeHardware.SiteLongitude;
        transform.SiteElevation = TelescopeHardware.SiteElevation;
        transform.Refraction = true;

        // to ra/dec
        DateTime obsTime = !SharedResources.TrackingDisabled? DateTime.UtcNow : SharedResources.trackingStopTime;
        transform.JulianDateUTC = SharedResources.utilities.DateUTCToJulian(obsTime);
        transform.SetAzimuthElevation(az, alt);
        return (transform.RATopocentric, transform.DECTopocentric, az, alt, alt>0.0);
    }
    public static string tohms(double v)
    {
        string n = "";
        if (v < 0) { n = "-"; v = -v; }
        n += ((int)v).ToString() + ":";
        v = (v - Math.Floor(v)) * 60;
        n += ((int)v).ToString() + ":";
        v = (v - Math.Floor(v)) * 60;
        n += ((int)v).ToString();
        return n;
    }
    private void FrmMain_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Up) keyState |= 1;
        if (e.KeyCode == Keys.Down) keyState |= 2;
        if (e.KeyCode == Keys.Right) keyState |= 4;
        if (e.KeyCode == Keys.Left) keyState |= 8;
        keyStateShift= e.Shift;
    }
    private void textBox22_KeyUp(object sender, KeyEventArgs e)
    {
        if (e.KeyCode == Keys.Up) keyState&= ~1;
        if (e.KeyCode == Keys.Down) keyState&= ~2;
        if (e.KeyCode == Keys.Right) keyState&= ~4;
        if (e.KeyCode == Keys.Left) keyState&= ~8;
    }


    ///////////////////////////////////////////////////////////
    /// TLE stuff
    ///////////////////////////////////////////////////////////
    string[] N2YOTLE(string number)
    {
        ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // 3072 explicitly forces TLS 1.2
        string jsonString= (new WebClient()).DownloadString("https://api.n2yo.com/rest/v1/satellite/tle/"+number+"&apiKey=59W3M5-BYK4RM-T252L2-5IUJ");
        int startIndex = jsonString.IndexOf("\"tle\":");
        if (startIndex == -1) throw new Exception("The 'tle' property was not found in the server response.");
        // Move index to where the actual TLE data begins (after "tle":" )
        startIndex += 6;
        // look for an opening "
        int endIndex = jsonString.IndexOf("\"", startIndex);
        if (endIndex == -1) throw new Exception("Malformed JSON string response. 1");
        startIndex= endIndex+1;
        // 3. Find the closing quote of the TLE string value
        endIndex = jsonString.IndexOf("\"", startIndex);
        if (endIndex == -1) throw new Exception("Malformed JSON string response.");
        // 4. Extract the substring block
        string tleBlock = jsonString.Substring(startIndex, endIndex - startIndex);
        // 5. Clean up JSON literal escape tokens. 
        // In raw text responses, literal carriage returns are encoded as "\r\n" strings.
        tleBlock = tleBlock.Replace("\\r\\n", "\n").Replace("\\n", "\n").Replace("\\r", "\n");
        // 6. Split the cleaned text block into your 2 array rows
        string[] lines = tleBlock.Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
        if (lines.Length < 2) throw new Exception("TLE string block was missing expected line array rows.");
        return new string[] { lines[0], lines[1] };
    }

    private void button38_MouseDown(object sender, MouseEventArgs e)
    {
        if (e.Button == MouseButtons.Middle) makeIssVisible= true;
    }

    private void button2_Click(object sender, EventArgs e)
    {
        if (nbPasses<=0) return;
        TelescopeHardware.SlewToCoordinatesAsync(radecposes[0].az, radecposes[0].alt);
    }

    private void button1_Click(object sender, EventArgs e)
    {
        if (textBox23.Text.Length==0) // case of iss
        { 
            try
            {
                var lines= N2YOTLE("25544");
                issl1= lines[0]; issl2= lines[1];
                TelescopeHardware.saveisstls(issl1, issl2);
                labelErr.Text= "ISS TLE OK";
                SharedResources.doLog(labelErr.Text+"\r\n"+issl1+"\r\n"+issl2, 4);
                checkBox14.Enabled= true; return;
            }
            catch { labelErr.Text = "exception on ISS load"; SharedResources.doLog(labelErr.Text, 4); }
            SharedResources.doLog("ISS: use saved tle", 4);
            labelErr.Text = "ISS use saved TLE";
            issl1 = SharedResources.isstle1;
            issl2= SharedResources.isstle2;
            if (issl1 == null || issl1.Length == 0) { labelErr.Text = "ISS TLE not found."; }
        }
        else { // satellite
            try
            {
                var lines= N2YOTLE(textBox23.Text);
                issl1= lines[0]; issl2= lines[1];
                SharedResources.doLog(lines.ToString(), 4); labelErr.Text = textBox23.Text + " TLE OK!";

            }
            catch { labelErr.Text = "exception on TLE load"; SharedResources.doLog(labelErr.Text, 4); } // could not find. uncheck check box...
        }
        checkBox14.Enabled= issl1!=null && issl1.Length!=0;
    }

    bool hasTLE() {  return issl1!=null && issl1.Length!=0; }

}
