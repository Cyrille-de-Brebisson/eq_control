// This is in a separate file in order to be includable in the windows version for testing
uint32_t Milisecond() { return Time::mnow(); }
class CMyTelescope : public CTelescope { public: CMyTelescope(int id): CTelescope(id, "EQ Control", "1.0", "EQ Control", "EQ Control") { }
protected:
    bool canpulseguide() override { return true; } // Indicates whether the telescope can be pulse guided
    TAlpacaErr pulseguide(int dir, int length) override  // Moves the scope in the given $Direction for the given $Duration (ms). . 0: north, 1: south, 2: east, 3: west
    { 
        stopMovingOnKeyRelease= false;
        if (dir==0 && CSavedData::savedData.guideRateDec>0) { MDecOn(); decGuiding= true; float spd= float(CSavedData::savedData.guideRateDec*CSavedData::savedData.dec.maxPos)/(360.0f*36000.0f); MDec.guide(int32_t(length*spd/1000.0f), uint32_t(spd)); return ALPACA_OK; }
        if (dir==1 && CSavedData::savedData.guideRateDec>0) { MDecOn(); decGuiding= true; float spd= float(CSavedData::savedData.guideRateDec*CSavedData::savedData.dec.maxPos)/(360.0f*36000.0f); MDec.guide(-int32_t(length*spd/1000.0f), uint32_t(spd)); return ALPACA_OK; }
        if (dir==2 && CSavedData::savedData.guideRateRA>0) { float r= float(CSavedData::savedData.guideRateRA*CSavedData::savedData.ra.maxPos)/(360.0f*36000.0f); MRa.guide(int32_t(length*r/1000.0f), uint32_t(r)); return ALPACA_OK; }
        if (dir==3 && CSavedData::savedData.guideRateRA>0) { float r= float(CSavedData::savedData.guideRateRA*CSavedData::savedData.ra.maxPos)/(360.0f*36000.0f); MRa.guide(-int32_t(length*r/1000.0f), uint32_t(r)); return ALPACA_OK; }
        return ALPACA_ERR_INVALID_VALUE;
    } 
    bool ispulseguiding() override  { return decGuiding || MRa._guide!=0; }; // Indicates whether the telescope is currently executing a PulseGuide command
    float get_guideratedeclination() override { return CSavedData::savedData.guideRateDec/36000.0f; }  // Returns the current Declination rate offset for telescope guiding
    TAlpacaErr set_guideratedeclination(float v) override  { if (v<0.0f || v>25.0f/3600.0f) return ALPACA_ERR_INVALID_VALUE; CSavedData::savedData.guideRateDec= int(v*36000.0f); return ALPACA_OK; } // Sets the current $GuideRateDeclination rate offset for telescope guiding.
    float get_guideraterightascension() override  { return CSavedData::savedData.guideRateRA/36000.0f; } // Returns the current RightAscension rate offset for telescope guiding
    TAlpacaErr set_guideraterightascension(float v) override { if (v<0.0f || v>25.0f/3600.0f) return ALPACA_ERR_INVALID_VALUE; CSavedData::savedData.guideRateRA= int(v*36000.0f); return ALPACA_OK; } // Sets the current $GuideRateRightAscension  rate offset for telescope guiding.

    bool get_tracking() override { return MRa.deltaBetweenUncountedSteps!=0; } // Indicates whether the telescope is tracking.
    TAlpacaErr set_tracking(bool v) override 
    { 
        if (!v) CSavedData::savedData.initUncountedStep2(0); 
        else CSavedData::savedData.initUncountedStep2(sideralSpeeds[trackingrate+1]);
        return ALPACA_OK; } // Enables or disables telescope $Tracking.
         // 0: sideral, 1: lunar, 2: solar, 3: king (15.0369 arc"/s)
    TAlpacaErr set_trackingrate(int v) override { if (v<0 || v>3) return ALPACA_ERR_INVALID_VALUE; trackingrate= v; set_tracking(get_tracking()); return ALPACA_OK; } // Sets the mount's $TrackingRate.
    int get_trackingrate() override { if (MRa.deltaBetweenUncountedSteps==0) return trackingrate; return MRa.sideralMove-1; } // Gets the mount's $TrackingRate.

    bool slewing() override { return MDec.isMoving()||MRa.isMoving(); }
    TAlpacaErr abortslew() override { savedGotoForFlip.flipFlags= 0; MDec.stop(); MRa.stop(); return ALPACA_OK; }; // Immediatley stops a slew in progress.
    TAlpacaErr slewtocoordinatesasync(float ra, float dec) override { if (ra<0.0f || ra>24.0f || dec<90.0f || dec>90.0f) return ALPACA_ERR_INVALID_VALUE; goTo(int32_t(ra*3600.0f), int32_t(dec*3600.0f), false); return ALPACA_OK; } // Asynchronously slew to the given equatorial $RightAscension $Declination coordinates.
    TAlpacaErr synctocoordinates(float ra, float dec) override { if (ra<0.0f || ra>24.0f || dec<90.0f || dec>90.0f) return ALPACA_ERR_INVALID_VALUE; sync(int32_t(ra*3600.0f), int32_t(dec*3600.0f)); return ALPACA_OK; } // Syncs to the given $RightAscension $Declination coordinates.

    TAlpacaErr axisrates(int axis, char *b) override { strcpy(b, "[{\"Maximum\": 4.001,\"Minimum\": 0.001}]"); return ALPACA_OK; } // Returns the rates at which the telescope may be moved about the specified $Axis  returns [{"Maximum": 0,"Minimum": 0}] in b (b will be 30 chr long)
    TAlpacaErr moveaxis(int axis, float rate) override   // Moves a telescope $Axis at the given $Rate.
    { 
        if (axis>=2 || (rate!=0.0f && (fabsf(rate)<0.00099 || fabsf(rate)>4.001))) return ALPACA_ERR_INVALID_VALUE;
        if (axis==0) MRa.goUpRealNoAbs(int(rate*(3600.0f/15.0f)));
        else if (axis==1) MDec.goUpRealNoAbs(int(rate*(3600.0f)));
        return ALPACA_OK; 
    }

    float declination() override { return MDec.posInReal()/3600.0f; }; // Returns the mount's declination.
    float rightascension() override { return MRa.posInReal()/3600.0f; }; // Returns the mount's right ascension coordinate.

    int get_sideofpier() override { return scopeWest()?1:0; } // Returns the mount's pointing state. 0:east, 1: west, -1: unknown
    int destinationsideofpier(float ra, float dec) override { return (scopeWest() ^ sameSideOfMeridian(int32_t(ra*3600.0f))) ? 0:1; } // Predicts the pointing state after a German equatorial mount slews to given $RightAscension $Declination coordinates. 0: east, 1: west: -1: unknown

    float get_aperturearea() override { return CSavedData::savedData.Area_cm2/10000.0f; } // Returns the telescope's aperture.
    TAlpacaErr set_aperturearea(float v) override { CSavedData::savedData.Area_cm2= uint16_t(v*10000.0f); return ALPACA_OK; } // Returns the telescope's aperture.
    float get_aperturediameter() override { return CSavedData::savedData.Diameter_mm/1000.0f; } // Returns the telescope's effective aperture.
    TAlpacaErr set_aperturediameter(float v) override { CSavedData::savedData.Diameter_mm= uint16_t(v*1000.0f); return ALPACA_OK; } // Returns the telescope's effective aperture.
    float get_focallength() override { return CSavedData::savedData.FocalLength/1000.0f; } // Returns the telescope's focal length in meters.
    TAlpacaErr set_focallength(float v) override { CSavedData::savedData.FocalLength= uint16_t(v*1000.0f); return ALPACA_OK;  } // Returns the telescope's focal length in meters.
    float get_siteelevation() override { return CSavedData::savedData.Altitude; } // Returns the observing $SiteElevation above mean sea level.
    TAlpacaErr set_siteelevation(float v) override { if (v<-200.0f || v>9999.0f) return ALPACA_ERR_INVALID_VALUE; CSavedData::savedData.Altitude= uint16_t(v); return ALPACA_OK; } // Sets the observing site's elevation above mean sea level.
    float get_sitelatitude() override { return CSavedData::savedData.Latitude/36000.0f; } // Returns the observing $SiteLatitude .
    TAlpacaErr set_sitelatitude(float v) override { if (v<-90.0f || v>90.0f) return ALPACA_ERR_INVALID_VALUE; CSavedData::savedData.Latitude= uint32_t(v*36000.0f); return ALPACA_OK; } // Sets the observing site's latitude.
    float get_sitelongitude() override { return CSavedData::savedData.Longitude/36000.0f; } // Returns the observing site's longitude.
    TAlpacaErr set_sitelongitude(float v) override {  if (v<-180.0f || v>180.0f) return ALPACA_ERR_INVALID_VALUE; CSavedData::savedData.Longitude= uint32_t(v*36000.0f); return ALPACA_OK; } // Sets the observing $SiteLongitude .

    void doReinit() override 
    { 
        CSavedData::savedData.ra.maxPos= mount.ra.maxPos;
        CSavedData::savedData.ra.maxSpd= mount.ra.maxSpd;
        CSavedData::savedData.ra.msToSpd= mount.ra.msToSpd;
        CSavedData::savedData.raBacklash= mount.ra.Backlash;
        CSavedData::savedData._raSettle= mount.raSettle;
        CSavedData::savedData.raAmplitude= mount.raAmplitude;

        CSavedData::savedData.dec.maxPos= mount.dec.maxPos;
        CSavedData::savedData.dec.maxSpd= mount.dec.maxSpd;
        CSavedData::savedData.dec.msToSpd= mount.dec.msToSpd;
        CSavedData::savedData.decBacklash= mount.dec.Backlash;
        CSavedData::savedData.invertAxes= (CSavedData::savedData.invertAxes&~3) | ((mount.ra.invert!=0)?2:0) | ((mount.dec.invert!=0)?1:0);

        CSavedData::savedData.save();
    } 
} *MyTelescope= nullptr;

class CMyFocuser : public CFocuser
{ public:
    CMyFocuser(int id): CFocuser(id, "CdB Focuser Driver", "1", "CdB Alpaca Focuser", "Focuser for eqMount") { }
    bool get_ismoving() override { return MFocus.isMoving(); }
    int32_t get_maxincrement() override { return MFocus.maxPos; }
    int32_t get_maxstep() override { return MFocus.maxPos; }
    int32_t get_position() override { return MFocus.pos; }
    float get_stepsize() override { return CSavedData::savedData.FocStepdum/10.0f; }
    TAlpacaErr put_halt() override { MFocus.stop(); return ALPACA_OK; };
    TAlpacaErr put_move(int32_t position) override { stopMovingOnKeyRelease= false; MFocusOn(); MFocus.goToSteps(position, MFocus.spdMax); return ALPACA_OK; };
    bool reinit= false; // used to handle parameter changes in sub_setup
    void subSetup(CAlpaca *Alpaca, int sock, bool get, char *data, CMyStr &s) override // This allows you to add stuff in the HTML or handle inputs...
    {
        int TODO; // add backlash!
        reinit= false;
        CFocuser::subSetup(Alpaca, sock, get, data, s);
        if (reinit) CSavedData::savedData.save();
    }
    void set_stepSize(CAlpaca *Alpaca, float vf) override { CSavedData::savedData.FocStepdum= int(vf*10.0f), reinit= true; }
    void set_maxSteps(CAlpaca *Alpaca, int vi) override { CSavedData::savedData.focMaxStp= vi, reinit= true; }
    void set_maxSpeed(CAlpaca *Alpaca, int vi) override { CSavedData::savedData.focMaxSpd= vi, reinit= true; }
    void set_msToMaxSpeed(CAlpaca *Alpaca, int vi) override { CSavedData::savedData.focAcc= vi, reinit= true; }
    void set_motorDirection(CAlpaca *Alpaca, int vi) override { CSavedData::savedData.invertAxes= (CSavedData::savedData.invertAxes&~4) | ((vi!=0)?4:0); reinit= true; }

};
