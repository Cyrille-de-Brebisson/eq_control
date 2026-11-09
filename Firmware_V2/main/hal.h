#include "stdint.h"

// #define NO_UI // define to remove UI

static int const stepperUartPin= 6;
static int const adrRa= 1;
static int const stepPinRa= 5;
static int const adrDec= 2;
static int const stepPinDec= 7;
static int const adrFoc= 0;
static int const stepPinFoc= 8;
static int const LCD_SLC= 20;
static int const LCD_SDA= 21;

#ifdef ESP_PLATFORM
#define atomic_exchange(a,b) __atomic_exchange_n(&a, b, __ATOMIC_ACQ_REL)

#include "driver/gpio.h"
#include "driver/uart.h"
#include "hal/gpio_types.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"
#include "esp_attr.h"
class GPIO { public:
    static void inline output(uint64_t mask)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = mask;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
    }
    static void inline input(uint64_t mask, bool poolup)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = mask;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = poolup?GPIO_PULLUP_ENABLE:GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
    }
    static int inline read(int pin) { return gpio_get_level(gpio_num_t(pin)); }
    #define GPIO_IN_REG_R  (*((volatile uint32_t*)GPIO_IN_REG)) // base + 3c
    #define GPIO_OUT_W1TS_REG_R     (*((volatile uint32_t*)GPIO_OUT_W1TS_REG))
    #define GPIO_OUT_W1TC_REG_R     (*((volatile uint32_t*)GPIO_OUT_W1TC_REG))
    static uint32_t pins;
    static void inline IRAM_ATTR setMask(uint32_t mask, int value) { if (value) GPIO_OUT_W1TS_REG_R= mask, pins|= mask; else GPIO_OUT_W1TC_REG_R= mask, pins&= ~mask; }
    static void inline IRAM_ATTR setMask(uint32_t mask) { GPIO_OUT_W1TS_REG_R= mask; pins|= mask; }
    static void inline IRAM_ATTR clearMask(uint32_t mask) { GPIO_OUT_W1TC_REG_R= mask; pins&= ~mask; }
    static void inline IRAM_ATTR toggleMask(uint32_t mask)
    { 
        pins^= mask;
        if ((pins&mask)!=0) GPIO_OUT_W1TS_REG_R= mask; else GPIO_OUT_W1TC_REG_R= mask;
    }
    static void inline IRAM_ATTR set(int pin, int value) { setMask(1<<pin, value); }
    static void inline IRAM_ATTR set(int pin) { setMask(1<<pin); }
    static void inline IRAM_ATTR clear(int pin) { clearMask(1<<pin); }
};

#include "esp_system.h"
void inline reboot() { esp_restart(); }


#include "driver/i2c_master.h"
class CI2C { public:
    i2c_master_bus_handle_t bus_handle= nullptr;
    i2c_master_dev_handle_t dev[2]{nullptr};
    void begin()
    { 
        if (bus_handle!=nullptr) return;
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = gpio_num_t(LCD_SDA),
            .scl_io_num = gpio_num_t(LCD_SLC),
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = 1 }
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
    }
    void setupDevice(int device, uint16_t adr, uint32_t speed)
    {
        begin();
        if (dev[device]!=nullptr) return;
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = adr,
            .scl_speed_hz = speed
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev[device]));
    }
    void inline send(int device, uint8_t const *d, int nb) 
    { 
        i2c_master_transmit(dev[device], d, nb, 1000/portTICK_PERIOD_MS);
    }
    bool inline read(int device, int len1, uint8_t *out, int len2, uint8_t *in)
    {
        return i2c_master_transmit_receive(dev[device], out, len1, in, len2, 1000/portTICK_PERIOD_MS)==0;
    }
 
};
#endif

void initHW();

#ifdef _MSC_VER
#define IRAM_ATTR
#include <intrin.h>
static int32_t atomic_exchange(int32_t &a, int32_t b) { return _InterlockedExchange((long volatile*)&a, b); }
class GPIO { public:
    static void output(uint64_t mask) { }
    static void input(uint64_t mask, bool poolup) { }
    static uint32_t pins;
    static int inline read(int pin) { return (pins&(1<<pin))!=0; }
    static void inline setMask(uint32_t mask, int value) { if (value) pins|= mask; else pins&= ~mask; }
    static void inline setMask(uint32_t mask) { pins|= mask; }
    static void inline clearMask(uint32_t mask) { pins&= ~mask; }
    static void inline toggleMask(uint32_t mask) { pins^= mask; }
    static void inline set(int pin, int value) { setMask(1<<pin, value); }
    static void inline set(int pin) { setMask(1<<pin); }
    static void inline clear(int pin) { clearMask(1<<pin); }
};
//void xTaskCreate(void(*task)(void*), char const*, int stack, void *param, int, void *);
void vTaskDelay(int ms);
static int const portTICK_PERIOD_MS= 1;
static void reboot() {}
#include <windows.h>
#include <strsafe.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
static inline void trace(const char* format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    StringCchVPrintfA(buffer, 1024, format, args);
    va_end(args);
    OutputDebugStringA(buffer);
}
static void inline xTaskCreate(void (*cb)(void*), char const *, int stack, void *p, int, void*) { CreateThread(0, stack, (LPTHREAD_START_ROUTINE)cb, p, 0, nullptr); }
class CI2C { public: // windows version of I2C (send data to LCD)
    void begin() { }
    void send(int device, uint8_t const *i, int16_t nb)  { Sleep(50); }
    void setupDevice(int, int, int) {}
};
#define STATS
struct TStat {
    char const *title;
    char const *unit;
    std::vector<double> v;
};
extern TStat m2pos, m2dstPos, m2maxSpd, m2requestedSpeed, m2currentSpeed;
extern TStat m1pos, m1dstPos, m1maxSpd, m1requestedSpeed, m1currentSpeed;
extern TStat statincrementalDec, statdecMoveRate, statdesiredDec;
extern TStat statincrementalRa, statRaMoveRate, statdesiredRa;
typedef std::vector<TStat*> CData;
extern CData stats;
extern bool stopStats;
#endif


uint32_t IRAM_ATTR nowms();
uint64_t IRAM_ATTR nowus();
uint32_t Milisecond();
