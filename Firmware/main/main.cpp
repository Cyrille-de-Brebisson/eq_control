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



#include "../eqControl_Ino/eqControl_Ino.ino"

static void UITask(void*)
{
    display.begin();
    while (true) doUI();
}
static void SerialTask(void*)
{
    MSerial::begin();
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

extern "C" void app_main()
{
    Time::begin(); // these are needed for UI
    #ifdef HASADC // This one will setup the uart system... which will not be used after...
        CADC::begin();
    #endif
    xTaskCreate(UITask, "UI", 4096, NULL, 2, NULL);

    #ifdef HASGPS // This one will setup the uart system... which will not be used after...
        CGPS::waitGPS= true;
        if (CGPS::begin()) // During this time the UI task will up date the LCD...
        {
            for (int i=0; i<1000; i++) if (!CGPS::waitGPS || (CGPS::hasPosInfo && CGPS::hasTimeInfo)) break; else vTaskDelay(100/portTICK_PERIOD_MS); // Wait 5s or until loation/place info is there!
            CGPS::waitGPS= false;
        }
    #endif

    GPIOSetup();

    alpaca= new CAlpaca("CdBTelescopeServer", "CdB", "Alpaca CdB eq telescope", "Ardeche"); // done here as it initializes the storage and provides access facilities for CSavedData::savedData.load()

    MRa.powerOn(); MDec.powerOn(); MFocus.powerOn(); MDecIsOn=-1; MDecOn(); // This works when power is off because the DC-DC back powers from the ESP32 5V! But this might not be true in next version! It also initializes the serial port...
    CSavedData::savedData.load(); // motors are initialized here.. This includes a "begin" which will include serial comuncations... which is a problem with 

    if (alpaca->wifi[0]==0) { strcpy(alpaca->wifi, "EqControl"); alpaca->wifip[0]= 0; CSavedData::savedData.guidingBits&= ~0x40; } // Make sure we have connection..
    startWifi(alpaca->wifi, alpaca->wifip, "eqControl", (CSavedData::savedData.guidingBits&0x40)==0);
    alpaca->addDevice(new CMyTelescope(0));
    alpaca->addDevice(new CMyFocuser(0));
    alpaca->start(80);

    xTaskCreate(SerialTask, "Serial", 2048, NULL, 2, NULL);

    #ifdef HASGPS // This one will setup the uart system... which will not be used after...
        if (CGPS::hasPosInfo && CGPS::hasTimeInfo)
        {
            double sd= CGPS::localSiderealTime();
            if (scopeWest()) sd-= 6.0f; else sd+= 6.0f; // setup ra depending on side of pier!
            while (sd<0.0f) sd+= 24.0f; while (sd>24.0f) sd-= 24.0f;
            sync(int(sd*3600.0), 90*3600L);
            // Set alpaca gps positions?
        }
    #endif

    candisplay= true;

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

    // update motor speed and handle flip 100 times per second...
    while (true) 
    {
        quantizePowerFlip(); // quantize motor speed, handles power and meridian flip...
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}
