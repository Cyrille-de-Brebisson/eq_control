using ASCOM.BrebissonV1.Focuser;
using ASCOM.BrebissonV1.Telescope;
using ASCOM.DeviceInterface;
using ASCOM.Utilities;
using System;
using System.IO;
using System.Linq;
using System.Windows.Forms;
using static ASCOM.LocalServer.SharedResources;
using StarDisp;
using System.Drawing;
using System.Media;
using System.Net;
using ASCOM.Astrometry.Transform;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Collections.Generic;

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
            rbMessier.Checked= true;
            Width= groupBox2.Width+groupBox2.Left*2+Width-ClientSize.Width;
            Height= groupBox2.Height+groupBox2.Top+groupBox2.Left+Height-ClientSize.Height;
            calcSteps();
            checkBox7.Checked= SharedResources.guideAfterSlew;
            updateLocations();  posCB.Text= "";
        }
        ~FrmMain() { SharedResources.log= null; SharedResources.finish= true;  }
        private int lastFocusPos= 0x7fffffff;
        bool lastPowerBit= false; int lastPowerCount= 1;
        private void updateConnectedLabel() // every 500ms, update UI based on driver's data
        {
            try
            {
                BeginInvoke((MethodInvoker)delegate ()
                {
                    if (comboBox1.SelectedIndex == -1) comboBox1.SelectedIndex = 3;
                    checkBox1.Checked= SharedResources.saveTimes;
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

                        int lastHB= DateTime.UtcNow.Subtract(SharedResources.lastHeartBeat).Seconds;
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
                        button12.Text = "Mount " + (TelescopeHardware.SideOfPier == PierSide.pierEast ? "East" : "West");
                        if (SideralSelect.SelectedIndex==-1) SideralSelect.SelectedIndex= SharedResources.TrackingDisabled?0:(SharedResources.ascomtrackspd+1);
                        if (SharedResources.TrackingDisabled) if (SideralSelect.SelectedIndex!=0) SideralSelect.SelectedIndex= 0;
                        if (!SharedResources.TrackingDisabled) if (SideralSelect.SelectedIndex==0) SideralSelect.SelectedIndex= SharedResources.ascomtrackspd+1;
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
                            label41.Text = SharedResources.raPos.ToString() + " / " + maxRa.ToString() + " " + ((SharedResources.raPos*100)/maxRa).ToString() + "%";
                            int decMaxPos= SharedResources.decMaxPos; if (decMaxPos==0) decMaxPos= 1;
                            label42.Text = SharedResources.decPos.ToString() + " / " + (SharedResources.decMaxPos / 2).ToString() + " " + (SharedResources.decPos * 200 / decMaxPos).ToString() + "%";
                            int focMaxPos= SharedResources.focMaxStp; if (focMaxPos==0) focMaxPos= 1;
                            label52.Text = SharedResources._FocusserPosition.ToString() + " / " + focMaxPos.ToString() + " " + (SharedResources._FocusserPosition * 100 / focMaxPos).ToString() + "%";

                        }
                        TestCycle(); // deal with testing
                        if (!FreezeLastResponse.Checked && groupBox7.Visible)
                        {
                            lastRep1.Text= SharedResources.latestResponse1;
                            lastRep2.Text= SharedResources.latestResponse2;
                            NbResponses.Text= SharedResources.responceCount.ToString();
                        }
                    }
                    else
                    {
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
                        label18.Text = "N/A";
                        label19.Text = "N/A";
                        label21.Text = "N/A";
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

                    SiteLatitude.Text = SharedResources.raToText(SharedResources.Latitude/10);
                    SiteElevation.Text = SharedResources.SiteAltitude.ToString();
                    SiteLongitude.Text = SharedResources.raToText(SharedResources.Longitude/10);
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
                    textBox21.Text = (360 * 3600 * (Int64)SharedResources.raSettle / SharedResources.raMaxPos).ToString();
                    RAMaxMovement.Text= SharedResources.raAmplitude.ToString();

                    textBox6.Text = (SharedResources.guideRateDecf()*3600).ToString("N1");
                    textBox15.Text = (SharedResources.guideRateRaf()*3600).ToString("N1");
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
                    String s= ""; for (int i=3;  i>=0; i--) s+= (((SharedResources.ipaddr)>>(8*i))&0xff).ToString()+'.';
                    textBox26.Text= s.Substring(0, s.Length-1);
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

                textBox6.Text = "N/A";
                textBox15.Text = "N/A";

                label36.Text = "";
                rasps.Text =  "";
                radps.Text =  "";
                decsps.Text =  "";
                decdps.Text = "";
                checkBox4.Checked = checkBox5.Checked = checkBox6.Checked = false;
                textBox19.Text= "N/A";
                textBox21.Text= "N/A";
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
                Latitude = SharedResources.fromHms(SiteLatitude.Text, out b1) * 10;
                Longitude = SharedResources.fromHms(SiteLongitude.Text, out b2) * 10;
                double foxstp, decGuideRate, raGuideRate;
                if (b1 && b2)
                {
                    if (int.TryParse(SiteElevation.Text, out SiteAltitude) && int.TryParse(this.FocalLength.Text, out FocalLength) && int.TryParse(Aperture.Text, out Diameter_mm) &&
                        int.TryParse(Area.Text, out Area_cm2) && double.TryParse(StepSize.Text, out foxstp) &&
                        int.TryParse(FocMaxAcc.Text, out focAcc) && int.TryParse(this.FocMaxPos.Text, out focMaxStp) && int.TryParse(FocMaxSpd.Text, out focMaxSpd) &&
                        int.TryParse(textBox18.Text, out decBacklash) && int.TryParse(RAMaxMovement.Text, out raAmplitude) &&
                        double.TryParse(textBox6.Text, out decGuideRate) && double.TryParse(textBox15.Text, out raGuideRate) &&
                        int.TryParse(textBox14.Text, out raBacklash) && int.TryParse(textBox19.Text, out focBacklash) && int.TryParse(textBox21.Text, out raSettle))
                    {
                        SharedResources.raMaxPos = raMaxPos; SharedResources.raMaxSpeed = raMaxSpeed; SharedResources.ramsToSpeed =ramsToSpeed; SharedResources.decMaxPos = decMaxPos; SharedResources.decMaxSpeed = decMaxSpeed; SharedResources.decmsToSpeed = decmsToSpeed; SharedResources.timeComp = timeComp;
                        SharedResources.Latitude = Latitude; SharedResources.Longitude= Longitude; SharedResources.SiteAltitude= SiteAltitude; SharedResources.FocalLength= FocalLength; SharedResources.Diameter_mm= Diameter_mm; SharedResources.Area_cm2= Area_cm2; SharedResources.FocStepdum = (int)(foxstp * 10);
                        SharedResources.focMaxStp = focMaxStp; SharedResources.focMaxSpd = focMaxSpd; SharedResources.focAcc = focAcc;
                        if (SharedResources.focMaxStp>65535) SharedResources.focMaxStp= 65535;
                        SharedResources.decBacklash= (int)((decBacklash*(Int64)decMaxPos)/(360*3600)); SharedResources.raAmplitude = raAmplitude;
                        SharedResources.guideRateDec = (int)((decGuideRate * (Int64)SharedResources.decMaxPos / (360*3600)));
                        if (SharedResources.guideRateDec>=256) SharedResources.guideRateDec= 255;
                        SharedResources.guideRateRA = (int)((raGuideRate * (Int64)SharedResources.raMaxPos / (360*3600)));
                        if (SharedResources.guideRateRA>=256) SharedResources.guideRateRA= 255;
                        SharedResources.invertAxes = (checkBox4.Checked ? 2 : 0) | (checkBox5.Checked ? 1 : 0) | (checkBox6.Checked ? 4 : 0);
                        SharedResources.raBacklash = (int)((raBacklash * (Int64)raMaxPos) / (360 * 3600));
                        SharedResources.raSettle = (int)((raSettle * (Int64)raMaxPos) / (360*3600));
                        SharedResources.focBacklash = focBacklash;
                        SharedResources.guidingBits = (checkBox9.Checked ? 1 : 0) + (checkBox2.Checked ? 2 : 0) + (raGuideStop.Checked ? 4 : 0) +
                                                      (checkBox10.Checked ? 8 : 0) + (checkBox3.Checked ? 16 : 0) + (decGuideStop.Checked ? 32 : 0);
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

        private void textBox4_KeyPress(object sender, KeyPressEventArgs e)
        {
        }
        public void log(string message, int source)
        {
            if (groupBox7.Visible)
            try
            {
                    if (
                        (source==-1 && checkboxlogsystem.Checked) ||
                        (source==0 && checkboxascom.Checked) ||
                        (source==1 && checkBox8.Checked) ||
                        (source==2 && checkBox1.Checked) ||
                        (source==3 && checkBox15.Checked) ||
                        (source==4 && checkBox16.Checked))
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

        private void rbMessier_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbMessier.Checked) return;
            rbPlanets.Checked= rbC.Checked= rbNgc.Checked= rbStars.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Messier.Length; i++)
                catalog.Items.Add("M" + Ngcs.Messier[i].id.ToString()+" ("+Ngcs.Messier[i].ra.ToString()+","+Ngcs.Messier[i].dec.ToString()+")");
        }

        private void rbC_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbC.Checked) return;
            rbPlanets.Checked= rbMessier.Checked= rbNgc.Checked= rbStars.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Cadwell.Length; i++)
                catalog.Items.Add("C" + Ngcs.Cadwell[i].id.ToString()+" ("+Ngcs.Cadwell[i].ra.ToString()+","+Ngcs.Cadwell[i].dec.ToString()+")");
        }

        private void rbNgc_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbNgc.Checked) return;
            rbPlanets.Checked= rbMessier.Checked= rbC.Checked= rbStars.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Ngc.Length; i++)
                catalog.Items.Add("Ngc" + Ngcs.Ngc[i].id.ToString()+" ("+Ngcs.Ngc[i].ra.ToString()+","+Ngcs.Ngc[i].dec.ToString()+")");
        }

        private void rbStars_CheckedChanged(object sender, EventArgs e)
        {
            if (!rbStars.Checked) return;
            rbPlanets.Checked= rbMessier.Checked= rbC.Checked= rbNgc.Checked= false;
            catalog.Items.Clear();
            for (int i=0; i<Ngcs.Stars.Length; i++)
                catalog.Items.Add(Ngcs.Stars[i].id+" ("+Ngcs.Stars[i].ra.ToString()+","+Ngcs.Stars[i].dec.ToString()+")");
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
                catalog.Items.Add(Ngcs.planetNames[i]+" ("+ra.ToString()+","+dec.ToString()+")");
            }
        }

        private bool catalogCoord(out double ra, out double dec)
        {
            ra= dec= 0.0;
            if (catalog.SelectedItem==null) return false;
            string it= catalog.SelectedItem.ToString();
            int i1= it.IndexOf('('); if (i1<0) return false;
            int i2= it.IndexOf(','); if (i2<0) return false;
            int i3= it.IndexOf(')'); if (i3<0) return false;
            if (!double.TryParse(it.Substring(i1+1, i2-i1-1), out ra)) return false;
            if (!double.TryParse(it.Substring(i2+1, i3-i2-1), out dec)) return false;
            ra/= 3600.0; dec/= 3600.0;
            return true;
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
            SharedResources.SendSerialCommand(":Q#");
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
        private void button27_Click(object sender, EventArgs e)
        {
           string t= "pctime, hwtimes, uncountedsteps, hw/pctime, uncounted/pctime\r\n";
           for (int i=0; i<SharedResources.usedSavedTimes-120; i++)
                t+= SharedResources.savedTimes[i].pctime.ToString()+", "+SharedResources.savedTimes[i].HWtime.ToString()+", "+SharedResources.savedTimes[i].uncountedSteps.ToString()
                    +", "+(((double)(SharedResources.savedTimes[i+120].HWtime-SharedResources.savedTimes[i].HWtime))/(SharedResources.savedTimes[i+120].pctime-SharedResources.savedTimes[i].pctime)).ToString()
                    +", "+(((double)(SharedResources.savedTimes[i+120].uncountedSteps-SharedResources.savedTimes[i].uncountedSteps))/(SharedResources.savedTimes[i+120].pctime-SharedResources.savedTimes[i].pctime)).ToString()+"\r\n";
           logBox.Text= t;
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            SharedResources.saveTimes= checkBox1.Checked;
        }

        private void button28_Click(object sender, EventArgs e)
        {
            SharedResources.usedSavedTimes= 0;
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

        private void label41_Click(object sender, EventArgs e)
        {

        }
        private void button36_Click(object sender, EventArgs e)
        {
            
        }

        private void label43_Click(object sender, EventArgs e)
        {

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




        ///////////////////////////////////////////////////////////
        /// ISS STUFF
        ///////////////////////////////////////////////////////////
        string ISSErr = "";
        string issl1, issl2;
        bool gettle()
        {
            if (issl1!=null && issl1.Length != 0) return true;
            if (textBox23.Text.Length==0) // case of iss
            { 
                try
                {
                    var client = new WebClient();
                    string data = client.DownloadString("https://celestrak.org/NORAD/elements/stations.txt");
                    var lines = data.Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries);
                    for (int i = 0; i < lines.Length - 2; i++)
                        if (lines[i].Contains("ISS (NAUKA)"))
                        {
                            issl1= lines[i + 1]; issl2= lines[i + 2];
                            TelescopeHardware.saveisstls(issl1, issl2);
                            ISSErr = "ISS TLE OK";
                            log(ISSErr+"\r\n"+issl1+"\r\n"+issl2, 4);
                            return true;
                        }
                    ISSErr = "ISS TLE not found.";
                    log(ISSErr, 4);
                }
                catch { ISSErr = "exception on ISS load"; log(ISSErr, 4); }
                log("ISS: use saved tle", 4);
                issl1= SharedResources.isstle1;
                issl2= SharedResources.isstle2;
            } else { // satellite
                try
                {
                    var client = new WebClient();
                    string data = client.DownloadString("https://celestrak.org/NORAD/elements/gp.php?CATNR="+textBox23.Text+"&FORMAT=TLE");
                    var lines = data.Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries);
                    if (lines.Length>=3) { issl1= lines[1]; issl2= lines[2]; log(data, 4); }
                    ISSErr = textBox23.Text+" TLE not found.";
                    log(ISSErr, 4);
                }
                catch { ISSErr = "exception on TLE load"; log(ISSErr, 4); }
            }
            return issl1!=null && issl1.Length!=0;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct PassDetails
        {
            public double timeTo_H;
            public double duration_mn;
            public double max_elevation_deg;
        }
        PassDetails[] passes= new PassDetails[5]; int nbPasses= 0; DateTime passesTime;
        [DllImport("issposdll.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int GeneratePassList(double siteLat, double siteLong, double siteAltitude, [MarshalAs(UnmanagedType.LPStr)] string tle1, [MarshalAs(UnmanagedType.LPStr)] string tle2,
                        [Out] PassDetails[] pass_list, int nbpassesMax);

        [DllImport("issposdll.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void currentPos(double siteLat, double siteLong, double siteAltitude, [MarshalAs(UnmanagedType.LPStr)] string tle1, [MarshalAs(UnmanagedType.LPStr)] string tle2, out double az, out double alt, out double dst);
        [StructLayout(LayoutKind.Sequential)]
        public struct Tpos { public double az, alt, dst; };
        // get a list of coordinate for a pass. when is in h (get from pass list). typically 600 slots in pos is enough (10mn)
        [DllImport("issposdll.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern  int FuturePos(double siteLat, double siteLong, double siteAltitude, [MarshalAs(UnmanagedType.LPStr)] string tle1, [MarshalAs(UnmanagedType.LPStr)] string tle2, double when, [Out] Tpos[] pos, int nbpos);
        double issNextPassMaxRaSpd=0, issNextPassMaxDecSpd=0;
        Tpos[] poses= new Tpos[800]; int nbposes= 0;
        Tpos[] radecposes= new Tpos[800];

        public (double ra, double dec, double az, double alt, bool visible) GetIssRaDecFromLocation2(DateTime obstime, double inSecondsFromNow)
        {
            if (!gettle())  return (0.0f, 0.0f, 0.0f, 0.0, false);
            //set calculation parameters StartTime, EndTime and caclulation steps in minutes
            double az, alt, range;
            currentPos(TelescopeHardware.SiteLatitude, TelescopeHardware.SiteLongitude, TelescopeHardware.SiteElevation/1000.0, issl1, issl2, out az, out alt, out range);

            var transform = new Transform();
            transform.SiteLatitude = TelescopeHardware.SiteLatitude;
            transform.SiteLongitude = TelescopeHardware.SiteLongitude;
            transform.SiteElevation = TelescopeHardware.SiteElevation;
            transform.Refraction = true;

            if (nbPasses==0 && alt<0.0)
            {
                nbPasses= GeneratePassList(TelescopeHardware.SiteLatitude, TelescopeHardware.SiteLongitude, TelescopeHardware.SiteElevation/1000.0, issl1, issl2, passes, passes.Length);
                passesTime= obstime;
                if (nbPasses!=0)
                { 
                    nbposes= FuturePos(TelescopeHardware.SiteLatitude, TelescopeHardware.SiteLongitude, TelescopeHardware.SiteElevation/1000.0, issl1, issl2, passes[0].timeTo_H, poses, poses.Length);
                    transform.JulianDateUTC = utilities.DateUTCToJulian(obstime.AddHours(passes[0].timeTo_H));
                    transform.SetAzimuthElevation(poses[0].az, poses[0].alt);
                    double lra= transform.RATopocentric, ldec= transform.DECTopocentric;
                    double mra=0, mdec=0;
                    radecposes[0].az= lra; radecposes[0].alt= ldec; 
                    for (int i=1; i<nbposes; i++)
                    {
                        transform.JulianDateUTC = utilities.DateUTCToJulian(obstime.AddHours(passes[0].timeTo_H).AddSeconds(i));
                        transform.SetAzimuthElevation(poses[i].az, poses[i].alt);
                        double ra= transform.RATopocentric, dec= transform.DECTopocentric;
                        double dra= Math.Abs(ra-lra); if (dra<20/15 && dra>mra) mra= dra;
                        double ddec= Math.Abs(dec-ldec); if (ddec<20 && ddec>mdec) mdec= ddec;
                        lra= ra; ldec= dec;
                        radecposes[i].az= lra; radecposes[i].alt= ldec; 
                    }
                    issNextPassMaxRaSpd= mra*15; issNextPassMaxDecSpd= mdec;
                }
            }
            if (alt>0.0) nbPasses= 0;

            // to ra/dec
            transform.JulianDateUTC = utilities.DateUTCToJulian(obstime);
            transform.SetAzimuthElevation(az, alt);
            return (transform.RATopocentric, transform.DECTopocentric, az, alt, alt>0.0);
        }


        System.Timers.Timer issev = null;
        private void checkBox13_CheckedChanged(object sender, EventArgs e)
        {
            if (!checkBox13.Checked)
            {
                if (issev != null) { issev.Dispose(); issev = null; }
                issl1= "";
                textBox23.Enabled= true;
                return;
            }
            if (issev == null)
            {
                issev = new System.Timers.Timer(500);
                issev.Elapsed += (source, e2) => { try { BeginInvoke((MethodInvoker)delegate () { issUpdate(); }); } catch { } };
                issev.Enabled= true;
                textBox23.Enabled= false;
            }
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
        public static string tohms2(double v)
        {
            string n = "";
            if (v < 0) { n = "-"; v = -v; }
            n += ((int)v).ToString() + ":";
            v = (v - Math.Floor(v)) * 60;
            n += ((int)v).ToString();
            return n;
        }
        private void button38_Click(object sender, EventArgs e)
        {
            nbPasses= 0;
        }

        private void labelCom_DoubleClick(object sender, EventArgs e)
        {
            SharedResources.comPort= "tcp";
        }

        private void button13_Click(object sender, EventArgs e)
        {
            logBox.SelectAll(); logBox.Copy();
        }

        DateTime issTrackStartTime;
        int keyState = 0; // 1:up 2:down 3:right 4:left
        bool keyStateShift= false;

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

        void resetPictureBox()
        {
            pictureBox2.Image = null;
        }

        int issTrackMode = 0; // 0: no tracking. 1: first goto was sent. 2: regular tracking

        private void label51_Click(object sender, EventArgs e)
        {
            Process.Start(new ProcessStartInfo("cmd", "/c start https://celestrak.org/NORAD/elements/") { CreateNoWindow = true });
        }

        private void button39_Click(object sender, EventArgs e)
        {
            SharedResources.setToTrueNorth();
        }

        double issTrackDeltaRa = 0.0, issTrackDeltaDec= 0.0;

        private void button40_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":Mg" + (SharedResources.raPos+SharedResources.raMaxPos/4).ToString("X8") + SharedResources.decPos.ToString("X8") + "#", 0);
        }

        private void button41_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":Mg" + (SharedResources.raPos-SharedResources.raMaxPos/4).ToString("X8") + SharedResources.decPos.ToString("X8") + "#", 0);
        }

        double lastDelayToISSPass = 1; // last time we checked in how long the ISS will pass overhead. used to find 5mn warning...
        bool hasStartedTracking = false;

        struct TTrackingInfo { public double ra, dec, cra, cdec; public TTrackingInfo(double pra, double pdec, double pcra, double pcdec) {  ra= pra; dec= pdec; cra= pcra; cdec= pcdec; }  }
        List<TTrackingInfo> TrackingInfos;

        private void pictureBox2_DoubleClick(object sender, EventArgs e)
        {
            pictureBox2.Visible= false;
            writeISSLog();
        }

        void writeISSLog()
        {
            if (TrackingInfos!=null && TrackingInfos.Count > 0 )
            {
                string desktopPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
                string filePath = Path.Combine(desktopPath, "iss"+DateTime.Now.ToString()+".txt");
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

        private void textBox1_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (e.KeyChar == (char)Keys.Enter) focusTo();
        }

        private void button42_Click(object sender, EventArgs e)
        {
            SharedResources.SendSerialCommand(":Mf#");
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
            if (testMoveCycle==0) TelescopeHardware.goToMotor(0, decm);
            if (testMoveCycle==1) TelescopeHardware.goToMotor(ram, decm);
            if (testMoveCycle==2) TelescopeHardware.goToMotor(0, decm);
            if (testMoveCycle==3) TelescopeHardware.goToMotor(ram/2, decm);
            if (testMoveCycle==4) TelescopeHardware.goToMotor(ram/2, decl);
            if (testMoveCycle==5) TelescopeHardware.goToMotor(ram/2, decm);
            if (testMoveCycle==6) TelescopeHardware.goToMotor(0, decm);
            if (testMoveCycle==7) TelescopeHardware.goToMotor(ram, decl);
            if (testMoveCycle==8) TelescopeHardware.goToMotor(0, decm);
            if (testMoveCycle==9) TelescopeHardware.goToMotor(ram, decm);
            if (testMoveCycle==10) TelescopeHardware.goToMotor(0, decl);
            if (testMoveCycle==11) TelescopeHardware.goToMotor(ram, decm);
            if (testMoveCycle==12) TelescopeHardware.goToMotor(ram/2, decm);
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

        DateTime lastTrackingInfo;

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
                int Latitude = SharedResources.fromHms(SiteLatitude.Text, out b1) * 10;
                int Longitude = SharedResources.fromHms(SiteLongitude.Text, out b2) * 10;
                int SiteAltitude; int fl, ap, ar;
                if (b1 && b2 && int.TryParse(SiteElevation.Text, out SiteAltitude) && 
                     int.TryParse(FocalLength.Text, out fl) &&
                     int.TryParse(Aperture.Text, out ap) &&
                     int.TryParse(Area.Text, out ar))
                {
                    SharedResources.Latitude = Latitude; 
                    SharedResources.Longitude= Longitude; 
                    SharedResources.SiteAltitude= SiteAltitude;
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

        void updateIssImage()
        {
            int w= pictureBox2.Width, h= pictureBox2.Height;
            Bitmap b= new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppPArgb);
            Graphics g = Graphics.FromImage(b);
            for (int i = 0; i < TrackingInfos.Count-1; i++)
            {
                g.DrawLine(new Pen(Color.Yellow, 2), new PointF((float)TrackingInfos[i].ra*w/24, (float)(90-TrackingInfos[i].dec)*h/180), new PointF((float)TrackingInfos[i+1].ra*w/24, (float)(90-TrackingInfos[i+1].dec)*h/180));
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
            pictureBox2.Visible= true;
        }

        private void issUpdate()
        {
            if (!checkBox13.Checked) return;
            if (!SharedResources.Connected || !SharedResources.hasHWData) return;
            DateTime utc= DateTime.UtcNow;
            var r= GetIssRaDecFromLocation2(utc, 0);
            //Console.WriteLine("iss ra/dec:"+r.ra.ToString("N4")+"/"+r.dec.ToString("N4")+" az/alt:"+r.az.ToString("N4")+"/"+r.alt.ToString("N4")+" az2/alt2:"+az.az.ToString("N4")+"/"+az.alt.ToString("N4"));
            label43.Text = "pos " + tohms(r.ra) + "/" + tohms(r.dec)+" az:"+tohms(r.az)+"/"+ tohms(r.alt);

            bool isTracking = false;

            if (r.alt>0) // track
            {
                if (checkBox14.Checked)
                {
                    isTracking= true;
                    if (issev.Interval!=100) // tracking starts..
                    {
                        SharedResources.TrackingDisabled= true; // stop tracking...
                        issTrackStartTime= utc; // record start of track
                        issev.Interval= 100;
                        issTrackMode= 0;
                    }
                    if (!SharedResources.meridianFlip)
                    { 
                        r= GetIssRaDecFromLocation2(issTrackStartTime, 1.0);
                        if (issTrackMode==0) // first track. goto coordinates...
                        {
                            issTrackMode= 1; SharedResources._ScopeMoving= true;
                            TelescopeHardware.SlewToCoordinatesAsync(r.ra, r.dec);
                        } else if (issTrackMode==1) 
                        { 
                            if (!SharedResources.ScopeMoving) // got to starting point. go to new coordinates as iss has moved!. but save coordinates..
                            {
                                int ra= (int)(r.ra*3600), dec= (int)(r.dec*3600);
                                issTrackMode= 2;
                                issTrackDeltaRa= 0.0; issTrackDeltaDec= 0.0;
                                TelescopeHardware.SlewToCoordinatesAsync(r.ra, r.dec);
                                TrackingInfos= new List<TTrackingInfo>(); 
                                lastTrackingInfo= utc;
                                TrackingInfos.Add(new TTrackingInfo(r.ra, r.dec, 0, 0));
                            }
                        } else { // tracking. to to new coordinates at speed equal to the delta between the last 2 coordinates...
                            double nra= r.ra+issTrackDeltaRa, ndec= r.dec+issTrackDeltaDec;
                            int ra= (int)(nra*3600), dec= (int)(ndec*3600);
                            int time= 1000; // be there in 1000ms
                            int crc= ra+(ra>>8)+(ra>>16) + (dec)+(dec>>8)+(dec>>16) + (time)+(time>>8);
                            log("Track "+nra.ToString("N4")+" "+ndec.ToString("N4")+" "+time.ToString(), 4);
                            string cmd= ":T" + (ra&0xffffff).ToString("X6")+ (dec&0xffffff).ToString("X6")+(time&0xffff).ToString("X4") +  (crc&0xff).ToString("X2") + "#";
                            SharedResources.SendSerialCommand(cmd, 0);
                            double multip= 2.0; // speed up keys which are too slow...
                            if ((keyState&1)!=0) issTrackDeltaDec+=(keyStateShift?5.0:1)*5.0/3600.0*multip;
                            if ((keyState&2)!=0) issTrackDeltaDec-=(keyStateShift?5.0:1)*5.0/3600.0*multip;
                            if ((keyState&4)!=0) issTrackDeltaRa+=(keyStateShift?5.0:1)*5.0/3600.0/15.0*multip;
                            if ((keyState&8)!=0) issTrackDeltaRa-=(keyStateShift?5.0:1)*5.0/3600.0/15.0*multip;
                            ISSErr= "correction "+(issTrackDeltaRa*3600*15).ToString("N0")+"/"+(issTrackDeltaDec*3600).ToString("N0");
                            if (keyState!=0) log(ISSErr, 4);

                            if (utc.Subtract(lastTrackingInfo).TotalMilliseconds>1000)
                            { 
                                lastTrackingInfo= utc;
                                TrackingInfos.Add(new TTrackingInfo(nra, ndec, issTrackDeltaRa, issTrackDeltaDec));
                                updateIssImage();
                            }
                        }
                    }
                    else issTrackMode= 0;
                } else { 
                    issTrackMode= 0;
                    if (issev.Interval!= 500) issev.Interval= 500;
                }
            } else { 
                issTrackMode= 0;
                if (issev.Interval!= 500) issev.Interval= 500;
            }
            if (textBox22.Visible != (issTrackMode!=0)) 
            {
                textBox22.Visible= issTrackMode!=0;
                if (textBox22.Visible) textBox22.Focus();
            }
            
            if (hasStartedTracking && !isTracking)
            { 
                TelescopeHardware.AbortSlew(); // stop movements
                TelescopeHardware.Tracking= true; // restart normal tracking
            }
            hasStartedTracking= isTracking;

            if (r.alt > 0) label49.Text = "Visible";
            else
            {
                string next = "";
                if (nbPasses!=0)
                {
                    double v= passes[0].timeTo_H-((utc-passesTime).TotalSeconds/3600.0);
                    if (lastDelayToISSPass>5.0/60.0 && v<5.0/60.0 && checkBox17.Checked)
                        try { SoundPlayer player = new SoundPlayer(@"iss.wav"); player.Play(); } catch { }
                    lastDelayToISSPass= v;
                    if (v>=1.0)
                    { 
                        next += ((int)v).ToString() + "h";
                        v = (v - (int)(v)) * 60; next += ((int)v).ToString() + "m ";
                    } else
                    {
                        v = (v - (int)(v)) * 60; next += ((int)v).ToString() + "m ";
                        v = (v - (int)(v)) * 60; next += ((int)v).ToString() + "s ";
                    }
                    next+= "top:"+((int)passes[0].max_elevation_deg).ToString()+"°";
                    next+= "lasts:"+((int)(passes[0].duration_mn*60)).ToString()+"mn";
                    next+= " "+issNextPassMaxRaSpd.ToString("N1")+"°/s "+issNextPassMaxDecSpd.ToString("N1")+"°/s ";
                }
                label49.Text = "NotVisible "+next;
            }
            label48.Text = ISSErr;
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

    }

}

