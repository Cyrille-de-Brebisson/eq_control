struct Vec3 { float x=0.0f, y=0.0f, z=0.0f; };

namespace BNO055 {
    i2c_master_dev_handle_t dev_handle2= nullptr;
    void writeReg(uint8_t reg, uint8_t v)
    {
        uint8_t t[2]= { reg, v };
        i2c_master_transmit(dev_handle2, t, 2, 1000/portTICK_PERIOD_MS);
        //printf("BNO write %x = %02x\r\n", reg, v);
    }
    bool read(uint8_t reg, int len, uint8_t *buffer)
    {
        int ret2= i2c_master_transmit_receive(dev_handle2, &reg, 1, buffer, len, 1000/portTICK_PERIOD_MS);
        //printf("BNO read %x (%d):%d-> %02x %02x %02x %02x %02x %02x\r\n", reg, len, ret2, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
        return 0==ret2;
    }
    void getCalib(uint8_t d[22]) { read(0x55, 22, d); } // get the calibration data from the sensor to save it to flash and reuse later at startup...
    void setCalib(uint8_t const d[22])  // reset calib data to the sensor
    { 
        uint8_t t[23]; t[0]= 0x55; memcpy(t+1, d, 22);
        i2c_master_transmit(dev_handle2, t, 23, 1000/portTICK_PERIOD_MS);
    }
    bool begin(uint8_t const *calibData= nullptr)
    {
        if (dev_handle2==nullptr)
        {
            I2C.begin();
            i2c_device_config_t dev_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x29, .scl_speed_hz = 400000 };
            ESP_ERROR_CHECK(i2c_master_bus_add_device(I2C.bus_handle, &dev_config, &dev_handle2));
        }
        vTaskDelay(700/portTICK_PERIOD_MS); // wait for reboot
        uint8_t b; read(0, 1, &b); //  register 0 is chip id, which should be a0
        //printf("Start BNO recev %0x\r\n", b);
        if (b!=0xa0) return false; // check chip id
        //printf("BNO begin calib:%s\r\n", calibData!=nullptr?"yes":"no");
        //writeReg(0x3f, 0x20); vTaskDelay(700/portTICK_PERIOD_MS); // reboot
        writeReg(0x7, 0);       // set page 0
        writeReg(0x3d, 0); vTaskDelay(50/portTICK_PERIOD_MS); // set in config mode 
        writeReg(0x40, 1); // reg 40 is temp source Set to gyro which is supposed to be better
        writeReg(0x41, 0x21); // axis mapping. inverts x and y here... should be parametrized? 2 bit per axis
        writeReg(0x42, 0);    // axis direction inversion (1 bit per axis)
        writeReg(0x3e, 0); vTaskDelay(50/portTICK_PERIOD_MS); // normal power mode
        read(0x3f, 1, &b);         // read sys trigger (should be 0)
        writeReg(0x3f, b|0x80); vTaskDelay(50/portTICK_PERIOD_MS);  // use external cristal
        //if (calibData!=nullptr) setCalib(calibData);
        writeReg(0x3d, 0x08); vTaskDelay(50 / portTICK_PERIOD_MS); // c is full fusion. // 8 is only gyro/accelerometer
        return true;
    }
    uint8_t getTemp() { uint8_t b; read(0x34, 1, &b); return b; } // temperature...
    struct bno055_calibration_t { uint8_t sys, gyro, accel, mag; };
    bno055_calibration_t getCalibrationStatus()
    {
        uint8_t calData; read(0x35, 1, &calData);
        return {uint8_t((calData >> 6) & 0x03), uint8_t((calData >> 4) & 0x03), uint8_t((calData >> 2) & 0x03), uint8_t(calData & 0x03) };
    }
    /*bool getQuaternion(Quaternion &q) 
    {
        uint8_t buffer[8]; if (!read(0x20, 8, buffer)) return false; // quaternions as s.1.14 bit precision integers
        float const scale = 1 << 14;
        q= Quaternion(  (int16_t)((buffer[1] << 8) | buffer[0]) / scale,
                        (int16_t)((buffer[3] << 8) | buffer[2]) / scale,
                        (int16_t)((buffer[5] << 8) | buffer[4]) / scale,
                        (int16_t)((buffer[7] << 8) | buffer[6]) / scale);
        return true;
    }*/
    bool getGravity(Vec3 *v) 
    {
        uint8_t buffer[6]; if (!read(0x2e, 6, buffer)) return false;
        float const scale = 100.0f;
        v->x= (int16_t)((buffer[1]<<8) | buffer[0]) / scale;
        v->y= (int16_t)((buffer[3]<<8) | buffer[2]) / scale;
        v->z= (int16_t)((buffer[5]<<8) | buffer[4]) / scale;
        return true;
    }
};

// Utilisation:
// 1: homeing/parking position. Startup.
//    at startup, have an idea of the physical position of the scope (including side of pier) allowing for setup of ra/dec
// 2: parking at alt/az...
struct {
    float angle[4];
    uint8_t temp= 0;
    uint8_t hasBNO:1=0, hasOffset1:1=0, hasOffset2:1=0, scopeEast:1=0, hasValue:1=0, zero:2=0;
    uint8_t calibrateHere:1=0, zero2:7=0;
    uint8_t t2= 0;
    float scopeaz=0.0f, scopealt=0.0f, bnoaz=0.0f, bnoalt=0.0f, lst= 0.0f; // az/alt are all in radian!!!!
} BNOData;

// These 2 functions are for interraction with the serial communications in the .ino file
void sendBNO(CSerial &serial) // send BNO data to serial
{
    if (!BNOData.hasBNO) return;
    for (uint8_t i=0; i<sizeof(BNOData); i++) printHex2(serial, ((uint8_t*)&BNOData)[i], 2);
}
void execBNO(uint32_t i, uint32_t j) // perform a BNO actions from serial command.
{
    if (i==1) BNOData.calibrateHere= true;
    if (i==2) MyTelescope->set_utcdate(j);
}

void BNOTaskTest(void *)
{
    if (!BNO055::begin()) vTaskDelete(nullptr);
    CSavedData::savedData.initUncountedStep2(0); // stop sideral!
    MRa.pos= CSavedData::savedData.ra.maxPos/2;
    MRa.maxPos= CSavedData::savedData.ra.maxPos;
    MDec.pos= CSavedData::savedData.dec.maxPos/2;
    MDec.maxPos= CSavedData::savedData.dec.maxPos;
    MDecOn();
    struct CalSample { float m1, m2; Vec3 gravity_sensor; };
    int posespos= 0;
    CalSample poses[]= {{0,0},
  { 0, 0, {}},   { 0, 15, {}},   { 0, 45, {}},   { 0, 75, {}},   { 0, 105, {}},   { 0, 135, {}},   { 0, 225, {}},   { 0, 255, {}},   { 0, 285, {}},   { 0, 315, {}},   { 0, 345, {}},
  { 45, 0, {}},   { 45, 15, {}},   { 45, 45, {}},   { 45, 75, {}},   { 45, 105, {}},   { 45, 135, {}},   { 45, 225, {}},   { 45, 255, {}},   { 45, 285, {}},   { 45, 315, {}},   { 45, 345, {}},
  { 90, 0, {}},   { 90, 15, {}},   { 90, 45, {}},   { 90, 75, {}},   { 90, 105, {}},   { 90, 135, {}},   { 90, 225, {}},   { 90, 255, {}},   { 90, 285, {}},   { 90, 315, {}},   { 90, 345, {}},
  { 100, 0, {}},   { 100, 15, {}},   { 100, 45, {}},   { 100, 75, {}},   { 100, 105, {}},   { 100, 135, {}},   { 100, 225, {}},   { 100, 255, {}},   { 100, 285, {}},   { 100, 315, {}},   { 100, 345, {}},
  { 260, 0, {}},   { 260, 15, {}},   { 260, 45, {}},   { 260, 75, {}},   { 260, 105, {}},   { 260, 135, {}},   { 260, 225, {}},   { 260, 255, {}},   { 260, 285, {}},   { 260, 315, {}},   { 260, 345, {}},
  { 270, 0, {}},   { 270, 15, {}},   { 270, 45, {}},   { 270, 75, {}},   { 270, 105, {}},   { 270, 135, {}},   { 270, 225, {}},   { 270, 255, {}},   { 270, 285, {}},   { 270, 315, {}},   { 270, 345, {}},
  { 315, 0, {}},   { 315, 15, {}},   { 315, 45, {}},   { 315, 75, {}},   { 315, 105, {}},   { 315, 135, {}},   { 315, 225, {}},   { 315, 255, {}},   { 315, 285, {}},   { 315, 315, {}},   { 315, 345, {}},
   {0,0}
};
    while (true)
    {
        vTaskDelay(1000/portTICK_PERIOD_MS); // every 1s
        if (MRa.isMoving() || MDec.isMoving()) continue; // only do this when scope is not moving!

        vTaskDelay(500/portTICK_PERIOD_MS); // wait for stabilisation after stop...

        // Take 16 readings and display them
        Vec3 r, g;
        int const nbm= 32;
        int i=0; while (i<nbm)
        {
            vTaskDelay(100/portTICK_PERIOD_MS);
            if (!BNO055::getGravity(&g)) continue;
            i++;
            r.x+= g.x, r.y+= g.y, r.z+= g.z;
        }
        r.x/= nbm, r.y/= nbm, r.z/= nbm;
        float mra= -(int32_t(MRa.pos)-int32_t(MRa.maxPos/2))*360.0f/MRa.maxPos; if (mra<0) mra+= 360;
        float mdec= -(int32_t(MDec.pos)-int32_t(MDec.maxPos/2))*360.0f/MDec.maxPos; if (mdec<0) mdec+= 360;
        printf("  {{%ff, %ff}, {%ff, %ff, %ff}},\r\n", mra, mdec, r.x, r.y, r.z);

        posespos++; if (posespos>=sizeof(poses)/sizeof(poses[0])) vTaskDelete(nullptr); // next...

        stopMovingOnKeyRelease= false;
        float ra= poses[posespos].m1; if (ra>180) ra-= 360;
        float dec= poses[posespos].m2; if (dec>180) dec-= 360;
        MRa.goToSteps(int32_t((-ra+180.0f)*MRa.maxPos/360.0f));
        MDec.goToSteps(int32_t((-dec+180.0f)*MDec.maxPos/360.0f));
        //printf("//goto %ld:%f %ld:%f\r\n", MRa.dst, poses[posespos].m1, MDec.dst, poses[posespos].m2);
    }
}

// Main BNO task.
// read BNO quaternion every 2s and calculate the scope ra/dec from it if calibrated...
// else, wait until told to calibrate here (save calibration data for next run)
void BNOTask(void *)
{
    struct { uint8_t calib[22]; float offset[4]; bool valid= false; } BNOCalib; // structure that will be saved in flash

    if (alpaca->load("BNO", (uint8_t*)&BNOCalib, sizeof(BNOCalib))) 
    {
        if (!BNO055::begin(BNOCalib.calib)) vTaskDelete(nullptr);
        BNOData.hasOffset1= true;
    } else
        if (!BNO055::begin()) vTaskDelete(nullptr);

    BNOCalib.valid= false;

    while (true)
    {
        vTaskDelay(1000/portTICK_PERIOD_MS); // every 1s

        // Get the data from sensor
        //if (!BNO055::getGravity(BNOData.angle)) { BNOData.hasBNO= false; continue; }
        BNOData.temp= BNO055::getTemp();
        BNOData.hasBNO= true;
        //printf("BNO Quat %f %f %f %f\r\n", BNOData.angle.w, BNOData.angle.x, BNOData.angle.y, BNOData.angle.z);

        // get LST from GPS or ascom time + site_latitude
        float lst= -100.0f; // this is in 24h format!
        float lat= CSavedData::savedData.Latitude/36000.0f; // get latitude from wherever we can!
        #ifdef HASGPS
            if (CGPS::hasPosInfo && CGPS::hasTimeInfo) { lat= CGPS::latitude*(180.0f/M_PI); lst= CGPS::localSiderealTime(); }
            else 
        #endif
            MyTelescope->siderealtime(lst); // get lst from ascom time+setup

        BNOData.lst= lst;
        if (lst<-99.0f) continue; // no LST, cannot do anything...
        //printf("lst:%f %d\n", lst, int(MyTelescope->UTCTimeDelta));

        raDecToAltAz(MRaposInReal()/3600.0f, MDec.posInReal()/3600.0f, lst, lat, &BNOData.scopealt, &BNOData.scopeaz); // ra/dec to alt/az
        if (BNOData.calibrateHere) // calibrate if requested and has lst
        {
            BNOData.calibrateHere= false;
            // Get current az/alt position from telescope
            // The sensor's local forward axis is the board boresight. Compute the quaternion that rotates the current
            // BNO-measured forward direction onto the known telescope direction and save it as the fixed calibration offset.
            // printf("float BNOCaliboffset[4]={ %f, %f, %f, %f };\r\n", BNOCalib.offset.w, BNOCalib.offset.x, BNOCalib.offset.y, BNOCalib.offset.z);
            BNOCalib.valid= true;
        }
        if (!BNOCalib.valid) { BNOData.hasValue= false; continue; } // not calibrated, nothing we can do!

        // Apply the saved offset before the live BNO rotation so the sensor forward vector is corrected into the
        // telescope frame, then convert the resulting world-space forward vector to az/alt.
        BNOData.hasValue= true;
    }
}
