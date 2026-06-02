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
    while (true) doUI();
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
    Quaternion conjugate() { return Quaternion(w, -x, -y, -z); }
};


void quatToAzAlt(Quaternion const &q, float &alt, float &az)
{
    // 1. Conversion Quaternion -> Angles d'Euler (Alt/Az)
    // Note: L'ordre dépend du montage. Ici, nous supposons le montage standard.
    // roll, pitch, yaw;
    
    // Altitude (Pitch)
    float sin_alt = 2.0f * (q.w * q.y - q.z * q.x);
    alt = asinf(sin_alt);

    // Azimuth (Yaw)
    float y_az = 2.0f * (q.w * q.z + q.x * q.y);
    float x_az = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    az = atan2f(y_az, x_az);
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
    if (cos_ha > 1.0f) cos_ha = 1.0f;
    if (cos_ha < -1.0f) cos_ha = -1.0f;
    
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
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x29, 
            .scl_speed_hz = 400000,
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(I2C.bus_handle, &dev_config, &dev_handle2));
    }
    void writeReg(uint8_t reg, uint8_t v)
    {
        uint8_t t[2]= { reg, v };
        i2c_master_transmit(dev_handle2, t, 2, 1000/portTICK_PERIOD_MS);
        // printf("write %x = %02x\r\n", reg, v);
    }
    bool read(uint8_t reg, int len, uint8_t *buffer)
    {
        memset(buffer, 255, len);
        int ret2= i2c_master_transmit_receive(dev_handle2, &reg, 1, buffer, len, 1000/portTICK_PERIOD_MS);
        printf("read %x (%d):%d-> %02x %02x %02x %02x %02x %02x\r\n", reg, len, ret2, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
        return 0==ret2;

        int ret= i2c_master_transmit(dev_handle2, &reg, 1, 1000/portTICK_PERIOD_MS);
        if (ret!=0) { printf("BNO write error %d\r\n", ret); return false; }
        ret= i2c_master_receive(dev_handle2, buffer, len, 1000/portTICK_PERIOD_MS);
        printf("read %x (%d):%d-> %02x %02x %02x %02x %02x %02x\r\n", reg, len, ret, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);
        return 0==ret;
    }
    void getCalib(uint8_t d[22]) { read(0x55, 22, d); } // get the calibration data from the sensor to save it to flash and reuse later at startup...
    void setCalib(uint8_t const d[22])  // reset calib data to the sensor
    { 
        uint8_t t[23]; t[0]= 0x55; memcpy(t+1, d, 22);
        i2c_master_transmit(dev_handle2, t, 23, 1000/portTICK_PERIOD_MS);

    }
    bool hasBN0()
    {
        printf("Start BNO\r\n");
        starti2c();
        uint8_t b= 0; // register 0 is chip id, which should be a0
        i2c_master_transmit(dev_handle2, &b, 1, 1000/portTICK_PERIOD_MS);
        int ret= i2c_master_receive(dev_handle2, &b, 1, 1000/portTICK_PERIOD_MS);
        printf("Start BNO recev %d %0x\r\n", ret, b);
        if (ESP_OK!=ret) return false;
        return b==0xa0; // check chip id
    }
    void begin(uint8_t const *calibData= nullptr)
    {
        printf("BNO begin calib:%s\r\n", calibData!=nullptr?"yes":"no");
        starti2c();
        writeReg(0x3f, 0x20);   // reboot
        vTaskDelay(700/portTICK_PERIOD_MS); // wait for reboot
        uint8_t b[10];
        read(0, 1, b);          // readchip id, should be A0
        writeReg(0x7, 0);       // set page 0
        writeReg(0x3d, 0); vTaskDelay(30 / portTICK_PERIOD_MS); // set in config mode 
        writeReg(0x3f, 0);      // sys trigger... don't know what that means...
        read(0x3f, 1, b);       // read mode (should be 0)
        writeReg(0x3f, 0x80);   // use external cristal (the 0x80 is an or of what is read above and 0x80 flag)
        if (calibData!=nullptr) setCalib(calibData);
        //writeReg(0x3d, 0x0c); vTaskDelay(30 / portTICK_PERIOD_MS); // set operating mode to Ndof (normal fusion mode)
        writeReg(0x3d, 0x0b); vTaskDelay(30 / portTICK_PERIOD_MS); // set operating mode to NDOF_FMC_OFF (fusion but with limited magnetometer as it is most likely inaccurate)
    }
    uint8_t getTemp()
    {
        uint8_t b; read(0x34, 1, &b);
        return b;
    }
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
    bno055_vector_t getVectorEuler()
    {
        //setPage(0);
        uint8_t buffer[6]; read(0x1a, 6, buffer);
        float scale = 16; // or 900 for radians... A bit in register 3B allows to switch from deg to rad... look at documention..
        bno055_vector_t xyz;
        xyz.x = (int16_t)((buffer[1] << 8) | buffer[0]) / scale;
        xyz.y = (int16_t)((buffer[3] << 8) | buffer[2]) / scale;
        xyz.z = (int16_t)((buffer[5] << 8) | buffer[4]) / scale;
        return xyz;
    }
    bool getQuaternion(Quaternion &q) 
    {
        uint8_t buffer[8]; if (!read(0x20, 8, buffer)) return false; // quaternions as s.1.14 bit precision integers
        float scale = 1 << 14;
        q= Quaternion(  (int16_t)((buffer[1] << 8) | buffer[0]) / scale,
                            (int16_t)((buffer[3] << 8) | buffer[2]) / scale,
                            (int16_t)((buffer[5] << 8) | buffer[4]) / scale,
                            (int16_t)((buffer[7] << 8) | buffer[6]) / scale);
        return true;
    }
};

    // Convertit RA/Dec/LST/Lat en Alt/Az (en radians)
    void raDecToAltAz(float ra, float dec, float lst, float lat, float *alt, float *az) 
    {
        float ra_rad = ra * 15.0f * M_PI / 180.0f; // RA de heures à radians
        float dec_rad = dec * M_PI / 180.0f;
        float lst_rad = lst * 15.0f * M_PI / 180.0f;
        float lat_rad = lat * M_PI / 180.0f;
        float ha = lst_rad - ra_rad;

        // Calcul de l'Altitude
        *alt = asinf(sinf(dec_rad) * sinf(lat_rad) + cosf(dec_rad) * cosf(lat_rad) * cosf(ha));

        // Calcul de l'Azimuth
        float y = -sinf(ha);
        float x = tanf(dec_rad) * cosf(lat_rad) - sinf(lat_rad) * cosf(ha);
        *az = atan2f(y, x);
    }
    // Transforme Alt/Az en Quaternion cible
    // Note: Adaptez l'ordre selon l'orientation de votre BNO sur le tube
    Quaternion target_to_quat(float alt, float az) 
    {
/*
Pour que la fonction target_to_quat fonctionne, nous devons faire correspondre la rotation "Céleste" à la rotation "Capteur".
Généralement, pour un télescope :
    L'Azimut fera tourner le capteur autour de son axe Z.
    L'Altitude fera pivoter le capteur autour de son axe Y.
Cette fonction utilise une séquence spécifique.
Si vous changez l'ordre (par exemple faire l'Altitude avant l'Azimut), le quaternion final sera différent.
Voici trois variantes courantes selon la position de votre capteur :
Cas A : Le capteur est à plat (standard)
    Z = Azimut, Y = Altitude.
Cas B : Le capteur est sur le côté du tube

Si le capteur est "debout" sur le flanc du tube, l'axe qui servait à l'altitude devient l'axe X. Vous devez alors modifier la fonction :
Exemple pour une rotation X-Z au lieu de Y-Z
q.w = cl * ca;
q.x = sl * ca;
q.y = cl * sa;
q.z = -sl * sa; 

Comment vérifier votre ordre de rotation ?
    Posez le télescope à l'horizontale (Alt = 0) vers le Nord (Az = 0).
    Lisez le quaternion.
    Faites monter le télescope à 45° sans bouger l'azimut.
    Regardez quelle valeur change le plus dans les angles d'Euler renvoyés par le BNO.
        Si c'est le Pitch, votre axe d'altitude est Y.
        Si c'est le Roll, votre axe d'altitude est X.

Si vous n'êtes pas sûr de l'ordre, utilisez la fonction quat_multiply au lieu de reconstruire le quaternion à la main.
Plutôt que d'essayer de deviner la formule mathématique de target_to_quat, vous pouvez définir deux quaternions de base :
    q_az = {cos(az/2), 0, 0, sin(az/2)} (Rotation pure autour de Z)
    q_alt = {cos(alt/2), 0, sin(alt/2), 0} (Rotation pure autour de Y)
    Et faire : q_target = quat_multiply(q_az, q_alt);
C'est beaucoup plus facile à déboguer car vous pouvez inverser l'ordre des deux lignes pour voir laquelle correspond à votre montage.
*/        
        float cy = cosf(az * 0.5f);
        float sy = sinf(az * 0.5f);
        float cp = cosf(alt * 0.5f);
        float sp = sinf(alt * 0.5f);
        return Quaternion (cy * cp, cy * sp, sy * cp, -sy * sp);
    }
// CALIBRATION : compute offset (distance) between sky and sensor.
Quaternion compute_calibration_offset(Quaternion q_sensor, float ra, float dec, float lst, float lat) 
{
    float alt, az; raDecToAltAz(ra, dec, lst, lat, &alt, &az); // ra/dec to alt/az
    Quaternion q_target = target_to_quat(alt, az);             // compute quaternion representation
    return q_target*(q_sensor.conjugate());    // compute offset : Q_target / Q_sensor. But 1/q = cong(q) when |q|=1 so offset=Q_target * conj(Q_sensor)
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
}
void BNOTask(void *)
{
    printf("StartBNO\r\n");
    struct { uint8_t calib[22]; Quaternion offset1, offset2; } BNOCalib;
    if (alpaca->load("BNO", (uint8_t*)&BNOCalib, sizeof(BNOCalib))) BNO055::begin(BNOCalib.calib), BNOData.hasOffset1= true; else BNO055::begin();
    vTaskDelay(1000/portTICK_PERIOD_MS); // wait 1s for startup
    while (true)
    {
        vTaskDelay(100/portTICK_PERIOD_MS); // ten times per second????
        if (!BNO055::getQuaternion(BNOData.angle)) { BNOData.hasBNO= false; continue; }
        BNOData.temp= BNO055::getTemp();
        BNOData.hasBNO= true;
        //printf("BNO tmp%d\r\n", BNOData.temp);
        quatToAzAlt(BNOCalib.offset1*BNOData.angle, BNOData.alt, BNOData.az);
        float lst= -100;
        float lat= CSavedData::savedData.Latitude/((36000.0f*180.0f)*M_PI); // get latitude from wherever we can!
        #ifdef HASGPS
            if (CGPS::hasPosInfo) lat= CGPS::latitude; // get latitude from wherever we can!
            if (CGPS::hasTimeInfo) lst= CGPS::localSiderealTime();
            else 
        #endif
        if (MyTelescope->UTCTimeDelta!=0)
        {
            lst= fmodf((MyTelescope->UTCTimeDelta + Milisecond())*(1.00273790935/36000.0f), 24.0f) + (6+39/60.0f+45/3600.0f); // lst at grenwitch
            lst+= CSavedData::savedData.Latitude/36000.0f; // add latitude in 24h
        }
        if (lst!=-100) altAzToRaDec(BNOData.alt, BNOData.az, lat, lst, BNOData.ra, BNOData.dec);
        else BNOData.ra= BNOData.dec= NAN;
        BNOData.scopeEast= telescopeEastFromQuaternion(BNOData.angle);
        if (BNOData.calibrateHere)
        {
            BNOCalib.offset1= compute_calibration_offset(BNOData.angle, MRaposInReal()/3600.0f, MDec.posInReal()/3600.0f, lst, lat);
            BNO055::getCalib(BNOCalib.calib);
            alpaca->save("BNO", (uint8_t*)&BNOCalib, sizeof(BNOCalib));
            BNOData.calibrateHere= false;
            BNOData.hasOffset1= true;
        }
    }
}


extern "C" void app_main()
{
    Time::begin(); // these are needed for UI
    #ifdef HASADC // This one will setup the uart system... which will not be used after...
        CADC::begin();
    #endif
    MSerial::begin();

    #ifdef HASGPS
        CGPS::begin();
    #endif

    GPIOSetup();

    alpaca= new CAlpaca("CdBTelescopeServer", "CdB", "Alpaca CdB eq telescope", "Ardeche"); // done here as it initializes the storage and provides access facilities for CSavedData::savedData.load()

    MRa.powerOn(); MDec.powerOn(); MFocus.powerOn(); MDecIsOn=-1; MDecOn(); // This works when power is off because the DC-DC back powers from the ESP32 5V! But this might not be true in next version! It also initializes the serial port...
    CSavedData::savedData.load(); // motors are initialized here.. This includes a "begin" which will include serial comuncations... which is a problem with 

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
        #ifdef HASGPS // This one will setup the uart system... which will not be used after...
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
