#include "ble_uart.h"

static char const * advertizingName;
static StreamBufferHandle_t xStreamBuffer = NULL;

static void ble_uart_on_rx(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) return;
    xStreamBufferSend(xStreamBuffer, data, len, 0);
}

static void BTTask(void*);

class CBLESerial : public CSerial { public:
    static void begin()
    {
        advertizingName= "EQControl";
        // 1. Initialize NVS (Required for Wi-Fi / Bluetooth)
        //Serial.flush("BT begin\n");
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
        { ESP_ERROR_CHECK(nvs_flash_erase()); ret = nvs_flash_init(); }
        ESP_ERROR_CHECK(ret);

        if (xStreamBuffer == NULL) xStreamBuffer = xStreamBufferCreate(512, 1);
        xTaskCreate(BTTask, "Serial", 2048, NULL, 2, NULL);

        ble_uart_config_t config = {
            .encrypted = false,
            .device_name = advertizingName,
            .ble_uart_on_rx = ble_uart_on_rx,
        };
        ESP_ERROR_CHECK(ble_uart_install(&config));
        ESP_ERROR_CHECK(ble_uart_open());
    }
    int16_t read() override
    { 
        char c;
        int len = xStreamBufferReceive(xStreamBuffer, &c, 1, 20 / portTICK_PERIOD_MS);
        if (len!=1) return -1; return c;
    }
    int read(uint8_t *d, int size) override
    { 
        return xStreamBufferReceive(xStreamBuffer, (char*)d, size, 1000 / portTICK_PERIOD_MS);
    }
    void flush(char const *s, int size) override { ble_uart_tx((uint8_t const *)s, size); }
} BLESerial;

static void BTTask(void*)
{
    TSerialContext serialContext;
    while (true)
    {
        uint8_t d[64]; int l= BLESerial.read(d, sizeof(d)); // blocking...
        if (l>0) processSerial((char*)d, l, serialContext, BLESerial);
    }
}