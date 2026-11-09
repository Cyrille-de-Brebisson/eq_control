#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

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

#include "system.h"

#include "register\soc\timer_group_struct.h"
#include "driver/gptimer.h"
static gptimer_handle_t ustimer = NULL;
void timeBegin() 
{ 
    gptimer_config_t timer_config; memset(&timer_config, 0, sizeof(timer_config));
    timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT, timer_config.direction = GPTIMER_COUNT_UP, timer_config.resolution_hz = 1000000; // 1MHz, 1 tick=1us
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &ustimer));
    ESP_ERROR_CHECK(gptimer_enable(ustimer));
    ESP_ERROR_CHECK(gptimer_start(ustimer));
}
uint64_t IRAM_ATTR nowus()
{ 
    TIMERG0.hw_timer[0].update.tx_update = 1; while (TIMERG0.hw_timer[0].update.tx_update) { }
    return TIMERG0.hw_timer[0].lo.tx_lo;
}
uint32_t IRAM_ATTR nowms() // THIS IS NOT milliseconds, but 1024th of seconds!
{
    TIMERG0.hw_timer[0].update.tx_update = 1; while (TIMERG0.hw_timer[0].update.tx_update) { }
    return ((uint64_t)TIMERG0.hw_timer[0].hi.tx_hi << 22) | ((TIMERG0.hw_timer[0].lo.tx_lo)>>10);
}

extern "C" void app_main()
{
    timeBegin();
    initSystem();
}
