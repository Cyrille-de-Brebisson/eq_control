#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_task_wdt.h"
#include <math.h>
#include <string.h>

int raAmplitude= 195; // assumes
int MFocusspdMax= 1600; // assumes

int MDecPos, MRAPos, MFocusPos;
bool moving, sideOfPeir, flipping, noFlip, power;
int midPos, sideralSpeed;
static void command(char const *s);

#include "eqControl_Ino.ino"

class CHarmonicSerial : public CSerial { public:
    static void begin() 
    {
        const uart_config_t uart_config = { .baud_rate= 9600, .data_bits= UART_DATA_8_BITS, .parity= UART_PARITY_DISABLE, .stop_bits= UART_STOP_BITS_1, .flow_ctrl= UART_HW_FLOWCTRL_DISABLE };
        uart_driver_install(UART_NUM_0, 1024*2, 1024*2, 0, NULL, 0);
        uart_param_config(UART_NUM_0, &uart_config);
        uart_set_pin(UART_NUM_0, 19, 18, -1, -1); // use pins 5 and 6 for send/recv
    }
    int16_t read() override
    { 
        char c;
        int len= uart_read_bytes(UART_NUM_0, &c, 1, 20 / portTICK_PERIOD_MS);
        if (len!=1) return -1; return c;
    }
    int read(uint8_t *d, int size) override
    { 
        return uart_read_bytes(UART_NUM_0, (char*)d, size, 1000 / portTICK_PERIOD_MS);
    }
    void flush(char const *s, int size) override { uart_write_bytes(UART_NUM_0, s, size); }
} harmonicSerial;

static void command(char const *s) { harmonicSerial.flush(s, sizeof(s)); }

static int readHex2(char *&s, int l)
{
    int v = 0; int m = 1;
    while (--l>=0)
    {
        if (*s >= '0' && *s <= '9') v = v + (*s++ - '0') * m;
        else if (*s >= 'a' && *s <= 'f') v = v + (*s++ - 'a' + 10) * m;
        else if (*s >= 'A' && *s <= 'F') v = v + (*s++ - 'A' + 10) * m;
        else break;
        m *= 16;
    }
    return v;
}

static void harmonicSerialTask(void*)
{
    harmonicSerial.begin();
    uint8_t d[256]; 
    while (true)
    {
        vTaskDelay(100/portTICK_PERIOD_MS);
        uart_flush_input(UART_NUM_0); // flush input buffer to avoid old data...
        command("!");
        int l= harmonicSerial.read(d, 57); if (l<=57) continue;
        char *s= (char*)d;
        MDecPos= readHex(s, 6); MRAPos= readHex(s, 6); MFocusPos= readHex(s, 6);
        int flags= readHex(s, 2);
        moving= flags&1; sideOfPeir= flags&4; flipping= flags&8; noFlip= !(flags&16); power= flags&64;
        s+= 3*6+2*8; // int time= readHex(s, 6); int midPos= readHex(s, 6); int uncounted= readHex(s, 6); int MRaPos2= readHex(s, 8); int MDecPos2= readHex(s, 8);
        int powercnt= readHex(s, 2);
        sideralSpeed= (powercnt>>5)&3;
    }
}


static void UITask(void*)
{
    display.begin();
    while (true) { vTaskDelay(1); doUI(); } // minimum delay to give other task time to do something
}

extern "C" void app_main()
{
    Time::begin();
    harmonicSerial.begin();
    GPIOSetup();
    xTaskCreate(harmonicSerialTask, "Serial", 2048, NULL, 2, NULL);
    xTaskCreate(UITask, "UI", 4096, NULL, 2, NULL);
}
