using ASCOM.DeviceInterface;
using ASCOM.EQControl.Focuser.V1;
using ASCOM.EQControl.Telescope.V1;
using ASCOM.Utilities;
using StarDisp;
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Media;
using System.Windows.Forms;
using static ASCOM.LocalServer.SharedResources;
using ASCOM.Astrometry.AstroUtils;

namespace ASCOM.LocalServer
{
    public partial class FrmMain : Form, SharedResources.ILog
    {
        private delegate void SetTextCallback(string text);

        Focuser f = new Focuser();
        Telescope T = new Telescope();
        private System.Timers.Timer timerPos;
        private string tver;
        public FrmMain()
        {
            InitializeComponent();
            this.ShowInTaskbar = true;
            this.Visible = true;
            label1.Text = label1.Text + " " + FocuserHardware.DriverVersion;
            button2.Text = f.Connected ? "Disconnect" : "Connect";
            rescanPort();
            timerPos = new System.Timers.Timer(500);
            timerPos.Elapsed += (source, e) => { updateConnectedLabel(); };
            timerPos.Enabled = true;
            textBox2.Text = FocuserHardware.fastSpeed.ToString();
            textBox5.Text = FocuserHardware.slowSpeed.ToString();
            tver= TelescopeHardware.DriverVersion;
            resetHWSetupFields();
            SharedResources.log= this;
            rbMessier.Checked= true; rbMessier_CheckedChanged(null, null);
            Width = groupBox2.Width+groupBox2.Left*2+Width-ClientSize.Width;
            Height= groupBox2.Height+groupBox2.Top+groupBox2.Left+Height-ClientSize.Height;
            calcSteps();
            checkBox7.Checked= SharedResources.guideAfterSlew;
            checkBox19.Checked= SharedResources.reconnectOnDrop;
            checkBox20.Checked= SharedResources.parkAtSunrise;
            updateLocations();  posCB.Text= "";
            updateSavedPos();
            phd2GuideDelay.Text= SharedResources.phd2GuideDelay.ToString();
            labelBellowHorizon.Text= "";
        }
        ~FrmMain() { SharedResources.log= null; SharedResources.finish= true;  }
        private int lastFocusPos= 0x7fffffff;
        bool lastPowerBit= false; int lastPowerCount= 1;
        bool no_send_track_change= false;
        AstroUtils astroUtils = new AstroUtils();

        private void updateConnectedLabel() // every 500ms, update UI based on driver's data
        {
            try
            {
                BeginInvoke((MethodInvoker)delegate ()
                {
                    if (comboBox1.SelectedIndex == -1) comboBox1.SelectedIndex = 3;
                    labelCom.Text = SharedResources.comPort;
                    if (SharedResources.Connected)
                    {
                        button2.Text = "Disconnect";
                        if (lastFocusPos!=SharedResources.FocusserPosition) 
                        { 
                            lastFocusPos= SharedResources.FocusserPosition; 
                            if (!checkBox12.Checked)
                                textBox1.Text = SharedResources.FocusserPosition.ToString();
                            else
                                textBox1.Text = ((SharedResources.FocusserPosition-SharedResources.focMaxStp/2)*SharedResources.FocStepdum/10000.0f).ToString("N1");
                        }

                        int lastHB= DateTime.Now.Subtract(SharedResources.lastHeartBeat).Seconds;
                        if (lastHB<5) groupMount.Text= "Mount";
                        else groupMount.Text= "Mount ("+lastHB.ToString()+"s old)";

                        labelDec.Text = "Declinaison: "+SharedResources.raToText(SharedResources.Declinaison);
                        labelRa.Text = "Right Assension: "+SharedResources.raToText(SharedResources.RightAssension);
                        button1.Text = SharedResources.FocusMoving ? "Stop" : "Goto"; button1.Enabled= true;
                        textBox1.Enabled = !SharedResources.FocusMoving;
                        button3.Text = SharedResources.ScopeMoving ? "Stop" : "Goto"; button3.Enabled = true;
                        textBox3.Enabled = !SharedResources.ScopeMoving;
                        textBox4.Enabled = !SharedResources.ScopeMoving;
                        button4.Enabled = !SharedResources.ScopeMoving;
                        button7.Enabled = true;
                        button8.Enabled = true;
                        button9.Enabled = true;
                        button10.Enabled = true;
                        button11.Enabled = true;
                        button34.Enabled = true;
                        SideralSelect.Enabled = true;
                        button21.Enabled = true;
                        button12.Enabled = true;
                        button22.Enabled = true;
                        button23.Enabled = true;
                        button24.Enabled = true;
                        button25.Enabled = true;
                        MovingLabel.Text = SharedResources._ScopeGuiding ? "Guide" : (SharedResources._ScopeMoving ? "Slewing": "");
                        button12.Text = "Mount " + (TelescopeHardware.SideOfPier == PierSide.pierEast ? "East" : "West");
                        if (!SideralSelect.DroppedDown)
                        { 
                            no_send_track_change= true;
                            if (SideralSelect.SelectedIndex==-1) SideralSelect.SelectedIndex= SharedResources.TrackingDisabled?0:(SharedResources.ascomtrackspd+1);
                            if (SharedResources.TrackingDisabled) if (SideralSelect.SelectedIndex!=0) SideralSelect.SelectedIndex= 0;
                            if (!SharedResources.TrackingDisabled) if (SideralSelect.SelectedIndex!=SharedResources.ascomtrackspd+1) SideralSelect.SelectedIndex= SharedResources.ascomtrackspd+1;
                            no_send_track_change= false;
                        }
                        button21.Text= SharedResources.FlipDisabled ? "Flip disabled" : "Flip enable";
                        if (!hasHWPos) label50.Visible= SharedResources.meridianFlip;
                        else {
                            label50.Visible= true;
                            if (SharedResources.meridianFlip) label50.Text= "Flipping";
                            else
                            {
                                int rat= SharedResources.raMaxPos;
                                int maxRa= (int)((Int64)SharedResources.raMaxPos*SharedResources.raAmplitude/360); if (maxRa==0) maxRa= 1;
                                int left= maxRa-SharedResources.raPos;
                                int TimeS= (int)(((double)left)/rat*24*3600);
                                label50.Text= "Flip in "+SharedResources.raToText(TimeS);
                            }
                        }

                        if (SharedResources.timeSpanPC != 0) labelPCTime.Text = "PCTime:" + SharedResources.timeSpanPC.ToString();
                        if (SharedResources.timeSpanHW != 0) labelHWTime.Text = "HWTime:" + SharedResources.timeSpanHW.ToString();
                        if (SharedResources.timeSpanPC != 0 && SharedResources.timeSpanHW != 0)
                        { 
                            labelDriftTime.Text = ((SharedResources.timeSpanHW - SharedResources.timeSpanPC) * 100.0 / SharedResources.timeSpanPC).ToString("0.##") + "% drift";
                            double stepsPerS= (SharedResources.timeSpanUncountedSteps*1000.0/SharedResources.timeSpanPC);
                            UncountedPerHouse.Text= stepsPerS.ToString("N3");
                            double er= (stepsPerS-SharedResources.raMaxPos/(23*3600+56*60+4.0))/stepsPerS;
                            labelSideralEr.Text= (er*100).ToString("N5")+"%";
                        }
                        labelGuiding.Text = SharedResources._ScopeGuiding ? "Guiding" : "";

                        if (groupBox8.Visible) drawStars();

                        if (SharedResources.hasPowerCount)
                        { 
                            checkBox11.Text= "Yell on power ("+(SharedResources.powerBit?"On":"Off")+" cnt:"+powerCount.ToString()+')';
                            if (checkBox11.Checked)
                                if ((lastPowerBit && !SharedResources.powerBit) || // detect a drop in power...
                                    (SharedResources.powerBit && lastPowerBit && lastPowerCount!=SharedResources.powerCount)) // no drop in power detected, BUT power count changed
                                    try { SoundPlayer player = new SoundPlayer(@"power.wav"); player.Play(); } catch { }
                            lastPowerBit= SharedResources.powerBit; lastPowerCount= SharedResources.powerCount;
                        }
                        if (SharedResources.hasHWPos)
                        {
                            int maxRa= (int)((Int64)SharedResources.raMaxPos*SharedResources.raAmplitude/360); if (maxRa==0) maxRa= 1;
                            label41.Text = "ra "+SharedResources.raPos.ToString() + " / " + maxRa.ToString() + " " + ((SharedResources.raPos*100)/maxRa).ToString() + "%";
                            int decMaxPos= SharedResources.decMaxPos; if (decMaxPos==0) decMaxPos= 1;
                            label42.Text = "dec "+SharedResources.decPos.ToString() + " / " + (SharedResources.decMaxPos / 2).ToString() + " " + (SharedResources.decPos * 200 / decMaxPos).ToString() + "%";
                            int focMaxPos= SharedResources.focMaxStp; if (focMaxPos==0) focMaxPos= 1;
                            label52.Text = "foc "+SharedResources._FocusserPosition.ToString() + " / " + focMaxPos.ToString() + " " + (SharedResources._FocusserPosition * 100 / focMaxPos).ToString() + "%";

                        }
                        TestCycle(); // deal with testing
                        if (!FreezeLastResponse.Checked && groupBox7.Visible)
                        {
                            lastRep1.Text= SharedResources.latestResponse1;
                            lastRep2.Text= SharedResources.latestResponse2;
                            NbResponses.Text= SharedResources.responceCount.ToString();
                        }
                        if (SharedResources.BNOhas)
                        {
                            BNO0.Text = "Tmp:"+SharedResources.BNOTemp.ToString()+"° "+(SharedResources.BNOhasOffset1?"O1":"")+" "+(SharedResources.BNOscopeEast?"E":"W");
                            BNO1.Text = SharedResources.BNOw.ToString("F4")+" "+SharedResources.BNOx.ToString("F4")+" "+SharedResources.BNOy.ToString("F4")+" "+SharedResources.BNOz.ToString("F4");
                            BNO2.Text = SharedResources.BNOra.ToString("F4")+" "+SharedResources.BNOdec.ToString("F4")+" "+SharedResources.BNOaz.ToString("F4")+" "+SharedResources.BNOalt.ToString("F4");
                        }
                        if (!textBox21.Focused)
                            textBox21.Text= SharedResources.getSunRaiseTime().ToString("HH:mm");

                        var result = GetSettingTimeUtc(TelescopeHardware.RightAscension, TelescopeHardware.Declination, 0, TelescopeHardware.SiteLatitude, TelescopeHardware.SiteLongitude, DateTime.UtcNow);
                        if (result.IsCircumpolar) labelBellowHorizon.Text= "Circumpolar";
                        else if (result.NeverRises) labelBellowHorizon.Text= "Never rises";
                        else labelBellowHorizon.Text= $"Object sets below horizon at : {result.SettingTimeUtc.ToLocalTime():HH:mm:ss}";

                    }
                    else
                    {
                        labelBellowHorizon.Text= "";
                        groupMount.Text= "Mount";
                        button2.Text = "Connect";
                        textBox1.Text = ""; textBox1.Enabled = false;
                        textBox4.Text = ""; textBox4.Enabled = false;
                        textBox3.Text = ""; textBox3.Enabled = false;
                        button1.Text = "Goto"; button1.Enabled = false;
                        button3.Text = "Stop"; button3.Enabled= false;
                        button4.Enabled = false;
                        labelPCTime.Text = "PCTime:N/A";
                        labelHWTime.Text = "HWTime:N/A";
                        labelDriftTime.Text = "% drift:N/A";
                        button7.Enabled= false;
                        button8.Enabled = false;
                        button9.Enabled = false;
                        button10.Enabled = false;
                        button11.Enabled = false;
                        button34.Enabled = false;
                        SideralSelect.Enabled = false;
                        button21.Enabled = false;
                        button12.Enabled = false;
                        button22.Enabled = false;
                        button23.Enabled = false;
                        button24.Enabled = false;
                        button25.Enabled = false;
                        lastFocusPos= 0;
                        labelGuiding.Text = "";
                        checkBox11.Text= "Yell on power off";
                        label41.Text = "N/A";
                        label42.Text = "N/A";
                        label21.Text = "N/A";
                        BNO0.Text = "NO BNO"; BNO1.Text = ""; BNO2.Text = "";
                        MovingLabel.Text = "";
                        textBox21.Text= "";
                        if (SharedResources.serialCrahed && SharedResources.reconnectOnDrop)
                        {
                            var l= (new Serial()).AvailableCOMPorts;
                            if (l.Contains(SharedResources.comPort))
                            { 
                                try { 
                                    log("Try reconnecting to "+SharedResources.comPort, 0);
                                    SharedResources.Connected = true;
                                    log("Reconnected!!!", 0);
                                } catch (Exception ) { SharedResources.serialCrahed=true; }
                            }

                        }
                    }
                    resetHWSetupFields();
                });
            } 
            catch (Exception ) { }
        }
        private void button2_Click(object sender, EventArgs e)
        {
            if (SharedResources.Connected) SharedResources.Disconnect();
            else SharedResources.Connected = true;
        }
        private void focusTo()
        {
            if (!checkBox12.Checked)
            {
                int v; if (int.TryParse(textBox1.Text, out v)) f.Move(v);
            } else
            {
                double v; if (double.TryParse(textBox1.Text, out v))
                {
                    v = (v*10000 / SharedResources.FocStepdum) + SharedResources.focMaxStp/2;
                    f.Move((int)v);
                }
            }
        }
        private void button1_Click(object sender, EventArgs e) // focus to
        {
            if (SharedResources.FocusMoving) f.Stop();
            else focusTo();
        }
        private void scopeTo()
        {
            bool ok;
            int ra = SharedResources.fromHms(textBox3.Text, out ok); if (!ok) return;
            int dec = SharedResources.fromHms(textBox4.Text, out ok); if (!ok) return;
            T.SlewToCoordinatesAsync(ra/3600.0, dec/3600.0);
        }
        private void button3_Click(object sender, EventArgs e) // go to
        {
            if (SharedResources.ScopeMoving) T.AbortSlew();
            else scopeTo();
        }
        private void button4_Click(object sender, EventArgs e) // sync to
        {
            if (SharedResources.ScopeMoving) return;
            bool ok;
            int ra = SharedResources.fromHms(textBox3.Text, out ok); if (!ok) return;
            int dec = SharedResources.fromHms(textBox4.Text, out ok); if (!ok) return;
            T.SyncToCoordinates(ra / 3600.0, dec / 3600.0);
        }

        private void comboBoxComPort_SelectedIndexChanged(object sender, EventArgs e)
        {
            SharedResources.comPort= comboBoxComPort.SelectedItem.ToString();
            TelescopeHardware.saveProfile();
            FocuserHardware.saveProfile();
        }
        private bool setupVisible = false;
        private void rewidth(bool grow)
        {
            if (grow)
                Width= groupBox10.Width+groupBox10.Left+groupBox2.Left+Width-ClientSize.Width;
            else
                Width= groupBox2.Width+groupBox2.Left*2+Width-ClientSize.Width;
        }
        private void button5_Click(object sender, EventArgs e) // Setup visible/invisible...
        {
            setupVisible = !setupVisible; rewidth(setupVisible); 
            resetHWSetupFields();
            groupBox7.Visible= false;
            groupBox10.Visible= true;
            groupBox8.Visible= false;
        }
        private void button15_Click(object sender, EventArgs e)
        {
            setupVisible = !setupVisible; rewidth(setupVisible); 
            groupBox7.Visible= setupVisible;
            groupBox10.Visible= false;
            groupBox8.Visible= false;
        }
        private void button17_Click(object sender, EventArgs e)
        {
            setupVisible = !setupVisible; rewidth(setupVisible); 
            groupBox8.Visible= true;
            groupBox7.Visible= false;
            groupBox10.Visible= false;
        }

        private void rescan_Click(object sender, EventArgs e) { rescanPort(); }
        void rescanPort()
        {
            // set the list of COM ports to those that are currently available
            comboBoxComPort.Items.Clear(); // Clear any existing entries
            using (Serial serial = new Serial()) // User the Se5rial component to get an extended list of COM ports
                comboBoxComPort.Items.AddRange(serial.AvailableCOMPorts);
            // select the current port if possible
            if (comboBoxComPort.Items.Contains(SharedResources.comPort))
                comboBoxComPort.SelectedItem = SharedResources.comPort;
        }

        void resetHWSetupFields()
        {
            if (SharedResources.hasHWData)
            {
                if (!SharedResources.dataDisplayed)
                { 
                    SharedResources.dataDisplayed= true;
                    raMaxPos.Text = SharedResources.raMaxPos.ToString();
                    rasps.Text= (360*3600.0/SharedResources.raMaxPos).ToString("N2")+"\"/stp";
                    raMaxSpd.Text = SharedResources.raMaxSpeed.ToString();
                    radps.Text= (360.0*SharedResources.raMaxSpeed/SharedResources.raMaxPos).ToString("N1")+"°/s";
                    ramsToSpd.Text = SharedResources.ramsToSpeed.ToString();
                    decMaxPos.Text = SharedResources.decMaxPos.ToString();
                    decsps.Text= (360*3600.0/SharedResources.decMaxPos).ToString("N2")+"\"/stp";
                    decMaxSpd.Text = SharedResources.decMaxSpeed.ToString();
                    decdps.Text= (360.0*SharedResources.decMaxSpeed/SharedResources.decMaxPos).ToString("N1")+"°/s";
                    decMsToSpd.Text = SharedResources.decmsToSpeed.ToString();
                    timeComp.Text= SharedResources.timeComp.ToString();
                    AutoMeridianFlip.Checked= (SharedResources.guidingBits&0x80)==0;

                    SiteLatitude.Text = SharedResources.raToText(((Double)SharedResources.Latitude)/10/3600);
                    SiteElevation.Text = SharedResources.SiteAltitude.ToString();
                    SiteLongitude.Text = SharedResources.raToText(((Double)SharedResources.Longitude)/10/3600);
                    FocalLength.Text = SharedResources.FocalLength.ToString();
                    Area.Text = SharedResources.Area_cm2.ToString();
                    Aperture.Text = SharedResources.Diameter_mm.ToString();
                    StepSize.Text = (SharedResources.FocStepdum/10.0f).ToString();

                    FocMaxAcc.Text = SharedResources.focAcc.ToString();
                    FocMaxPos.Text = SharedResources.focMaxStp.ToString();
                    FocMaxSpd.Text = SharedResources.focMaxSpd.ToString();
                    label36.Text = (SharedResources.focMaxStp * SharedResources.FocStepdum / 10000).ToString() + "mm";
                    textBox18.Text = (360 * 3600 * (Int64)SharedResources.decBacklash / SharedResources.decMaxPos).ToString();
                    textBox14.Text = (360 * 3600 * (Int64)SharedResources.raBacklash / SharedResources.raMaxPos).ToString();
                    textBox19.Text = SharedResources.focBacklash.ToString();
                    //textBox21.Text = (360 * 3600 * (Int64)SharedResources.raSettle / SharedResources.raMaxPos).ToString();
                    RAMaxMovement.Text= SharedResources.raAmplitude.ToString();
                    groupBox2.Visible= SharedResources.focMaxSpd != 0;

                    textBox6.Text = (SharedResources.guideRateDec/10.0f).ToString("N1");
                    textBox15.Text = (SharedResources.guideRateRA/10.0f).ToString("N1");
                    checkBox4.Checked = (SharedResources.invertAxes & 2) != 0;
                    checkBox5.Checked = (SharedResources.invertAxes & 1) != 0;
                    checkBox6.Checked = (SharedResources.invertAxes & 4) != 0;

                    checkBox9.Checked = (SharedResources.guidingBits & 1) != 0;
                    checkBox10.Checked = (SharedResources.guidingBits & 8) != 0;
                    checkBox2.Checked = (SharedResources.guidingBits & 2) != 0;
                    checkBox3.Checked = (SharedResources.guidingBits & 16) != 0;
                    raGuideStop.Checked = (SharedResources.guidingBits & 4) != 0;
                    decGuideStop.Checked = (SharedResources.guidingBits & 32) != 0;

                    label53.Enabled= label54.Enabled= label55.Enabled= textBox25.Enabled= textBox24.Enabled= textBox26.Enabled= SharedResources.haswifi;
                    textBox24.Text= SharedResources.wifi;
                    textBox25.Text= SharedResources.wifip;
                    checkBox18.Checked = (SharedResources.guidingBits&0x40)==0;
                    String s= ""; for (int i=3;  i>=0; i--) s+= (((SharedResources.ipaddr)>>(8*i))&0xff).ToString()+'.';
                    textBox26.Text= s.Substring(0, s.Length-1);
                    groupBox4.Text= "Telescope && observatory"+(hasGpsInfo?" (GPS)":"");
                }
            } else
            {
                raMaxPos.Text = "N/A";
                raMaxSpd.Text = "N/A";
                ramsToSpd.Text = "N/A";
                decMaxPos.Text = "N/A";
                decMaxSpd.Text = "N/A";
                decMsToSpd.Text = "N/A";
                timeComp.Text = "N/A";

                SiteLatitude.Text = "N/A";
                SiteElevation.Text = "N/A";
                SiteLongitude.Text = "N/A";
                FocalLength.Text = "N/A";
                Area.Text = "N/A";
                Aperture.Text = "N/A";
                StepSize.Text = "N/A";
                FocMaxAcc.Text= "N/A";
                FocMaxPos.Text = "N/A";
                FocMaxSpd.Text = "N/A";
                textBox14.Text = "N/A";
                RAMaxMovement.Text = "N/A";
                groupBox4.Text= "Telescope && observatory";

                textBox6.Text = "N/A";
                textBox15.Text = "N/A";

                label36.Text = "";
                rasps.Text =  "";
                radps.Text =  "";
                decsps.Text =  "";
                decdps.Text = "";
                checkBox4.Checked = checkBox5.Checked = checkBox6.Checked = false;
                textBox19.Text= "N/A";
                textBox18.Text= "N/A";
                label53.Enabled= label54.Enabled= label55.Enabled= textBox25.Enabled= textBox24.Enabled= textBox26.Enabled= false;
            }
        }
        private void button6_Click(object sender, EventArgs e)
        {
            SharedResources.dataDisplayed = false;
            SharedResources.hasHWData= false;
            resetHWSetupFields();
        }

        private void button5_Click_1(object sender, EventArgs e)
        {
            int raMaxPos = 0, raMaxSpeed = 0, ramsToSpeed = 0, decMaxPos = 0, decMaxSpeed = 0, decmsToSpeed = 0, timeComp = 0;
            int Latitude = 0, Longitude = 0, SiteAltitude = 0, FocalLength = 0, Diameter_mm = 0, Area_cm2= 0;
            int focMaxStp = 0, focMaxSpd = 0, focAcc = 0;
            int decBacklash = 0, raAmplitude = 0;
            int raBacklash = 0, focBacklash = 0, raSettle= 0;
            if (int.TryParse(this.raMaxPos.Text, out raMaxPos) && int.TryParse(this.raMaxSpd.Text, out raMaxSpeed) && int.TryParse(this.ramsToSpd.Text, out ramsToSpeed) &&
                int.TryParse(this.decMaxPos.Text, out decMaxPos) && int.TryParse(this.decMaxSpd.Text, out decMaxSpeed) && int.TryParse(this.decMsToSpd.Text, out decmsToSpeed) &&
                int.TryParse(this.timeComp.Text, out timeComp))
            {
                bool b1, b2;
                Latitude = (int)(SharedResources.fromHms2(SiteLatitude.Text, out b1) * 10);
                Longitude = (int)(SharedResources.fromHms2(SiteLongitude.Text, out b2) * 10);
                double foxstp, decGuideRate, raGuideRate;
                if (b1 && b2)
                {
                    if (int.TryParse(SiteElevation.Text, out SiteAltitude) && int.TryParse(this.FocalLength.Text, out FocalLength) && int.TryParse(Aperture.Text, out Diameter_mm) &&
                        int.TryParse(Area.Text, out Area_cm2) && double.TryParse(StepSize.Text, out foxstp) &&
                        int.TryParse(FocMaxAcc.Text, out focAcc) && int.TryParse(this.FocMaxPos.Text, out focMaxStp) && int.TryParse(FocMaxSpd.Text, out focMaxSpd) &&
                        int.TryParse(textBox18.Text, out decBacklash) && int.TryParse(RAMaxMovement.Text, out raAmplitude) &&
                        double.TryParse(textBox6.Text, out decGuideRate) && double.TryParse(textBox15.Text, out raGuideRate) &&
                        int.TryParse(textBox14.Text, out raBacklash) && int.TryParse(textBox19.Text, out focBacklash) )
                    {
                        SharedResources.raMaxPos = raMaxPos; SharedResources.raMaxSpeed = raMaxSpeed; SharedResources.ramsToSpeed =ramsToSpeed; SharedResources.decMaxPos = decMaxPos; SharedResources.decMaxSpeed = decMaxSpeed; SharedResources.decmsToSpeed = decmsToSpeed; SharedResources.timeComp = timeComp;
                        SharedResources.Latitude = Latitude; SharedResources.Longitude= Longitude; SharedResources.SiteAltitude= SiteAltitude; SharedResources.updateAzimutal();
                        SharedResources.FocalLength= FocalLength; SharedResources.Diameter_mm= Diameter_mm; SharedResources.Area_cm2= Area_cm2; SharedResources.FocStepdum = (int)(foxstp * 10);
                        SharedResources.focMaxStp = focMaxStp; SharedResources.focMaxSpd = focMaxSpd; SharedResources.focAcc = focAcc;
                        if (SharedResources.focMaxStp>65535) SharedResources.focMaxStp= 65535;
                        SharedResources.decBacklash= (int)((decBacklash*(Int64)decMaxPos)/(360*3600)); SharedResources.raAmplitude = raAmplitude;
                        SharedResources.guideRateDec = (int)(decGuideRate*10.0f);
                        if (SharedResources.guideRateDec>=256) SharedResources.guideRateDec= 255;
                        SharedResources.guideRateRA = (int)(raGuideRate*10.0f);
                        if (SharedResources.guideRateRA>=256) SharedResources.guideRateRA= 255;
                        SharedResources.invertAxes = (checkBox4.Checked ? 2 : 0) | (checkBox5.Checked ? 1 : 0) | (checkBox6.Checked ? 4 : 0);
                        SharedResources.raBacklash = (int)((raBacklash * (Int64)raMaxPos) / (360 * 3600));
                        SharedResources.raSettle = (int)((raSettle * (Int64)raMaxPos) / (360*3600));
                        SharedResources.focBacklash = focBacklash;
                        SharedResources.guidingBits = (checkBox9.Checked ? 1 : 0) + (checkBox2.Checked ? 2 : 0) + (raGuideStop.Checked ? 4 : 0) +
                                                      (checkBox10.Checked ? 8 : 0) + (checkBox3.Checked ? 16 : 0) + (decGuideStop.Checked ? 32 : 0) +
                                                      (checkBox18.Checked ? 0 : 0x40) + // = (SharedResources.guidingBits&0x40)!=0;
                                                      (AutoMeridianFlip.Checked?0:0x80);
;
                        SharedResources.wifi= textBox24.Text; SharedResources.wifip= textBox25.Text;
                        SharedResources.updateHW();
                    }
                }
            }
            SharedResources.hasHWData = false; SharedResources.dataDisplayed= false; // force reask...
        }

        private void button7_Click(object sender, EventArgs e)
        {
            textBox4.Text = SharedResources.raToText(SharedResources.Declinaison);
            textBox3.Text = SharedResources.raToText(SharedResources.RightAssension);
        }

        // fast in
        int focSpeedToUnit(int s) // if foc in steps, returns s. else transforms s from microns/s into steps/s
        {
            if (!checkBox12.Checked) return s;
            return s * 10 / SharedResources.FocStepdum;
        }
        private void button8_MouseDown(object sender, MouseEventArgs e) { FocuserHardware.moveIn(focSpeedToUnit(FocuserHardware.fastSpeed)); }
        private void button8_MouseLeave(object sender, EventArgs e) { FocuserHardware.Stop(); }
        private void button8_MouseUp(object sender, MouseEventArgs e) { FocuserHardware.Stop(); }
        // fase out
        private void button9_MouseDown(object sender, MouseEventArgs e) { FocuserHardware.moveOut(focSpeedToUnit(FocuserHardware.fastSpeed)); }
        private void button9_MouseLeave(object sender, EventArgs e) { FocuserHardware.Stop(); }
        private void button9_MouseUp(object sender, MouseEventArgs e) { FocuserHardware.Stop(); }

        private void button11_MouseDown(object sender, MouseEventArgs e) { FocuserHardware.moveIn(focSpeedToUnit(FocuserHardware.slowSpeed)); }
        private void button11_MouseLeave(object sender, EventArgs e) { FocuserHardware.Stop(); }
        private void button11_MouseUp(object sender, MouseEventArgs e) { FocuserHardware.Stop(); }
        private void button10_MouseDown(object sender, MouseEventArgs e) { FocuserHardware.moveOut(focSpeedToUnit(FocuserHardware.slowSpeed)); }
        private void button10_MouseLeave(object sender, EventArgs e) { FocuserHardware.Stop(); }
        private void button10_MouseUp(object sender, MouseEventArgs e) { FocuserHardware.Stop(); }

        private void textBox2_TextChanged(object sender, EventArgs e)
        {
            int i;
            if (int.TryParse(textBox2.Text, out i)) if (FocuserHardware.fastSpeed!=i) { FocuserHardware.fastSpeed = i; FocuserHardware.saveProfile(); }
        }

        private void textBox5_TextChanged(object sender, EventArgs e)
        {
            int i;
            if (int.TryParse(textBox5.Text, out i)) if (FocuserHardware.slowSpeed != i) { FocuserHardware.slowSpeed = i; FocuserHardware.saveProfile(); }
        }

        private void button12_Click(object sender, EventArgs e)
        {
            TelescopeHardware.SideOfPier = TelescopeHardware.SideOfPier == PierSide.pierEast ? PierSide.pierWest : PierSide.pierEast;
        }
        public void log(string message, int source)
        {
            if (groupBox7.Visible)
            try
            {
                    if (
                        (source==-1 && checkboxlogsystem.Checked) ||
                        (source==0 && checkboxascom.Checked) ||
                        (source==1 && checkBox8.Checked) ||   // frequent ascom
                        (source==2 && checkBox1.Checked) ||   // serial commands
                        (source==3 && checkBox15.Checked) ||  // phd2
                        (source==4 && checkBox16.Checked))    // iss
                     BeginInvoke((MethodInvoker)delegate () { logBox.AppendText(message + "\r\n"); });
            } catch (Exception ) { }
            
        }

        private void button14_Click(object sender, EventArgs e)
        {
            logBox.Text= "";
        }

        private void button16_Click(object sender, EventArgs e)
        {
            timeComp.Text= ((((Int64)(SharedResources.timeSpanPC-SharedResources.timeSpanHW))<<24)/SharedResources.timeSpanPC).ToString();
        }

        public class CatalogListItem
        {
            string text;
            public int ra, dec;
            public override string ToString() { return text; }
            public CatalogListItem(string _text, int _ra, int _dec) { text= _text; ra= _ra; dec= _dec; }
        }
        private void rbMessier_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbMessier.Checked) return;
            rbPlanets.Checked= rbC.Checked= rbNgc.Checked= rbStars.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Messier.Length; i++)
                catalog.Items.Add(new CatalogListItem("M" + Ngcs.Messier[i].id.ToString(), Ngcs.Messier[i].ra, Ngcs.Messier[i].dec));
        }

        private void rbC_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbC.Checked) return;
            rbPlanets.Checked= rbMessier.Checked= rbNgc.Checked= rbStars.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Cadwell.Length; i++)
                catalog.Items.Add(new CatalogListItem("C" + Ngcs.Cadwell[i].id.ToString(), Ngcs.Cadwell[i].ra, Ngcs.Cadwell[i].dec));
        }

        private void rbNgc_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbNgc.Checked) return;
            rbPlanets.Checked= rbMessier.Checked= rbC.Checked= rbStars.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Ngc.Length; i++)
                catalog.Items.Add(new CatalogListItem("Ngc" + Ngcs.Ngc[i].id.ToString(), Ngcs.Ngc[i].ra, Ngcs.Ngc[i].dec));
        }

        private void rbStars_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbStars.Checked) return;
            rbPlanets.Checked= rbMessier.Checked= rbC.Checked= rbNgc.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Stars.Length; i++)
                catalog.Items.Add(new CatalogListItem(Ngcs.Stars[i].id, Ngcs.Stars[i].ra, Ngcs.Stars[i].dec));
        }
        private void rbPlanets_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbPlanets.Checked) return;
            rbStars.Checked= rbMessier.Checked= rbC.Checked= rbNgc.Checked= false;
            catalog.Items.Clear();
            DateTime currentDateTime = DateTime.Now;
            Ngcs.year= currentDateTime.Year; Ngcs.day= currentDateTime.Day; Ngcs.month= currentDateTime.Month;
            for (int i=0; i<8; i++)
            {
                int ra, dec;
                Ngcs.planetPos(i, out ra, out dec);
                catalog.Items.Add(new CatalogListItem(Ngcs.planetNames[i], ra, dec));
            }
        }

        private bool catalogCoord(out double ra, out double dec)
        {
            ra= dec= 0.0;
            if (catalog.SelectedItem!=null)
            { 
                CatalogListItem item = (CatalogListItem)catalog.SelectedItem; 
                if (item!=null)
                { 
                    ra= item.ra/3600.0; dec= item.dec/3600.0;
                    return true;
                }
            }
            if (label40.Text!="" && av.clickra!=-100000 && av.clickdec!=-100000) { ra= av.clickra/3600.0; dec= av.clickdec/3600.0; return true; }
            return false;
        }
        private void button18_Click(object sender, EventArgs e) // Goto catalog
        {
            double ra, dec; if (!catalogCoord(out ra, out dec)) return;
            T.SlewToCoordinatesAsync(ra, dec);
        }

        private void button19_Click(object sender, EventArgs e)
        {
            double ra, dec; if (!catalogCoord(out ra, out dec)) return;
            T.SyncToCoordinates(ra, dec);
        }

        private void SideralSelect_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (no_send_track_change) return;
            if (SideralSelect.SelectedIndex==0) SharedResources.Track(false, SharedResources.ascomtrackspd); // stopped
            if (SideralSelect.SelectedIndex==1) SharedResources.Track(true, 0); // sideral
            if (SideralSelect.SelectedIndex==2) SharedResources.Track(true, 1); // moon 
            if (SideralSelect.SelectedIndex==3) SharedResources.Track(true, 2); // sun
            if (SideralSelect.SelectedIndex==4) SharedResources.Track(true, 3); // sun
        }

        private void button21_Click(object sender, EventArgs e)
        {
            SharedResources.FlipDisabled= !SharedResources.FlipDisabled;
        }
        private double moveSpeed()
        {
            if (comboBox1.SelectedIndex==0) return 60f/3600f; // 1'
            if (comboBox1.SelectedIndex==1) return 300f/3600f; // 5'
            if (comboBox1.SelectedIndex==2) return 600f/3600f; // 10'
            if (comboBox1.SelectedIndex==3) return 0.5; // 30'
            if (comboBox1.SelectedIndex==4) return 1f; // 1°
            if (comboBox1.SelectedIndex==5) return 2f; // 2°
            return 0.5;
        }
        private void button22_MouseDown(object sender, MouseEventArgs e)
        {
            T.MoveAxis(TelescopeAxes.axisSecondary, moveSpeed());
        }
        private void button23_MouseDown(object sender, MouseEventArgs e)
        {
            T.MoveAxis(TelescopeAxes.axisSecondary, -moveSpeed());
        }
        private void button24_MouseDown(object sender, MouseEventArgs e)
        {
            T.MoveAxis(TelescopeAxes.axisPrimary, moveSpeed());
        }
        private void button25_MouseDown(object sender, MouseEventArgs e)
        {
            T.MoveAxis(TelescopeAxes.axisPrimary, -moveSpeed());
        }
        private void button24_MouseUp(object sender, MouseEventArgs e)
        {
            SharedResources.SendSerialCommand(":Q#",0);
        }
        private static string decode(string s, int pos, int length, string txt)
        {
            txt+= ":";
            int v= 0;
            for (int i = pos; i<pos+length; i++)
            {
                int v2= 0;
                if (s[i*2+1]>='0' && s[i*2+1]<='9') v2+= s[i*2+1]-'0';
                if (s[i*2+1]>='A' && s[i*2+1]<='F') v2+= s[i*2+1]-'A'+10;
                if (s[i*2]>='0' && s[i*2]<='9') v2+= (s[i*2]-'0')*16;
                if (s[i*2]>='A' && s[i*2]<='F') v2+= (s[i*2]-'A'+10)*16;
                v+= v2<<((i-pos)*8);
            }
            txt+= v.ToString()+" ";
            return txt;
        }

        private void button29_Click(object sender, EventArgs e) // Save configuration
        {
            saveFileDialog1.Filter = "settings|*.settings";
            saveFileDialog1.Title = "Save a settings File";
            saveFileDialog1.ShowDialog();
            if (saveFileDialog1.FileName=="") return;
            FileStream f = new FileStream(saveFileDialog1.FileName, FileMode.Create);
            BinaryWriter  wr = new BinaryWriter(f);
            wr.Write(SharedResources.hwconfstring.ToArray());

            // wr.Write(("raMaxPos "+SharedResources.raMaxPos.ToString()+"\r\n").ToArray());
            // wr.Write(("raMaxSpeed "+SharedResources.raMaxSpeed.ToString()+"\r\n").ToArray());
            // wr.Write(("ramsToSpeed "+SharedResources.ramsToSpeed.ToString()+"\r\n").ToArray());
            // wr.Write(("decMaxPos "+SharedResources.decMaxPos.ToString()+"\r\n").ToArray());
            // wr.Write(("decMaxSpeed "+SharedResources.decMaxSpeed.ToString()+"\r\n").ToArray());
            // wr.Write(("decmsToSpeed "+SharedResources.decmsToSpeed.ToString()+"\r\n").ToArray());
            // wr.Write(("timeComp "+SharedResources.timeComp.ToString()+"\r\n").ToArray());
            // 
            // wr.Write(("SiteLatitude "+T.SiteLatitude.ToString()+"\r\n").ToArray());
            // wr.Write(("SiteElevation "+T.SiteElevation.ToString()+"\r\n").ToArray());
            // wr.Write(("SiteLongitude "+T.SiteLongitude.ToString()+"\r\n").ToArray());
            // wr.Write(("FocalLength "+T.FocalLength.ToString()+"\r\n").ToArray());
            // wr.Write(("ApertureArea "+T.ApertureArea.ToString()+"\r\n").ToArray());
            // wr.Write(("ApertureDiameter "+T.ApertureDiameter.ToString()+"\r\n").ToArray());
            // wr.Write(("GuideRateRightAscension "+T.GuideRateRightAscension.ToString()+"\r\n").ToArray());
            // wr.Write(("GuideRateDeclination "+T.GuideRateDeclination.ToString()+"\r\n").ToArray());
            // wr.Write(("FocuserHardware.StepSize "+this.f.StepSize.ToString()+"\r\n").ToArray());
            // wr.Write(("FocuserHardware.fastSpeed "+FocuserHardware.fastSpeed.ToString()+"\r\n").ToArray());
            // wr.Write(("FocuserHardware.slowSpeed "+FocuserHardware.slowSpeed.ToString()+"\r\n").ToArray());
            f.Close();
        }

        private void button30_Click(object sender, EventArgs e) // load configuration
        {
            openFileDialog1.Filter = "settings|*.settings";
            openFileDialog1.Title = "Save a settings File";
            openFileDialog1.ShowDialog();
            if (openFileDialog1.FileName=="") return;
            try { 
                string[] lines= System.IO.File.ReadAllLines(openFileDialog1.FileName);
                if (lines.Length>0 && lines[0].Length==154)
                {
                    SharedResources.hwconfstring= lines[0];
                    SharedResources.readHWString();
                    return;
                }
                foreach (string line in lines)
                {
                    var w1 = line.Split(' ').FirstOrDefault();
                    var w2 = line.Split(' ').Skip(1).FirstOrDefault();
                    double v= Convert.ToDouble(w2);
                    if (w1=="raMaxPos") SharedResources.raMaxPos= (int)v;
                    if (w1=="raMaxSpeed") SharedResources.raMaxSpeed= (int)v;
                    if (w1=="ramsToSpeed") SharedResources.ramsToSpeed= (int)v;
                    if (w1=="decMaxPos") SharedResources.decMaxPos= (int)v;
                    if (w1=="decMaxSpeed") SharedResources.decMaxSpeed= (int)v;
                    if (w1=="decmsToSpeed") SharedResources.decmsToSpeed= (int)v;
                    if (w1=="timeComp") SharedResources.timeComp= (int)v;
                    if (w1=="SiteLatitude") T.SiteLatitude= v;
                    if (w1=="SiteElevation") T.SiteElevation= v;
                    if (w1=="SiteLongitude") T.SiteLongitude= v;
                    if (w1=="FocalLength") T.FocalLength= v;
                    if (w1=="ApertureArea") T.ApertureArea= v;
                    if (w1=="ApertureDiameter") T.ApertureDiameter= v;
                    if (w1=="GuideRateRightAscension") T.GuideRateRightAscension= v;
                    if (w1=="GuideRateDeclination") T.GuideRateDeclination= v;
                    if (w1=="FocuserHardware.StepSize") this.f.StepSize= v;
                    if (w1=="FocuserHardware.fastSpeed") FocuserHardware.fastSpeed= (int)v;
                    if (w1=="FocuserHardware.slowSpeed") FocuserHardware.slowSpeed= (int)v;
                }
            } catch { MessageBox.Show("They was an error reading the file"); }
        }

        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.guidingBits = (SharedResources.guidingBits & 0b11111101)|(checkBox2.Checked?2:0);
        }

        private void checkBox3_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.guidingBits = (SharedResources.guidingBits & 0b11101111) | (checkBox3.Checked ? 16 : 0);
        }

        private void raGuideStop_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.guidingBits = (SharedResources.guidingBits & 0b11111011) | (raGuideStop.Checked ? 4 : 0);
        }

        private void decGuideStop_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.guidingBits = (SharedResources.guidingBits & 0b11011111) | (decGuideStop.Checked ? 32 : 0);
        }
        private void checkBox9_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.guidingBits = (SharedResources.guidingBits & 0b11111110) | (checkBox9.Checked ? 1 : 0);
        }

        private void checkBox10_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.guidingBits = (SharedResources.guidingBits & 0b11110111) | (checkBox10.Checked ? 8 : 0);
        }

        private void textBox7_TextChanged(object sender, EventArgs e)
        {
            double v; if (double.TryParse(textBox7.Text, out v)) SharedResources.guideDecAgressivity= v;
        }

        private void textBox8_TextChanged(object sender, EventArgs e) { calcSteps(); }
        private void textBox9_TextChanged(object sender, EventArgs e) { calcSteps(); }
        private void textBox10_TextChanged(object sender, EventArgs e) { calcSteps(); }
        private void textBox11_TextChanged(object sender, EventArgs e) { calcSteps(); }
        private void textBox12_TextChanged(object sender, EventArgs e) { calcSteps(); }
        private void textBox16_TextChanged(object sender, EventArgs e) { calcSteps(); }
        void calcSteps()
        {
            int crown, gear1, gear2, stepper, micros; double divider;
            if (!int.TryParse(textBox8.Text, out crown)) return;
            if (!int.TryParse(textBox9.Text, out gear1)) return;
            if (!int.TryParse(textBox10.Text, out gear2)) return;
            if (!int.TryParse(textBox11.Text, out stepper)) return;
            if (!int.TryParse(textBox12.Text, out micros)) return;
            if (!double.TryParse(textBox16.Text, out divider)) return;
            textBox13.Text = (crown * gear1 * stepper * micros / gear2).ToString();
            textBox17.Text = ((int)(crown * gear1 * stepper * micros / gear2 * (divider/360))).ToString();
        }

        private void button26_Click(object sender, EventArgs e)
        {
            T.PulseGuide(GuideDirections.guideNorth, 1000);
        }

        private void button33_Click(object sender, EventArgs e)
        {
            T.PulseGuide(GuideDirections.guideSouth, 1000);
        }

        private void button31_Click(object sender, EventArgs e)
        {
            T.PulseGuide(GuideDirections.guideEast, 1000);
        }

        private void button32_Click(object sender, EventArgs e)
        {
            T.PulseGuide(GuideDirections.guideWest, 1000);
        }

        private void button34_Click(object sender, EventArgs e)
        {
            T.AbortSlew();
            testMoveCycle= -1;
        }

        private void logGuideOnly_CheckedChanged(object sender, EventArgs e)
        {

        }

        private void StepsPerSecond_CheckedChanged(object sender, EventArgs e)
        {
        }

        CArduViseur av= new CArduViseur();
        Bitmap bmp = null;
        void drawStars()
        {
            if (bmp == null || bmp.Width!=pictureBox1.Width || bmp.Height!=pictureBox1.Height)
                bmp = new Bitmap(pictureBox1.Width, pictureBox1.Height, System.Drawing.Imaging.PixelFormat.Format32bppRgb);

            var data = bmp.LockBits(new Rectangle(0, 0, pictureBox1.Width, pictureBox1.Height), System.Drawing.Imaging.ImageLockMode.WriteOnly, bmp.PixelFormat);

            unsafe
            {
                uint* dest = (uint*)data.Scan0;
                if (!button35.Visible)
                    av.drawSky(dest, 0, 0, pictureBox1.Width, pictureBox1.Height, pictureBox1.Width, SharedResources.RightAssension, SharedResources.Declinaison, 0, 1);
                else
                    av.drawSky(dest, 0, 0, pictureBox1.Width, pictureBox1.Height, pictureBox1.Width);
            }

            bmp.UnlockBits(data);
            pictureBox1.Image= bmp;
        }

        private void pictureBox1_MouseDown(object sender, MouseEventArgs e)
        {
            av.penEvent(e.X, e.Y, true);
            label40.Text= av.displayText;
            if (label40.Text!="" && av.clickra!=-100000 && av.clickdec!=-100000)
            { 
                catalog.SelectedIndex= -1;
            }
        }

        private void pictureBox1_MouseLeave(object sender, EventArgs e)
        {
            av.penEvent(-1, -1, false);
        }

        private void pictureBox1_MouseMove(object sender, MouseEventArgs e)
        {
            if (av.penDown) 
            {
                button35.Visible= true;
                av.penEvent(e.X, e.Y, true); drawStars();
            }

        }

        private void pictureBox1_MouseUp(object sender, MouseEventArgs e)
        {
            av.penEvent(e.X, e.Y, false);
        }

        private void button35_Click(object sender, EventArgs e)
        {
            button35.Visible= false;
        }

        private void checkBox7_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.guideAfterSlew = checkBox7.Checked;
            TelescopeHardware.saveProfile();
        }

        private void textBox20_TextChanged(object sender, EventArgs e)
        {
            double v; if (double.TryParse(textBox20.Text, out v)) SharedResources.guideRaAgressivity = v;
        }

        private void button36_Click_1(object sender, EventArgs e)
        {
            TelescopeHardware.Park();
        }

        private void button37_Click(object sender, EventArgs e)
        {
            TelescopeHardware.Unpark();
        }



        private void checkBox12_CheckedChanged(object sender, EventArgs e)
        {
            lastFocusPos= -1000;
        }

        private void FrmMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            SharedResources.finish= true;
        }







        private void labelCom_DoubleClick(object sender, EventArgs e)
        {
            SharedResources.comPort= "tcp";
        }

        private void button13_Click(object sender, EventArgs e)
        {
            logBox.SelectAll(); logBox.Copy();
        }

        private void button39_Click(object sender, EventArgs e)
        {
            SharedResources.setToTrueNorth();
        }

        private void button40_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":Mg" + (SharedResources.raPos+SharedResources.raMaxPos/4).ToString("X8") + SharedResources.decPos.ToString("X8") + "#", 0);
        }

        private void button41_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":Mg" + (SharedResources.raPos-SharedResources.raMaxPos/4).ToString("X8") + SharedResources.decPos.ToString("X8") + "#", 0);
        }


        private void textBox1_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == (char)Keys.Enter) focusTo();
        }

        private void button42_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":Mf#", 0);
        }

        int testMoveCycle= -1; // > 0 is: moving...
        void TestCycle()
        {
            if (SharedResources._ScopeMoving) return;
            if (testMoveCycle==-1) return;
            int ram = ((int)(((Int64)(SharedResources.raMaxPos)) * SharedResources.raAmplitude / 360))*99/100; // This allows to avoid meridian flips on max!
            int decm = SharedResources.decMaxPos / 2;  
            int minvdec= SharedResources.Latitude/36000; // for north emisphere only!
            int decl= SharedResources.raMaxPos*minvdec/180; // min visible declinaison here... avoids scope bumping in things!
            if (testMoveCycle==0) SharedResources.goToMotor(0, decm);
            if (testMoveCycle==1) SharedResources.goToMotor(ram, decm);
            if (testMoveCycle==2) SharedResources.goToMotor(0, decm);
            if (testMoveCycle==3) SharedResources.goToMotor(ram/2, decm);
            if (testMoveCycle==4) SharedResources.goToMotor(ram/2, decl);
            if (testMoveCycle==5) SharedResources.goToMotor(ram/2, decm);
            if (testMoveCycle==6) SharedResources.goToMotor(0, decm);
            if (testMoveCycle==7) SharedResources.goToMotor(ram, decl);
            if (testMoveCycle==8) SharedResources.goToMotor(0, decm);
            if (testMoveCycle==9) SharedResources.goToMotor(ram, decm);
            if (testMoveCycle==10) SharedResources.goToMotor(0, decl);
            if (testMoveCycle==11) SharedResources.goToMotor(ram, decm);
            if (testMoveCycle==12) SharedResources.goToMotor(ram/2, decm);
            testMoveCycle= testMoveCycle+1; if (testMoveCycle==13) { testMoveCycle= -1;  button43.Text= "Test"; }
            button43.Text= "Stp "+testMoveCycle.ToString()+"/12";
        }
        private void button43_Click(object sender, EventArgs e)
        {
            // move RA back and forth
            // move dec back and forth
            // move ra/dec in both diagonals
            if (testMoveCycle==-1) { testMoveCycle= 0; TestCycle(); }
            else { testMoveCycle= -1; T.AbortSlew(); button43.Text= "Test"; }
        }

        private void posCB_SelectionChangeCommitted(object sender, EventArgs e)
        {
            var lines = SharedResources.locations.Split('\n');
            var matchingLine = lines.FirstOrDefault(line => line.Split('\t')[0] == posCB.Text);
            if (matchingLine != null && posCB.Text!="")
            {
                List<string> allItems = matchingLine.Split('\t').ToList();
                SiteLatitude.Text= allItems[1];
                SiteLongitude.Text= allItems[2];
                SiteElevation.Text= allItems[3];
                FocalLength.Text= allItems[4];
                Aperture.Text= allItems[5];
                Area.Text= allItems[6];
                bool b1, b2;
                int Latitude = (int)(SharedResources.fromHms2(SiteLatitude.Text, out b1) * 10);
                int Longitude = (int)(SharedResources.fromHms2(SiteLongitude.Text, out b2) * 10);
                int SiteAltitude; int fl, ap, ar;
                if (b1 && b2 && int.TryParse(SiteElevation.Text, out SiteAltitude) && 
                     int.TryParse(FocalLength.Text, out fl) &&
                     int.TryParse(Aperture.Text, out ap) &&
                     int.TryParse(Area.Text, out ar))
                {
                    SharedResources.Latitude = Latitude; 
                    SharedResources.Longitude= Longitude; 
                    SharedResources.SiteAltitude= SiteAltitude;
                    SharedResources.updateAzimutal();
                    SharedResources.FocalLength= fl;
                    SharedResources.Diameter_mm= ap;
                    SharedResources.Area_cm2= ar;
                } else
                    MessageBox.Show("Error: One of the data here was bad!");
            }
        }
        private void posSave_Click(object sender, EventArgs e)
        {
            List<string> lines = SharedResources.locations.Split('\n').ToList();
            int index = lines.FindIndex(s2 => s2.StartsWith(posCB.Text));
            string s= posCB.Text+'\t'+SiteLatitude.Text+'\t'+SiteLongitude.Text+'\t'+SiteElevation.Text+'\t'+FocalLength.Text+'\t'+Aperture.Text+'\t'+Area.Text;
            if (index!=-1) lines[index] = s; else lines.Insert(0, s);
            TelescopeHardware.savelocations(string.Join("\n", lines));
            updateLocations();
        }
        private void posDel_Click(object sender, EventArgs e)
        {
            List<string> lines = SharedResources.locations.Split('\n').ToList();
            int index = lines.FindIndex(s2 => s2.StartsWith(posCB.Text));
            if (index==-1) return;
            lines.RemoveAt(index);
            TelescopeHardware.savelocations(string.Join("\n", lines));
            updateLocations();
        }
        private void updateLocations()
        {
            posCB.DataSource= SharedResources.locations.Split('\n')                              // split into lines
                    .Where(line => !string.IsNullOrWhiteSpace(line)) // ignore empty lines
                    .Select(line => line.Split('\t')[0])      // take first column
                    .ToList();
        }

        private void button20_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":MR#", 0); // reboot
        }

        private void label56_Click(object sender, EventArgs e)
        {

        }

        private void checkBox19_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.reconnectOnDrop = checkBox19.Checked;
            TelescopeHardware.saveProfile();
        }


        public static void ParallaxConstantsToLatAlt(
            double longitudeDeg, double rho, double rhoSinPhiPrime,
            out double latitudeDeg, out double altitudeMeters)
        {
            const double a = 6378137.0; // Equatorial radius in meters
            const double f = 1.0 / 298.257223563;
            const double e2 = 2 * f - f * f; // Square of eccentricity

            // Step 1: Compute geocentric latitude φ′ from rhoSinPhiPrime
            double sinPhiPrime = rhoSinPhiPrime / rho;
            double phiPrime = Math.Asin(sinPhiPrime);

            // Step 2: Compute (X, Z) position in meters
            double R = rho * a; // Convert normalized radius to meters
            double Z = R * sinPhiPrime;
            double X = R * Math.Cos(phiPrime); // Assumes observer on the meridian (local)

            // Step 3: Iteratively solve for geodetic latitude φ and altitude h
            double lat = phiPrime; // initial guess
            double h = 0;
            double tolerance = 1e-12;
            int maxIterations = 10;

            for (int i = 0; i < maxIterations; i++)
            {
                double sinLat = Math.Sin(lat);
                double N = a / Math.Sqrt(1 - e2 * sinLat * sinLat);
                double newLat = Math.Atan2(Z + e2 * N * sinLat, X);
                h = X / Math.Cos(newLat) - N;
                if (Math.Abs(newLat - lat) < tolerance) break;
                lat = newLat;
            }

            latitudeDeg = lat * 180.0 / Math.PI;
            altitudeMeters = h;
        }

        // Deal with focus saved positions...
        void updateSavedPos()
        {
            comboBox2.Items.Clear();
            string[] elements = f.savedPos.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
            comboBox2.Items.AddRange(elements); // On ajoute le tableau complet
            // if (comboBox1.Items.Count>0) comboBox1.SelectedIndex = 0;
        }

        private void phd2GuideDelay_TextChanged(object sender, EventArgs e)
        {
            int i; if (int.TryParse(phd2GuideDelay.Text, out i)) { SharedResources.phd2GuideDelay= i; TelescopeHardware.saveProfile(); }
        }

        private void button45_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":B000000001#", 0); // BNO calibrate here (assign current position to BNO data). One more 0 because first one is ignored!
        }

        private void checkBox20_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.parkAtSunrise = checkBox20.Checked;
            TelescopeHardware.saveProfile();
        }

        private void checkBox20_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button==MouseButtons.Middle) SharedResources.sunRaiseTime= DateTime.UtcNow.AddSeconds(10);
        }

        private void button28_Click(object sender, EventArgs e) // focuser move to saved position...
        {
            try { 
                string s= comboBox2.SelectedItem.ToString(); 
                int debut = s.IndexOf('(') + 1, fin = s.IndexOf(')');
                int pos = int.Parse(s.Substring(debut, fin - debut));
                f.Move(pos);
            } catch {  return; }
        }
        private void button44_Click(object sender, EventArgs e) // remove entry from list...
        {
            var elements = f.savedPos.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries).ToList();
            if (comboBox2.SelectedIndex>=0 && comboBox2.SelectedIndex<elements.Count)
            { 
                elements.RemoveAt(comboBox2.SelectedIndex);
                f.savedPos= string.Join(";", elements);
                updateSavedPos();
            }
        }
        private void button27_Click(object sender, EventArgs e) // focusser save new position...
        {
            string s= comboBox2.Text;
            int debut = s.IndexOf('(');
            if (debut>=0) s= s.Substring(0, debut);
            if (s.Length<2) return;
            s+= "(";
            // find s in existing list...
            string[] elements = f.savedPos.Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
            for (int i=0; i<elements.Length; i++) 
                if (elements[i].StartsWith(s))
                {
                    elements[i]= s+f.Position.ToString()+')'; 
                    comboBox2.Items.Clear();
                    comboBox2.Items.AddRange(elements); // On ajoute le tableau complet
                    f.savedPos= string.Join(";", elements);
                    return;
                }
            f.savedPos= f.savedPos+s+f.Position.ToString()+");";
            updateSavedPos();
        }

        SateliteTrack sateliteTrack= null;
        private void button38_Click(object sender, EventArgs e)
        {
            if (sateliteTrack==null) sateliteTrack = new SateliteTrack();
            sateliteTrack.Show();
        }

        private void textBox21_Leave(object sender, EventArgs e)
        {
            bool ok;
            int time = SharedResources.fromHms(textBox21.Text, out ok); if (!ok) return;
            DateTime now= DateTime.Now;
            DateTime t= now.Date.AddSeconds(time);
            if (t< now) t = t.AddDays(1);
            SharedResources.sunRaiseTime= t;
        }

        private void textBox21_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == 13) textBox21_Leave(sender, e);
        }


        public struct SettingTimeResult
        {
            public bool IsCircumpolar { get; set; }  
            public bool NeverRises { get; set; }     
            public DateTime SettingTimeUtc { get; set; }
        }

        public static SettingTimeResult GetSettingTimeUtc(
            double raHours, 
            double decDegrees, 
            double targetAltDegrees, 
            double latitudeDeg, 
            double longitudeDeg, 
            DateTime currentUtc)
        {
            var result = new SettingTimeResult();

            double radLat = latitudeDeg * (Math.PI / 180.0);
            double radDec = decDegrees * (Math.PI / 180.0);
            double radAlt = targetAltDegrees * (Math.PI / 180.0);

            // Compute cos(H)
            double cosH = (Math.Sin(radAlt) - Math.Sin(radLat) * Math.Sin(radDec)) 
                        / (Math.Cos(radLat) * Math.Cos(radDec));

            if (cosH < -1.0)
            {
                result.IsCircumpolar = true; 
                return result;
            }
            if (cosH > 1.0)
            {
                result.NeverRises = true; 
                return result;
            }

            // Hour Angle H in hours (setting = positive angle)
            double hHours = Math.Acos(cosH) * (12.0 / Math.PI); 

            // Target LST when setting
            double targetLstHours = (raHours + hHours) % 24.0;
            if (targetLstHours < 0) targetLstHours += 24.0;

            // Current LST using ASCOM AstroUtils or standard math
            // (If using ASCOM: double currentLstHours = astroUtils.LocalSiderealTime(longitudeDeg);)
            double currentLstHours = CalculateLst(currentUtc, longitudeDeg);

            // Sidereal Hours until setting
            double lstDiffHours = (targetLstHours - currentLstHours) % 24.0;
            if (lstDiffHours < 0) lstDiffHours += 24.0;

            // Convert Sidereal Hours to Solar/UTC Hours
            double solarHoursToGo = lstDiffHours / 1.00273790935;

            result.SettingTimeUtc = currentUtc.AddHours(solarHoursToGo);
            return result;
        }

        private static double CalculateLst(DateTime utcTime, double longitudeDeg)
        {
            double d = GetJulianDay(utcTime) - 2451545.0;
            double gmstDeg = 280.46061837 + 360.98564736629 * d;
            double lstDeg = gmstDeg + longitudeDeg;
            double lstHours = (lstDeg % 360.0 + 360.0) % 360.0 / 15.0;
            return lstHours;
        }

        private static double GetJulianDay(DateTime utc)
        {
            int y = utc.Year;
            int m = utc.Month;
            int d = utc.Day;
            if (m <= 2) { y -= 1; m += 12; }
            int a = y / 100;
            int b = 2 - a + (a / 4);
            double dayFraction = utc.TimeOfDay.TotalSeconds / 86400.0;
            return Math.Floor(365.25 * (y + 4716)) + Math.Floor(30.6001 * (m + 1)) + d + dayFraction + b - 1524.5;
        }
    }

}

