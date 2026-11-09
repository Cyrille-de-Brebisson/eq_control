#include "system.h"

CMotor motor1, motor2, motorFocus;
CScope scope;
uint32_t GPIO::pins= 0;
CI2C I2C;

void CSavedData::applySettings() // init the 3 motors based on saved data..
{
    motor1.init(-raAmplitude/2, raAmplitude/2, 0.0, uint32_t(ra.maxPos*raAmplitude/360.0), ra.maxSpd, ra.maxSpd*1000/ra.msToSpd, (invertAxes&2)!=0, stepPinRa, adrRa, 31);
    motor2.init(-180.0, 180.0, 0.0, dec.maxPos, dec.maxSpd, dec.maxSpd*1000/dec.msToSpd, (invertAxes&1)!=0, stepPinDec, adrDec, 31);
    // the /2 are because we only use 1/2 of the range of the motor!
    motorFocus.init(-focMaxStp/2, focMaxStp/2, 0, focMaxStp<<8, focMaxSpd<<8, (focMaxSpd<<8)*1000/focAcc, (invertAxes&4)!=0, stepPinFoc, adrFoc, 5);
    // MFocus.backlash= savedData.focBacklash<<8;
    // MRa.backlash= -savedData.raBacklash;
    // MDec.backlash= savedData.decBacklash;
    // savedGotoForFlip.noAutoFlip= (savedData.guidingBits&0x80)!=0;
}
CSavedData savedData;

// this is a generic "serial" style comunication with the remote computer or other.
// has virtual functions for read/write and the main process command functions
class CSerial { public:
    static CSerial *serials[3];
    CSerial() { 
        r= w= inbuf; colum= nullptr; o= outBuf;
        int i=0; while (serials[i]!=nullptr) i++; serials[i]= this;
    }
    virtual void begin()= 0;    // init code
    char outBuf[512]; char *o;  // Max length is under 300 bytes (CSavedData). o is where data is written in outbuf. It gets reset on a sendBuf
    void virtual sendBuf()= 0;  // send outBuf all the way to o, and then reset o to outBuf...
    size_t virtual read(char *b, size_t nb, int timeout)= 0; // read nb char max in b with timeout
    char inbuf[512]; char *r, *w, *colum= nullptr; // input buffer. w is where read data gets writen, r is the read index and colum is where the last ':' was found (if one)
    size_t read(size_t nb, int timeout) 
    {
        if (nb>size_t(inbuf+sizeof(inbuf)-w)) nb= inbuf+sizeof(inbuf)-w;
        nb= read(w, nb, timeout); w+= nb;
        return nb;
    }
    void flush(char c) { *o++= c; sendBuf(); } // add c in buffer (usually #) and send the buffer...
    void printHex2(uint32_t v, int8_t l) // send a l digit hex number to output buffer
    {
        char const hexstr[]= "0123456789ABCDEF";
        while (--l>=0) *o++= hexstr[((v>>(4*l)))&15];
    }
    static uint32_t readHex(char *&r, int8_t cnt) // read an hex value from the input buffer...
    {
        uint32_t v= 0;
        while (--cnt>=0)
        {
            if (*r>='0' && *r<='9') v= (v<<4) + *r++-'0';
            else if (*r>='a' && *r<='f') v= (v<<4) + *r++-'a'+10;
            else if (*r>='A' && *r<='F') v= (v<<4) + *r++-'A'+10;
            else return v;
        }
        return v;
    }
    void process()
    {
        read(size_t(inbuf+sizeof(inbuf)-w), 0); // read as much as possible...
        // if (w!=r) { *w= 0; printf("process %s\n", r); }
        ///////////////////////////////////////////
        // serial input processing
        // ! -> returns 56 hex characters. pos in Dec(6), Ra(6), focusser(6), 1 byte (2 chr) of status bits, millis_timer(6), meridian_value(6), uncounted_steps(6), raPos(8), decPos(8), power and sideralspeed(2). Then a #
        // & -> returns SavedData as a hex sequence ending with #. The last byte is the CRC...
        // @Hex# is the reverse to set the settings... crc HAS to be correct!
        // For all other commands, lines start with ':' and end with '#' or '\n' Go look at them, they are not that many of them!!!!
        while (r<w)
        {
            char c= *r++; // read one char...
            if (c<=' ') continue;                                    // ignore blanks
            if (c=='!')   // get info command
            {
                printHex2(int32_t(scope.decActual*3600), 6); printHex2(int32_t(scope.raActual*3600), 6); printHex2(int32_t(motorFocus.posInReal()), 6);
                // bit 0: moving, bit 1: focus moving, bit 2: side of pier, bit 3: meridian swapping, bit 4: flip disabled,
                // bit 5: tracking disabled, bit 6: power, bit 7: guiding
                printHex2((scope.getSlewing()?1:0) | (motorFocus.isMoving()?2:0) | (scope.scopeEast ? 0:4) | (scope.flipping?8:0) | (scope.tracking?0:32) | (scope.isPulseGuiding()?128:0), 2);
                printHex2(nowms(), 6);                               // this allows to verify time drift
                printHex2(uint32_t(scope.raMotorDegToRa(0.0)), 6);   // This allows to check if something will cause a flip or not... (know what RA is the middle of the RA range
                printHex2(-1, 6);                                    // MRa.countAllUncountedSteps: not used anymore. mark it as a new mount...
                printHex2(motor1.getPos(), 8); printHex2(motor2.getPos(), 8); // motor mechanical position ra for flip calculation. dec not used at this point
                printHex2(scope.sideralRateType<<5, 2);              // powercount and gps not used...
                // sendBNO(serial); // as needed
                flush('#');
                continue;
            }
            if (c=='&') // serial write hex represenation of CSavedData (get settings)
            { 
                savedData.crc= savedData.calcCrc(); uint8_t *d= (uint8_t*)&savedData;
                for (uint8_t i=0; i<sizeof(savedData); i++) printHex2(*d++, 2);
                flush('#');
                continue;
            }
            if (c=='@') // serial read hex representation of CSavedData (save settings)
            {   
                if (w-r-1<(sizeof(savedData)-4)*2) // we want 282 bytes (the savedData without the IP Address). The -2 is to remove the @ and final #
                {
                    read((sizeof(savedData)-4)*2-(w-r), 1000);
                    if (w-r<(sizeof(savedData)-4)*2) break; // the break will exit the loop, clear the buffer and wait for more inboud data..
                }
                uint8_t *d= (uint8_t*)r;
                for (uint8_t i=0; i<sizeof(savedData)-4; i++) d[i]= readHex(r, 2); // To binary...
                if (!((CSavedData*)d)->testCrc()) continue;                      // chech CRC
                memcpy(&savedData, d, sizeof(CSavedData)); savedData.save(); reboot(); // save data and reboot the system!
                return; // used in windows case...
            }

            if (c==':') colum= r; if (colum==nullptr) continue;          // nothing until ':' (line start)...
            if (c!='#' && c!='\n') continue;                             // not end of line... get next character
            char c0=colum[0], c1=colum[1], *d= colum+2; colum= nullptr;  // mark line done but save command...

            if (c0=='$' && c1=='P') { scope.technicalFlip= true; continue; } // tel lthe systeo that it's the other way around!
            // if (t2('$', 'f')) { enableFlip(serialContext.input[3]!='0'); continue; } // flip on/off
            if (c0=='Q') { scope.abortslew(); motorFocus.stop(); continue; } // :Q# stop driven movements (including focusser)

            if (c0=='T') // track. provides dst in ra/dec as int24, time to be there in ms as int16. then a crc as int8
            { 
                d--;
                int32_t ra= readHex(d, 6); int32_t dec= readHex(d, 6); if ((dec&0x800000)!=0) dec|= 0xff000000; // negative extend.
                int16_t time= readHex(d, 4);
                uint8_t crc= readHex(d, 2);
                uint8_t tcrc= uint8_t(ra)+uint8_t(ra>>8)+uint8_t(ra>>16) + uint8_t(dec)+uint8_t(dec>>8)+uint8_t(dec>>16) + uint8_t(time)+uint8_t(time>>8);
                if (tcrc!=crc) continue;
                scope.moveDec((dec/3600.0-scope.decActual)*(1000.0/15.0)/time); // move at speed
                scope.moveRa((ra/3600.0-scope.raActual)*1000.0/time);    // move at speed
            }
            if (c0=='M' && c1=='R') reboot(); // reboot the system!
            // All the commands from there on will take 1 or 2 inputs. In 8 char hex form most significant nible first in our input... We read them...
            int32_t n1= readHex(d, 8); int32_t n2= readHex(d, 8); 
            if (c0=='M' && c1=='G') { scope.slew(n1/3600.0, n2/3600.0, false); continue; } 
            if (c0=='M' && c1=='g') { scope.parking= true; continue; // use the new parking system. But then the motor pos are ignored...
                motor1.goToPos(n1); motor2.goToPos(n2); continue; } // go to steps (used for park and test motor)
            if (c0=='M' && c1=='S') { scope.synctocoordinates(n1/3600.0, n2/3600.0); continue; } 
            if (c0=='M' && c1=='t') { scope.setJulianSecSince2026(n1); continue; } // n1 is number of sec since jan1 2026 in julian days
            if (c0=='M' && c1=='d') { scope.moveDec(n1/3600.0); continue; } // :Mdspd(8)# (move dec in direction at speed)
            if (c0=='M' && c1=='r') { scope.moveRa(n1/3600.0*15.0); continue; } // :Mrspd(8)# (move ra in direction at speed). Data comes in in ra unit, but control is in deg/s
            if (c0=='M' && c1=='f') { scope.flip(); continue; } // // force a meridian flip (assumes that the user has verified that it was possible!)
            if (c0=='p' && c1=='r') { scope.guideRa(n1, n2); continue; } // :prstp(8)spd(8)# pulse guide ra for step count at speed
            if (c0=='p' && c1=='d') { scope.guideDec(n1, n2); continue; } // :pdstp(8)spd(8)# pulse guide dec for step count at speed
            if (c0=='$' && c1=='T') { // Track speed. It sends the number of second to a full turn (old style). Transform into ascom speeds and unpark (as per previous scope useage)
                scope.unpark(); 
                if (n1==24*3600+50*60) scope.setSideralRate(1, true);
                else if (n1==24*3600) scope.setSideralRate(2, true);
                else if (n1==1299188) scope.setSideralRate(3, true); // king
                else scope.setSideralRate(0, true); // assumes sideral
                continue; } // Track at speed to allow lunar/solar... parameter is second per full turn...
            // Focusser commands
            if (c0=='F' && c1=='G') { motorFocus.goTo(n1); continue; } // -> start the motor toward destination
            if (c0=='F' && c1=='M') { motorFocus.goAtSpeed(n1); continue; } // :FMxxxxxxxx# move out if x is >0, else in. x is 8 hex chars
            // BNO commands
            //if (t1('B')) { execBNO(n1, n2); continue; }
        }
        if (colum==nullptr) r= w= inbuf; // no :, we just flush the buffer...
        else { memcpy(inbuf, colum, w-colum); w-= w-colum; r= inbuf; } // remove data..
    }
};
CSerial *CSerial::serials[3];






#ifdef IDF_VER
#include "nvs_flash.h"
#include "nvs.h"
static nvs_handle_t saveLoadHandle= 0xffffffff;
static nvs_handle_t nvsHandle()
{
    if (saveLoadHandle!=0xffffffff) return saveLoadHandle;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) { ESP_ERROR_CHECK(nvs_flash_erase()); err = nvs_flash_init(); }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open("EQControl", NVS_READWRITE, &saveLoadHandle));
    return saveLoadHandle;
}

void CSavedData::load()
{
    size_t buflen= sizeof(*this);
    esp_err_t err= nvs_get_blob(nvsHandle(), "settings", this, &buflen); // does NOT modify buf if error....
    if (err==ESP_OK && sizeof(*this)==buflen && testCrc()) applySettings();
    else { init(); save(); }
}
void CSavedData::save()
{
    nvs_handle_t h= nvsHandle();
    crc= calcCrc();
    nvs_set_blob(h, "settings", this, sizeof(*this)); nvs_commit(h);
    applySettings();
}

#include "driver/gptimer.h"
static bool IRAM_ATTR stepperTick(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    uint32_t now= nowus();
    motor1.step(now); motor2.step(now); motorFocus.step(now); 
    return pdFALSE;
}
void HWLoop(void*) 
{ 
    // setup alarm for motors!
    gptimer_handle_t gptimer;
    gptimer_config_t timer_config = { .clk_src = GPTIMER_CLK_SRC_DEFAULT, .direction = GPTIMER_COUNT_UP, .resolution_hz = 1000000 }; // 1MHz, clock
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    gptimer_event_callbacks_t cbs = { .on_alarm = stepperTick, };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, nullptr));
    gptimer_alarm_config_t alarm_config1= { .alarm_count = 1000000/CMotor::maxStepsPerSecond, .reload_count = 0, .flags= {.auto_reload_on_alarm = true } }; // every 50us = 10k/s
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config1));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));
    while (true)
    {
        uint64_t now= nowus();
        scope.HWLoop(now);
        motor1.quantize(); motor2.quantize(); motorFocus.quantize();
        vTaskDelay(1000/(CMotor::quantizationPerSecond*portTICK_PERIOD_MS));
    }
}

////////////////////
// usb in/out through virtual serial USB link to the PC..
#include "driver/usb_serial_jtag.h"
class CUsbSerial : public CSerial { public:
    void begin() override
    {
        usb_serial_jtag_driver_config_t usb_serial_jtag_config = {.tx_buffer_size = 1024, .rx_buffer_size = 1024 };
        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    }
    size_t read(char *b, size_t nb, int timeoutms) override { return usb_serial_jtag_read_bytes(b, nb, timeoutms / portTICK_PERIOD_MS); }
    void sendBuf() override { usb_serial_jtag_write_bytes(outBuf, o-outBuf, 100/portTICK_PERIOD_MS); o= outBuf; } // 100ms should be enough to send a lot of data...
} UsbSerial;

////////////////////
// Bluetooth in/out through "serial"
// This can only handle ONE connection at a time because the ble_uart relies on the nimble.cpp file in the same directory which is single connection
// while the underlying system can handle 3 (per config file). So at one point I will have to pull in that file, update it to multi con and update here to add a device ID
#include "ble_uart.h"
static StreamBufferHandle_t xStreamBuffer = NULL;
class CBLESerial : public CSerial { public:
    static void ble_uart_on_rx(const uint8_t *data, size_t len)
    {
        if (data==nullptr || len==0) return;
        xStreamBufferSend(xStreamBuffer, data, len, 0);
    }
    void begin()
    {
        char const *advertizingName= "EQControl";
        xStreamBuffer= xStreamBufferCreate(512, 1);
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) { ESP_ERROR_CHECK(nvs_flash_erase()); ret = nvs_flash_init(); }
        ESP_ERROR_CHECK(ret);
        ble_uart_config_t config = {.encrypted = false, .device_name = advertizingName, .ble_uart_on_rx = ble_uart_on_rx };
        ESP_ERROR_CHECK(ble_uart_install(&config));
        ESP_ERROR_CHECK(ble_uart_open());
    }
    size_t read(char *b, size_t nb, int timeoutms) override { return xStreamBufferReceive(xStreamBuffer, b, nb, timeoutms/portTICK_PERIOD_MS); }
    void sendBuf() override { ble_uart_tx((uint8_t*)outBuf, o-outBuf); o= outBuf; } // 100ms should be enough to send a lot of data...
} BLESerial;

static uint32_t inline getStartTime() { return 0; }
#endif






#ifdef _MSC_VER // Windows only
/*
#include "Alpaca.h"
class CMyTelescope : public CTelescope { public: CMyTelescope(int id): CTelescope(id, "EQ Control", "1.0", "EQ Control", "EQ Control") { doReinit(); }
protected:
    bool canpark() override { return true; } // Indicates whether the telescope can be parked.
    bool atpark() override { return scope.parked; } // Indicates whether the telescope is at the park position.
    bool cansetpark() override { return true; } // Indicates whether the telescope park position can be set.
    bool canunpark() override { return true; } // Indicates whether the telescope can be unparked.
    TAlpacaErr park() override { scope.park(); return ALPACA_OK; }// Park the mount
    TAlpacaErr setpark() override { scope.setpark(); return ALPACA_OK; }// Sets the telescope's park position as current's position
    TAlpacaErr unpark() override { scope.unpark(); return ALPACA_OK; }// Unparks the mount.

    bool cansetrightascensionrate() override { return true; } // Indicates whether the RightAscensionRate property can be changed.
    float get_rightascensionrate() override { return float(scope.raTrackRate); } // Returns the telescope's right ascension tracking rate.
    TAlpacaErr set_rightascensionrate(float v) override { scope.raTrackRate= v; return ALPACA_OK; } // Sets the telescope's $RightAscensionRate tracking rate.
    bool cansetdeclinationrate() override { return true; } // Indicates whether the DeclinationRate property can be changed.
    float get_declinationrate() override { return float(scope.decTrackRate); } // Returns the telescope's declination tracking rate.
    TAlpacaErr set_declinationrate(float v) override { scope.decTrackRate= v; return ALPACA_OK; } // Sets the telescope's $DeclinationRate tracking rate.

    bool canpulseguide() override { return true; } // Indicates whether the telescope can be pulse guided
    TAlpacaErr pulseguide(int dir, int length) override  // Moves the scope in the given $Direction for the given $Duration (ms). . 0: north, 1: south, 2: east, 3: west
    { 
        if (dir==0) { scope.guideDec(get_guideratedeclination(), length); return ALPACA_OK; }
        if (dir==1) { scope.guideDec(-get_guideratedeclination(), length); return ALPACA_OK; }
        if (dir==2) { scope.guideRa(get_guideraterightascension(), length); return ALPACA_OK; }
        if (dir==3) { scope.guideRa(-get_guideraterightascension(), length); return ALPACA_OK; }
        return ALPACA_ERR_INVALID_VALUE;
    } 
    bool ispulseguiding() override  { return scope.isPulseGuiding(); }; // Indicates whether the telescope is currently executing a PulseGuide command

    bool get_tracking() override { return scope.tracking; } // Indicates whether the telescope is tracking.
    TAlpacaErr set_tracking(bool v) override { scope.setTracking(v); return ALPACA_OK; } // Enables or disables telescope $Tracking.
    // 0: sideral, 1: lunar, 2: solar, 3: king (15.0369 arc"/s)
    TAlpacaErr set_trackingrate(int v) override { if (v<0 || v>3) return ALPACA_ERR_INVALID_VALUE; scope.setSideralRate(v); return ALPACA_OK; } // Sets the mount's $TrackingRate.
    int get_trackingrate() override { return scope.sideralRateType; } // Gets the mount's $TrackingRate.

    bool slewing() override { return scope.getSlewing(); }
    TAlpacaErr abortslew() override { scope.abortslew(); return ALPACA_OK; }; // Immediatley stops a slew in progress.
    TAlpacaErr slewtocoordinatesasync(float ra, float dec) override { if (ra<0.0f || ra>24.0f || dec<-90.0f || dec>90.0f) return ALPACA_ERR_INVALID_VALUE; scope.slew(ra, dec, false); return ALPACA_OK; } // Asynchronously slew to the given equatorial $RightAscension $Declination coordinates.
    TAlpacaErr synctocoordinates(float ra, float dec) override { if (ra<0.0f || ra>24.0f || dec<-90.0f || dec>90.0f) return ALPACA_ERR_INVALID_VALUE; scope.synctocoordinates(ra, dec); return ALPACA_OK; } // Syncs to the given $RightAscension $Declination coordinates.

    TAlpacaErr moveaxis(int axis, float rate) override   // Moves a telescope $Axis at the given $Rate.
    { 
        if (axis>=2 || (rate!=0.0f && (fabsf(rate)<0.00099 || fabsf(rate)>mount.ra.maxSpd*360.0/mount.ra.maxPos))) return ALPACA_ERR_INVALID_VALUE;
        if (axis==0) scope.moveRa(rate);
        else if (axis==1) scope.moveDec(rate);
        return ALPACA_OK; 
    }

    float declination() override { return float(scope.decActual); }; // Returns the mount's declination.
    float rightascension() override { return float(scope.raActual); }; // Returns the mount's right ascension coordinate.
    TAlpacaErr altitude(float &v) override { v= float(scope.alt); return ALPACA_OK; }// Returns the mount's altitude above the horizon.
    TAlpacaErr azimuth(float &v) override { v= float(scope.az); return ALPACA_OK; }// Returns the mount's azimuth.

    int get_sideofpier() override { return scope.scopeEast?0:1; } // Returns the mount's pointing state. 0:east, 1: west, -1: unknown
    int destinationsideofpier(float ra, float dec) override { return scope.destinationsideofpier(ra, dec); } // Predicts the pointing state after a German equatorial mount slews to given $RightAscension $Declination coordinates. 0: east, 1: west: -1: unknown

    void doReinit() override 
    { 
        motor1.init(-mount.raAmplitude/2, mount.raAmplitude/2, 0.0, uint32_t(mount.ra.maxPos*mount.raAmplitude/360.0), mount.ra.maxSpd, mount.ra.maxSpd*1000/mount.ra.msToSpd, mount.ra.invert, stepPinRa, adrRa, 31);
        motor2.init(-180.0, 180.0, 0.0, mount.dec.maxPos, mount.dec.maxSpd, mount.dec.maxSpd*1000/mount.dec.msToSpd, mount.dec.invert, stepPinDec, adrDec, 31);
        int TODO; // backlash?
        // CSavedData::savedData.raBacklash= mount.ra.Backlash;
        // CSavedData::savedData._raSettle= mount.raSettle;
        // CSavedData::savedData.decBacklash= mount.dec.Backlash;
    } 
};

class CMyFocuser : public CFocuser
{ public:
    CMyFocuser(int id): CFocuser(id, "CdB Focuser Driver", "1", "CdB Alpaca Focuser", "Focuser for eqMount") { reinitMotor(); }
    int32_t get_position() override { return int32_t(motorFocus.posInReal()); } // return current position. No clue what it means in non absolute mode!
    bool get_ismoving() override { return motorFocus.isMoving(); }
    TAlpacaErr put_halt() override { motorFocus.stop(); return ALPACA_OK; };
    TAlpacaErr put_move(int32_t position) override { motorFocus.goTo(position); return ALPACA_OK; };
    void reinitMotor() override { motorFocus.init(0, maxSteps, maxSteps/2.0, maxSteps, maxSpeed, maxSpeed*1000/msToMaxSpeed, motorDirection, stepPinFoc, adrFoc, 4); }
};
static void Alpaca()
{
  CAlpaca *alpaca= new CAlpaca("CdBTelescopeServer", "CdB", "Alpaca CdB eq telescope", "Ardeche"); // done here as it initializes the storage and provides access facilities for CSavedData::savedData.load()
  alpaca->addDevice(new CMyTelescope(0));
  alpaca->addDevice(new CMyFocuser(0));
  alpaca->start(80);
}
*/

void HWLoop(void*) 
{ 
    uint64_t lastNow= nowus();
    while (true)
    {
        uint64_t now= nowus();
        // Issue all the steps that should have been issued...
        while (lastNow<now)
        { 
            motor1.step(uint32_t(now)); motor2.step(uint32_t(now)); motorFocus.step(uint32_t(now)); 
            lastNow+= 1000000/CMotor::maxStepsPerSecond; 
        }
        lastNow= now;

        scope.HWLoop(now);
        motor1.quantize(); motor2.quantize(); motorFocus.quantize();

        motor2.stats(m2pos, m2dstPos, m2maxSpd, m2requestedSpeed, m2currentSpeed);
        motor1.stats(m1pos, m1dstPos, m1maxSpd, m1requestedSpeed, m1currentSpeed);
        scope.stats();
        Sleep(1000/CMotor::quantizationPerSecond);
    }
}
void HWSet(int motor, double pos)
{
    if (motor==0) motor1.setPos(pos);
    if (motor==1) motor2.setPos(pos);
}
void CSavedData::load()
{
    HANDLE h= CreateFileA("EQControl", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL); 
    if (h!=INVALID_HANDLE_VALUE)
    {
        DWORD size= GetFileSize(h, NULL); 
        if (size==sizeof(*this))
        {
            uint8_t *data= (uint8_t*)this;
            size_t pos= 0;
            while (size>0) { DWORD read= 0; if (!ReadFile(h, data+pos, size, &read, NULL)) { CloseHandle(h); return; }; size-= read; pos+= read; }
        }
        CloseHandle(h);
    }
    if (testCrc()) applySettings();
    else { init(); save(); }
}
void CSavedData::save()
{
    crc= calcCrc();
    HANDLE h= CreateFileA("EQControl", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (h==INVALID_HANDLE_VALUE) return;
    DWORD write; WriteFile(h, this, sizeof(*this), &write, NULL);
    CloseHandle(h);
    applySettings();
}

#pragma comment(lib, "WS2_32.lib")
class CTcpSerial : public CSerial { public:
    SOCKET clientSocket= INVALID_SOCKET;
    char t[1000]; int pos= 0;
    CRITICAL_SECTION cs;
    static DWORD WINAPI sockListen(LPVOID lpThreadParameter)
    {
        CTcpSerial *This= (CTcpSerial*)lpThreadParameter;
        WSADATA wsaData;
        SOCKET serverSocket;
        struct sockaddr_in serverAddr, clientAddr;
        int addrSize = sizeof(clientAddr);

        WSAStartup(MAKEWORD(2, 2), &wsaData);
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
        serverAddr.sin_port = htons(8080);        // Port 8080
        bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
        listen(serverSocket, SOMAXCONN);
        while (1) 
        {
            This->clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &addrSize);
            if (This->clientSocket == INVALID_SOCKET) continue;
            trace("Client connected.\n");
            char buffer[1024];
            while (true) 
            {
                int bytesReceived = recv(This->clientSocket, buffer, sizeof(t)-This->pos, 0);
                if (bytesReceived <= 0) break; 
                buffer[bytesReceived]= 0;
                trace("\r\nTCP Received: %s\r\n", buffer);
                EnterCriticalSection(&(This->cs));
                memcpy(This->t+This->pos, buffer, bytesReceived), This->pos+= bytesReceived;
                LeaveCriticalSection(&(This->cs));
            }
            closesocket(This->clientSocket); This->clientSocket= INVALID_SOCKET;
        }
    }
    size_t read(char *b, size_t nb, int timeoutms) override 
    { 
        EnterCriticalSection(&cs);
        if (nb>pos) nb= pos; memcpy(b, t, nb); pos= 0;
        LeaveCriticalSection(&cs);
        return nb;
    }
    void sendBuf() override 
    { 
        *o= 0; trace("TCP out: %s\r\n", outBuf);
        if (clientSocket!=INVALID_SOCKET) send(clientSocket, outBuf, int(o-outBuf), 0); o= outBuf; 
    } 

    void begin() 
    { 
        InitializeCriticalSection(&cs); 
        CreateThread(NULL, 0, sockListen, this, 0, NULL);
    }
} TcpSerial;

static uint32_t getStartTime()
{
    return 0; // to match real HW..
    libsgp4::DateTime now= libsgp4::DateTime::Now(), epoch= libsgp4::DateTime(2026, 1, 1); 
    return uint32_t((now-epoch).TotalSecondsInt());
}
#endif


void controlTask(void*)
{
    // init all serial devices
    for (int i=0; i<sizeof(CSerial::serials)/sizeof(CSerial::serials[0]) && CSerial::serials[i]!=nullptr; i++)
        CSerial::serials[i]->begin();
    beginUI();
    while (true)
    {
        // process all serial input
        for (int i=0; i<sizeof(CSerial::serials)/sizeof(CSerial::serials[0]) && CSerial::serials[i]!=nullptr; i++)
            CSerial::serials[i]->process();
        // handle UI. Has a blocking I2C send which will throttle this loop at 30 times per second
        doUI();
    }
}

void initSystem()
{
    savedData.load();
    scope.init(getStartTime(), savedData.Latitude/36000.0, savedData.Longitude/36000.0, savedData.Altitude, savedData.raAmplitude); // init scope!
    xTaskCreate(HWLoop, "HWLoop", 4096, nullptr, 2, nullptr);
    xTaskCreate(controlTask, "controlTask", 4096, nullptr, 2, nullptr);
}

