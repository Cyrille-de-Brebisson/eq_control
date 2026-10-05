#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

#define TMC // always defined in ESP mode...
//#define HARMONIC_MAIN // Used for harmonic dual board system... will cut off UI

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


class CSerial;
void sendBNO(CSerial &serial);
void execBNO(uint32_t i, uint32_t j);


#include "../eqControl_Ino/eqControl_Ino.ino"
#include "BLE.h"

#ifdef HARMONIC_MAIN // Used for harmonic dual board system... will cut off UI
class CHarmonicSerial : public CSerial { public:
    static void begin() 
    {
        const uart_config_t uart_config = { .baud_rate= 9600, .data_bits= UART_DATA_8_BITS, .parity= UART_PARITY_DISABLE, .stop_bits= UART_STOP_BITS_1, .flow_ctrl= UART_HW_FLOWCTRL_DISABLE };
        uart_driver_install(UART_NUM_0, 1024*2, 1024*2, 0, NULL, 0);
        uart_param_config(UART_NUM_0, &uart_config);
        uart_set_pin(UART_NUM_0, 5, 6, -1, -1); // use pins 5 and 6 for send/recv
    }
    int16_t read() override
    { 
        char c;
        int len= uart_read_bytes(UART_NUM_0, &c, 1, 20 / portTICK_PERIOD_MS);
        if (len!=1) return -1; return c;
    }
    int read(uint8_t *d, int size) override
    { 
        return  uart_read_bytes(UART_NUM_0, (char*)d, size, 1000 / portTICK_PERIOD_MS);
    }
    void flush(char const *s, int size) override { uart_write_bytes(UART_NUM_0, s, size); }
} harmonicSerial;
TSerialContext harmonicSerialContext;
static void harmonicSerialTask(void*)
{
    harmonicSerial.begin();
    while (true)
    {
        uint8_t d[64]; int l= harmonicSerial.read(d, sizeof(d)); // blocking...
        if (l>0) processSerial((char*)d, l, harmonicSerialContext, harmonicSerial);
    }
}
#endif


static void UITask(void*)
{
    display.begin();
    while (true) { vTaskDelay(1); doUI(); } // minimum delay to give other task time to do something
}
static void SerialTask(void*)
{
    while (true)
    {
        uint8_t d[64]; int l= Serial.read(d, sizeof(d)); // blocking...
        if (l>0) processSerial((char*)d, l, serialContext, Serial);
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

#include "bno.cpp"

extern "C" void app_main()
{
    Time::begin();
    Serial.begin();
    GPIOSetup();
    #ifdef HASADC
        CADC::begin();
    #endif
    #ifdef HASGPS
        //CGPS::begin();
    #endif

    alpaca= new CAlpaca("CdBTelescopeServer", "CdB", "Alpaca CdB eq telescope", "Ardeche"); // done here as it initializes the storage and provides access facilities for CSavedData::savedData.load()

    MRa.powerOn(); MDec.powerOn(); MFocus.powerOn(); MDecIsOn=-1; MDecOn(); // This works when power is off because the DC-DC back powers from the ESP32 5V! But this might not be true in next version! It also initializes the serial port...
    CSavedData::savedData.load(); // motors are initialized here.. This includes a "begin" which will include serial comuncations... which is a problem with TMC that needs power for that to work...

    if (alpaca->wifi[0]==0) { strcpy(alpaca->wifi, "EqControl"); alpaca->wifip[0]= 0; CSavedData::savedData.guidingBits&= ~0x40; } // Make sure we have connection..
    //startWifi(alpaca->wifi, alpaca->wifip, "eqControl", (CSavedData::savedData.guidingBits&0x40)==0);
    alpaca->addDevice(MyTelescope= new CMyTelescope(0));
    alpaca->addDevice(new CMyFocuser(0));
    //alpaca->start(80);

    xTaskCreate(SerialTask, "Serial", 2048, NULL, 2, NULL);
    BLESerial.begin();

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

    #ifndef HARMONIC_MAIN
        xTaskCreate(UITask, "UI", 4096, NULL, 2, NULL);
    #else
        xTaskCreate(harmonicSerialTask, "HSerial", 2048, NULL, 2, NULL);
    #endif
    //xTaskCreate(BNOTaskTest, "BNO", 4096, NULL, 2, NULL);
    //xTaskCreate(BNOTask, "BNO", 4096, NULL, 2, NULL);

    // update motor speed and handle flip 100 times per second...
    bool wasGpsSynced= false;
    int oneSec= 100;
    while (true) 
    {
        quantizePowerFlip(); // quantize motor speed, handles power and meridian flip...
        vTaskDelay(10/portTICK_PERIOD_MS);
        if (--oneSec==0) { oneSec= 100; stopIfUnder(); } // once per second, if under the horizon, stop!
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
