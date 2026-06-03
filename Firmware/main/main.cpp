#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

#define TMC // always defined in ESP mode...
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <math.h>
#include <string.h>


void sendBNO();
void execBNO(uint32_t i, uint32_t j);


#include "../eqControl_Ino/eqControl_Ino.ino"

static void UITask(void*)
{
    display.begin();
    while (true) { vTaskDelay(1); doUI(); } // minimum delay to give other task time to do something
}
static void SerialTask(void*)
{
    while (true)
    {
        uint8_t d[64]; int l= MSerial::read(d, sizeof(d)); // blocking...
        if (l>0) processSerial((char*)d, l);
    }
}
static bool IRAM_ATTR stepperTick(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    uint32_t now= Time::unow();
    MRa.step(now); MDec.step(now); MFocus.step(now); 
    return pdFALSE;
}

#include "localAlpaca.h"

	static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
	{
	    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) esp_wifi_connect();
	    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) { esp_wifi_connect(); ipaddr = 0; } 
	    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) ipaddr = ((ip_event_got_ip_t*) event_data)->ip_info.ip.addr;
	    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) ipaddr= 0x0104A8C0;
	}

	static wifi_init_config_t wificfg;
void startWifi(const char *net, const char *pass, const char *hostname, bool accessPoint)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) { ESP_ERROR_CHECK(nvs_flash_erase()); ret = nvs_flash_init(); }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (accessPoint) esp_netif_set_hostname(esp_netif_create_default_wifi_ap(), hostname);
    else esp_netif_set_hostname(esp_netif_create_default_wifi_sta(), hostname);

    wificfg = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&wificfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config; memset(&wifi_config, 0, sizeof(wifi_config));

    if (!accessPoint) 
    {
        wifi_config.sta.threshold.authmode = pass[0]!=0 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
        strncpy((char *)wifi_config.sta.ssid, net, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.pmf_cfg.capable = true;
        wifi_config.sta.pmf_cfg.required = false;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    } else {
        strncpy((char*)wifi_config.ap.ssid, net, sizeof(wifi_config.ap.ssid)-1);
        strncat((char*)wifi_config.ap.ssid, "_AP", sizeof(wifi_config.ap.ssid)-1);
        wifi_config.ap.ssid_len= strlen(net)+3;
        wifi_config.ap.channel = 6;
        wifi_config.ap.max_connection = 4,
        wifi_config.ap.authmode = WIFI_AUTH_OPEN, // WIFI_AUTH_WPA2_PSK,
        wifi_config.ap.pmf_cfg.required = true;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////                          BNO055 part...                     ///////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct Vec3 { float x=0.0f, y=0.0f, z=0.0f; };
static Vec3 normalizeVec3(Vec3 v)
{
    float mag = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if (mag <= 1e-6f) return {0.0f, 0.0f, 0.0f};
    return { v.x / mag, v.y / mag, v.z / mag };
}
static float dotVec3(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 crossVec3(Vec3 a, Vec3 b) { return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x }; }
static Vec3 scaleVec3(Vec3 v, float s) { return { v.x * s, v.y * s, v.z * s }; }
static Vec3 addVec3(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }

static const Vec3 SENSOR_FORWARD = { 1.0f, 0.0f, 0.0f }; // Adjust if your sensor forward axis is different.

class Quaternion { public:
    float w=0.0f, x=0.0f, y=0.0f, z=0.0f;
    Quaternion(float w=0.0f, float x=0.0f, float y=0.0f, float z=0.0f): w(w), x(x), y(y), z(z) { }
    Quaternion operator*(Quaternion const &b) const
    {
        return Quaternion(
        w * b.w - x * b.x - y * b.y - z * b.z,
        w * b.x + x * b.w + y * b.z - z * b.y,
        w * b.y - x * b.z + y * b.w + z * b.x,
        w * b.z + x * b.y - y * b.x + z * b.w);
    }
    Quaternion &operator*=(Quaternion const &b)
    {
        float _w= w * b.w - x * b.x - y * b.y - z * b.z;
        float _x= w * b.x + x * b.w + y * b.z - z * b.y;
        float _y= w * b.y - x * b.z + y * b.w + z * b.x;
        float _z= w * b.z + x * b.y - y * b.x + z * b.w;
        w=_w; x=_x; y=_y; z=_z;
        return *this;
    }
    // Conjugué d'un quaternion (équivalent à l'inverse pour un quat unitaire)
    Quaternion conjugate() const { return Quaternion(w, -x, -y, -z); }
    Vec3 rotate(Vec3 const &v) const
    {
        Quaternion qv(0.0f, v.x, v.y, v.z);
        Quaternion r = (*this) * qv * conjugate();
        return { r.x, r.y, r.z };
    }
    void printEuler(char const *end="\n")
    {
        // 1. Roll (X-axis rotation)
        float sinr_cosp = 2.0 * (w * x + y * z);
        float cosr_cosp = 1.0 - 2.0 * (x * x + y * y);
        float roll = atan2f(sinr_cosp, cosr_cosp)*180.0f/M_PI;
        // 2. Pitch (Y-axis rotation)
        float sinp = 2.0 * (w * y - z * x);
        float pitch;
        // Handle Gimbal Lock safety boundary condition Safely (-90 to 90 degrees)
        if (fabsf(sinp) >= 1.0) pitch = copysign(90.0f, sinp);  // Use copysign to handle edge truncation boundary safely
        else pitch = asinf(sinp)*180.0f/M_PI;
        // 3. Yaw (Z-axis rotation)
        float siny_cosp = 2.0 * (w * z + x * y);
        float cosy_cosp = 1.0 - 2.0 * (y * y + z * z);
        float yaw = atan2f(siny_cosp, cosy_cosp)*180.0f/M_PI;      

        printf("roll:%.2f pitch:%.2f yaw:%.2f%s", roll, pitch, yaw, end);
    }
};

static void directionToAltAz(Vec3 const &dir, float &alt, float &az)
{
    Vec3 d = normalizeVec3(dir);
    alt = atan2f(d.z, sqrtf(d.x*d.x + d.y*d.y));
    az = atan2f(d.x, d.y);
    if (az < 0.0f) az += 2.0f * M_PI;
}

static Vec3 altAzToDirection(float alt, float az)
{
    float ca = cosf(alt);
    return { sinf(az) * ca, cosf(az) * ca, sinf(alt) };
}

static Quaternion quaternionBetweenVectors(Vec3 from, Vec3 to)
{
    Vec3 f = normalizeVec3(from);
    Vec3 t = normalizeVec3(to);
    float cosTheta = dotVec3(f, t);
    if (cosTheta >= 1.0f - 1e-6f) return Quaternion(1.0f, 0.0f, 0.0f, 0.0f);
    if (cosTheta <= -1.0f + 1e-6f)
    {
        Vec3 axis = crossVec3({1.0f, 0.0f, 0.0f}, f);
        if (sqrtf(dotVec3(axis, axis)) < 1e-6f) axis = crossVec3({0.0f, 1.0f, 0.0f}, f);
        axis = normalizeVec3(axis);
        return Quaternion(0.0f, axis.x, axis.y, axis.z);
    }
    Vec3 axis = crossVec3(f, t);
    float s = sqrtf((1.0f + cosTheta) * 2.0f);
    float invs = 1.0f / s;
    return Quaternion(s * 0.5f, axis.x * invs, axis.y * invs, axis.z * invs);
}

void quatToAzAlt(Quaternion const &q, float &alt, float &az)
{
    Vec3 dir = q.rotate(SENSOR_FORWARD);
    directionToAltAz(dir, alt, az);
}
void altAzToRaDec(float alt, float az, float lat_rad, float lst, float &ra, float &dec) 
{
    // 2. Conversion Alt/Az -> Dec / Hour Angle (HA)
    // float lat_rad = lat * (M_PI / 180.0f);
    
    // Calcul de la Déclinaison (Dec)
    float sin_dec = sinf(alt) * sinf(lat_rad) + cosf(alt) * cosf(lat_rad) * cosf(az);
    float tdec = asinf(sin_dec);

    // Calcul de l'Angle Horaire (HA)
    float cos_ha = (sinf(alt) - sinf(lat_rad) * sinf(tdec)) / (cosf(lat_rad) * cosf(tdec));
    
    // Sécurité pour les erreurs d'arrondi de floating point
    if (cos_ha > 1.0f) cos_ha = 1.0f; if (cos_ha < -1.0f) cos_ha = -1.0f;
    
    float ha = acosf(cos_ha);
    
    // Si l'Azimuth est à l'Est du méridien, l'HA est négatif
    if (sinf(az) > 0.0f) ha = 2.0f * M_PI - ha;

    // 3. Conversion HA -> Right Ascension (RA)
    float ha_hours = ha * (180.0f / M_PI) / 15.0f;
    float tra = lst - ha_hours;

    // Normalisation de la RA entre 0 et 24h
    while (tra < 0.0f) tra += 24.0f;
    while (tra >= 24.0f) tra -= 24.0f;
    // write output...
    ra= tra;
    dec = tdec * (180.0f / M_PI);
}

struct bno055_calibration_t { uint8_t sys, gyro, accel, mag; };
struct bno055_vector_t { float x, y, z; };
namespace BNO055 {
    i2c_master_dev_handle_t dev_handle2= nullptr;
    void starti2c()
    {
        if (dev_handle2!=nullptr) return;
        I2C.begin();
        i2c_device_config_t dev_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x29, .scl_speed_hz = 400000 };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(I2C.bus_handle, &dev_config, &dev_handle2));
    }
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
    bool hasBN0()
    {
        starti2c();
        vTaskDelay(700/portTICK_PERIOD_MS); // wait for reboot
        uint8_t b; read(0, 1, &b); //  register 0 is chip id, which should be a0
        //printf("Start BNO recev %0x\r\n", b);
        return b==0xa0; // check chip id
    }
    void begin(uint8_t const *calibData= nullptr)
    {
        //printf("BNO begin calib:%s\r\n", calibData!=nullptr?"yes":"no");
        //writeReg(0x3f, 0x20); vTaskDelay(700/portTICK_PERIOD_MS); // reboot
        writeReg(0x7, 0);       // set page 0
        writeReg(0x3d, 0); vTaskDelay(50/portTICK_PERIOD_MS); // set in config mode 
        writeReg(0x40, 1); // reg 40 is temp source Set to gyro which is supposed to be better
        writeReg(0x41, 0x21); // axis mapping. inverts x and y here... should be parametrized? 2 bit per axis
        writeReg(0x42, 0);    // axis direction inversion (1 bit per axis)
        writeReg(0x3e, 0); vTaskDelay(50/portTICK_PERIOD_MS); // normal power mode
        uint8_t b; read(0x3f, 1, &b);         // read sys trigger (should be 0)
        writeReg(0x3f, b|0x80); vTaskDelay(50/portTICK_PERIOD_MS);  // use external cristal
        //if (calibData!=nullptr) setCalib(calibData);
        //writeReg(0x3d, 0x0c); vTaskDelay(30 / portTICK_PERIOD_MS); // set operating mode to Ndof (normal fusion mode)
        writeReg(0x3d, 0x0c); vTaskDelay(50 / portTICK_PERIOD_MS); // set operating mode to full fusion. // b is no magnetometer NDOF_FMC_OFF (fusion but with limited magnetometer as it is most likely inaccurate) 
    }
    uint8_t getTemp() { uint8_t b; read(0x34, 1, &b); return b; } // temperature...
    bno055_calibration_t getCalibration()
    {
        uint8_t calData; read(0x35, 1, &calData);
        bno055_calibration_t cal;
        cal.sys = (calData >> 6) & 0x03;
        cal.gyro = (calData >> 4) & 0x03;
        cal.accel = (calData >> 2) & 0x03;
        cal.mag = calData & 0x03;
        return cal;
    }
    bool getQuaternion(Quaternion &q) 
    {
        uint8_t buffer[8]; if (!read(0x20, 8, buffer)) return false; // quaternions as s.1.14 bit precision integers
        float const scale = 1 << 14;
        q= Quaternion(  (int16_t)((buffer[1] << 8) | buffer[0]) / scale,
                        (int16_t)((buffer[3] << 8) | buffer[2]) / scale,
                        (int16_t)((buffer[5] << 8) | buffer[4]) / scale,
                        (int16_t)((buffer[7] << 8) | buffer[6]) / scale);
        return true;
    }
};

    // Convertit RA/Dec/LST/Lat (en h24/degrees) en Alt/Az (en radians)
    void raDecToAltAz(float ra, float dec, float lst, float lat, float *alt, float *az) 
    {
        float ra_rad = ra * (15.0f * M_PI / 180.0f); // RA de heures à radians
        float dec_rad = dec * (M_PI / 180.0f);
        float lst_rad = lst * (15.0f * M_PI / 180.0f);
        float lat_rad = lat * (M_PI / 180.0f);
        float ha = lst_rad - ra_rad;

        // Calcul de l'Altitude
        *alt = asinf(sinf(dec_rad) * sinf(lat_rad) + cosf(dec_rad) * cosf(lat_rad) * cosf(ha));

        // Calcul de l'Azimuth
        float y = -sinf(ha);
        float x = tanf(dec_rad) * cosf(lat_rad) - sinf(lat_rad) * cosf(ha);
        *az = atan2f(y, x);
    }
    // Transforme Alt/Az en Quaternion cible
    // Ici on crée une rotation minimale qui aligne l'axe de visée du capteur avec la cible.
    Quaternion target_to_quat(float alt, float az) // az/alt in radians...
    {
        Vec3 targetDir = altAzToDirection(alt, az);
        return quaternionBetweenVectors(SENSOR_FORWARD, targetDir);
    }

void getCurrentPosition(Quaternion const &q_offset, float lat, float lst, float &ra, float &dec)
{
    //float alt, az; quatToAzAlt(q_offset*BNO055::getQuaternion(), alt, az);
    //altAzToRaDec(alt, az, lat, lst, ra, dec);
}

bool telescopeEastFromQuaternion(Quaternion q) 
{
    // Calcul de l'angle de roulis (Roll) à partir du quaternion
    float roll = atan2f(2.0f * (q.w * q.x + q.y * q.z), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
    // Si le tube est "retourné" (Roll autour de 180° ou -180°)
    return fabs(roll) <= M_PI/2.0f;
}

// Utilisation:
// 1: homeing/parking position. Startup.
//    at startup, have an idea of the physical position of the scope (including side of pier) allowing for setup of ra/dec
// 2: parking at alt/az...
struct {
    Quaternion angle;
    uint8_t temp= 0;
    uint8_t hasBNO: 1= 0, hasOffset1: 1= 0, hasOffset2: 1= 0, scopeEast: 1= 0, zero:4= 0;
    uint8_t calibrateHere: 1= 0, zero2:7= 0;
    uint8_t t2= 0;
    float ra= 0.0f, dec= 0.0f, az= 0.0f, alt= 0.0f;
} BNOData;

void sendBNO()
{
    if (!BNOData.hasBNO) return;
    //printf("BNO T:%d\r\n", BNOData.temp);
    for (uint8_t i=0; i<sizeof(BNOData); i++) printHex2(((uint8_t*)&BNOData)[i], 2);
}
void execBNO(uint32_t i, uint32_t j)
{
    if (i==1) BNOData.calibrateHere= true;
    if (i==2) MyTelescope->set_utcdate(j);
}
void BNOTask(void *)
{
    if (!BNO055::hasBN0()) vTaskDelete(nullptr); // no BNO, kill task and do nothing else...
    struct { uint8_t calib[22]; Quaternion offset1, offset2; } BNOCalib;
    if (alpaca->load("BNO", (uint8_t*)&BNOCalib, sizeof(BNOCalib))) 
        BNO055::begin(BNOCalib.calib), BNOData.hasOffset1= true; 
    else
        BNO055::begin();
    while (true)
    {
        vTaskDelay(500/portTICK_PERIOD_MS); // 2hz
        if (!BNO055::getQuaternion(BNOData.angle)) { BNOData.hasBNO= false; continue; }
        BNOData.temp= BNO055::getTemp();
        BNOData.hasBNO= true;

        //printf("BNO tmp%d\r\n", BNOData.temp);
        // BNO is used for absolute positionning. It gives a AZ/ALT type orientation.
        // So to get a RA/DEC, we need to know latitude and LST (longitude + time)
        // we can get longitude/latitude from GPS or user setup.
        // but to get LST, we need time also which can can get from GPS or ascom.
        // Assumes GPS is best, if we have, else use setup+ascom time
        Quaternion calibrated= BNOCalib.offset1*BNOData.angle;
        quatToAzAlt(calibrated, BNOData.alt, BNOData.az);

        float lst= -100.0f; // this is in 24h format!
        float lat= CSavedData::savedData.Latitude/36000.0f; // get latitude from wherever we can!
        #ifdef HASGPS
            if (CGPS::hasPosInfo && CGPS::hasTimeInfo) { lat= CGPS::latitude*(180.0f/M_PI); lst= CGPS::localSiderealTime(); }
            else 
        #endif
            if (MyTelescope->UTCTimeDelta!=0)
            {
                lst= fmodf((MyTelescope->UTCTimeDelta + Milisecond())*(1.00273790935/3600.0f), 24.0f) + (6+39/60.0f+45/3600.0f); // lst at grenwitch on jan 1 2024
                lst+= CSavedData::savedData.Longitude/36000.0f; // add Longitude in 24h Note that this in in 24h format!
            }
            //printf("lst:%f %d\n", lst, int(MyTelescope->UTCTimeDelta));
        if (lst!=-100) altAzToRaDec(BNOData.alt, BNOData.az, lat, lst, BNOData.ra, BNOData.dec);
        else BNOData.ra= BNOData.dec= NAN;

        float alt, az; raDecToAltAz(MRaposInReal()/3600.0f, MDec.posInReal()/3600.0f, lst, lat, &alt, &az); // ra/dec to alt/az
        printf("alt:%.1f az:%.1f ", alt*180.0f/M_PI, az*180.0f/M_PI); BNOData.angle.printEuler(" -> "); calibrated.printEuler("  "); printf("alt:%.1f az:%.1f\n", BNOData.alt*180.0f/M_PI, BNOData.az*180.0f/M_PI);

        BNOData.scopeEast= telescopeEastFromQuaternion(BNOData.angle);
        if (lst!=-100.0f && BNOData.calibrateHere) // can not calibrate if no lst!
        {
            // This "saves" the angle offset from BNO to telescope 
            // Calculates current AZ/ALT and saves  the difference with the sensor reading.
            // it also saves the sensor callibration data
            // CALIBRATION : compute offset (distance) between sky and sensor.
            float alt, az; raDecToAltAz(MRaposInReal()/3600.0f, MDec.posInReal()/3600.0f, lst, lat, &alt, &az); // ra/dec to alt/az
            Quaternion q_target= target_to_quat(alt, az);             // compute quaternion representation
            BNOCalib.offset1= q_target*(BNOData.angle.conjugate());    // compute offset : Q_target / Q_sensor. But 1/q = cong(q) when |q|=1 so offset=Q_target * conj(Q_sensor)

            // Diagnostics: print target, sensor quaternion, offset and result of applying offset
            //printf("clibrate ra:%.1f dec:%.1f lst:%.3f lat:%.1f alt:%.1f az:%.1f\n", MRaposInReal()/3600.0f, MDec.posInReal()/3600.0f, lst, lat, alt*180.0f/M_PI, az*180.0f/M_PI);
            //printf("q_target: w=%.6f x=%.6f y=%.6f z=%.6f\n", q_target.w, q_target.x, q_target.y, q_target.z);
            //printf("q_sensor: w=%.6f x=%.6f y=%.6f z=%.6f\n", BNOData.angle.w, BNOData.angle.x, BNOData.angle.y, BNOData.angle.z);
            //printf("offset1:  w=%.6f x=%.6f y=%.6f z=%.6f\n", BNOCalib.offset1.w, BNOCalib.offset1.x, BNOCalib.offset1.y, BNOCalib.offset1.z);
            Quaternion test = BNOCalib.offset1 * BNOData.angle;
            float talt, taz; quatToAzAlt(test, talt, taz);
            //printf("after offset alt:%.2f az:%.2f\n", talt*180.0f/M_PI, taz*180.0f/M_PI);

            BNO055::getCalib(BNOCalib.calib);
            alpaca->save("BNO", (uint8_t*)&BNOCalib, sizeof(BNOCalib));
            BNOData.calibrateHere= false;
            BNOData.hasOffset1= true;
        }
    }
}


extern "C" void app_main()
{
    Time::begin();
    MSerial::begin();
    GPIOSetup();
    #ifdef HASADC
        CADC::begin();
    #endif
    #ifdef HASGPS
        CGPS::begin();
    #endif

    alpaca= new CAlpaca("CdBTelescopeServer", "CdB", "Alpaca CdB eq telescope", "Ardeche"); // done here as it initializes the storage and provides access facilities for CSavedData::savedData.load()

    MRa.powerOn(); MDec.powerOn(); MFocus.powerOn(); MDecIsOn=-1; MDecOn(); // This works when power is off because the DC-DC back powers from the ESP32 5V! But this might not be true in next version! It also initializes the serial port...
    CSavedData::savedData.load(); // motors are initialized here.. This includes a "begin" which will include serial comuncations... which is a problem with TMC that needs power for that to work...

    if (alpaca->wifi[0]==0) { strcpy(alpaca->wifi, "EqControl"); alpaca->wifip[0]= 0; CSavedData::savedData.guidingBits&= ~0x40; } // Make sure we have connection..
    startWifi(alpaca->wifi, alpaca->wifip, "eqControl", (CSavedData::savedData.guidingBits&0x40)==0);
    alpaca->addDevice(MyTelescope= new CMyTelescope(0));
    alpaca->addDevice(new CMyFocuser(0));
    alpaca->start(80);

    xTaskCreate(SerialTask, "Serial", 2048, NULL, 2, NULL);

    // setup alarm for motors!
    gptimer_handle_t gptimer;
    gptimer_config_t timer_config = { .clk_src = GPTIMER_CLK_SRC_DEFAULT, .direction = GPTIMER_COUNT_UP, .resolution_hz = 1000000 }; // 1MHz, clock
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
    gptimer_event_callbacks_t cbs = { .on_alarm = stepperTick, };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, nullptr));
    gptimer_alarm_config_t alarm_config1= { .alarm_count = 100, .reload_count = 0, .flags= {.auto_reload_on_alarm = true } }; // every 50us = 10k/s
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config1));
    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    xTaskCreate(UITask, "UI", 4096, NULL, 2, NULL);
    xTaskCreate(BNOTask, "BNO", 4096, NULL, 2, NULL);

    // update motor speed and handle flip 100 times per second...
    bool wasGpsSynced= false;
    while (true) 
    {
        quantizePowerFlip(); // quantize motor speed, handles power and meridian flip...
        vTaskDelay(10/portTICK_PERIOD_MS);
        #ifdef HASGPS // if GPS has value, read them and use them!
            if (!wasGpsSynced && CGPS::hasPosInfo && CGPS::hasTimeInfo)
            {
                if (MDec.pos>=(MDec.maxPos-(MDec.maxPos>>7)) && abs(MRa.posInReal()-(6*3600))<30)
                {
                    double sd= CGPS::localSiderealTime();
                    if (scopeWest()) sd-= 6.0f; else sd+= 6.0f; // setup ra depending on side of pier!
                    while (sd<0.0f) sd+= 24.0f; while (sd>24.0f) sd-= 24.0f;
                    sync(int(sd*3600.0), 90*3600L);
                }
                CSavedData::savedData.Longitude= int(CGPS::longitude*(180.0f*36000.0f/M_PI));
                CSavedData::savedData.Latitude= int(CGPS::latitude*(180.0f*36000.0f/M_PI));
                CSavedData::savedData.Altitude= CGPS::altitude;
                wasGpsSynced= true;
            }
        #endif
    }
}
