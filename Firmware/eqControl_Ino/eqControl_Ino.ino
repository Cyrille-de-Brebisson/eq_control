#define TMC // in arduino (__AVR__) nano mode, you can select the TMC or non TMC driver... in esp, it is ALWAYS tmc...
#define HASADC // in __AVR__ mode, IF TMC, this is used to monitor the power supply and handle motor configuration. Some older versions had TMC but no ADC...
               // in ESP mode, the first PCB did not use the ADC. PCB2 uses the ADC for keyboard + power supply
//#define HASGPS // in ESP mode, you can have a GPS module used to get the LST...
//#define WEIRED_KBD // I had some early ARV boards with a slightly differnet keyboard. Uncomment for those

/********************************************
* 
* Equatorial mount controler with keyboard/screen based UI and Ascom drivers
* With optional focusser control
* 
* tracks at sideral speed (can not be turned off)
* Can be controled through the serial port associated with an ASCOM driver
* 
* provides a full UI on the device using a 9 key keyboard and a 32*128 screen (No move to larger screen, this works well)
* This UI allows basic movements (including speed selection and spiral),
* synchronization and goto on over 700 NGC, Messiers, Cadwell, 170 named stars and the planets!
* The UI also allows configuration of the stepper motors.
* Finally, it allows start/stop of sideral movement, enable/disable of meridian flip and changing side of pier
* 
* The same features are available through the Ascom driver.
* The Ascom driver also allows tuning of time drift.
* 

//////////////////////////////////////////////////////////////////////////////
*
*                           User manual
*
* Handheld manual
*
* Connect the RA and Dec motors to the device. Optionally connect the focus. DO NOT connect/disconnect when powered! Can burn the HW
* Connect 9V to 15V to the bottom barrel connector. If not powered then the speed display will be crossed off and no movements are possible
*
* Display indicates current RA and DEC spot. the speed and a telescope that indicate side of pier.
* The screen will turn off after 10s.
*
* RA/DEC keys will move the telescope at the given speed in the appropriate direction. RA movement is limited to 180° (unless otherwize configured)
* The speed key allows to change the speed of movement (from 1 arc minute/s to 4°s)
* Sync/Spiral key, if held, will move the telescope in a spiral fashion to find objects. The size of the spiral will depend on speed (small speed=small spiral)
*
* The menu key circles through the menu. The Esc key returns to the original screen
*
* Messier, NGC, Cadwel an stars menu: Use the RA/DEC keys to select an object (RA moves 1 object at a time, DEC 10 objects at a time)
* press SYNC or GO to sync the scope on this object or go to the object.
* Note: When selecting an object the "telescope" icon will be displayed next to the object name if a move there will result in a meridian flip.
*
* At startup, make sure that the telescope is pointing north (polar align). Then use the keys to move to a known object and 'sync'.
* This will calibrate the scope. Althrough you can move Dec manually, do NOT move RA manually as it is important for the
* system to know where it is in RA to limit the movements to +/-90° around the "vertical".
* Once synced, you can then select an object and "Go" to it!
*
* For planets, you have to enter the current date. Use the RA buttons to cycle through the day, month, year and selected object and Dec buttons
* to select the actual value. Go and Sync will work as for the Messier and other objects...
*
* About meridian flip
* Enable/disable meridian flip by pressing and holding the Esc button, and then pressing the DecUp key, then releasing the Esc key. You will see a small _ under the telescope icon
* If the dec is going the wrong way for you (dec has not flipped), press and hold Esc, press Sync, release Esc. You will see the telesocpe icon switch. The dec direction has been inverted
*
* Focusser
* In the focusser menu, use the RA and Dec buttons as fast and slow movement of the focusser. The position is indicated in mm assuming that you have configured the step size in Ascom driver
*
* On controler stepper conifg menu
* You can do basic configuration of the stepper on the handheld. But this is not advised as the Ascom interface does it much better
* Use the go/sync buttons to select an item to change and then the RA/DEC buttons to change it (RA=slow speed, DEC= fast speed).
* Press Escape to cancel and speed to validate and save the settings...
*
*
* ASCOM
* The true power of the system can only be realised when connected to a PC!
* Install the Ascom system, install the eq3 driver (copy in C:\Program Files (x86)\Common Files\ASCOM and then run it from the command line with the /register parameter)
* Start the .exe select a com port and click on connect.
* Click on setup. You can change all the parameters there! Validate and you should be good to go
* Special note about the "time compensation". Time on the arduino is not very accurate
* So the device will send its time to the PC on a regular basis. After around 1minute the PC will be able to start comparing
* the time provided by the arduino with its refence clock and compute the error. My advise is to wait at least 10 minutes
* to give time to the CPU to get to temperature (yes, it is temperature sensitive) and then press the "auto" button
* next to time comp. This will compute the time compensation. Then press on update HW to send the data to the HW.
* Accurate time is critical for good tracking.








* Handles side of peir, and meridian flip. Althrough the meridial flip is badly implemented and is not working well...
* Meridian flip is nessecary if we do not want the scope to bang on the mount.
* It is possible, because they are 2 ways, in spherical coordinates, to indicate one direction
* This means that if scope is right of pier, the dec motor needs to work in one direction, and if left, in the other direction...
* This also means that the scope needs to know the physical position of the RA axis (or at least the 90° position, weight at the bottom)...
* Then it can know how to handle the dec axis...
* To do this, it is ASSUMED that the scope gets started weight down. This allows the scope to never move the RA axis more than 90 deg
* right or left from this position. This is the physical mark.
* It then applies an offset to know the RA hourly value.
* 
* As earth rotate, the logical coordinates will slowly shift with adjustments made to the ra logical min/max to match real position.
* But the physical position will also be incremented so that the scope knows its physical position too... until the max pos...
* 
* When syncing, only the logical coordinates will be changed to sync the scope.
*
* Dec however is different...
* Dec values are from -90 to 90 on both sides of the rotation. i.e: top is 90 and you can get to -90 going right or left
* (think about being on the north pole and trying to get to the south. You can do it on the greenwitch meridian or the 180deg meridian)
* both will get you to -90 (south)...
* Direction of travel should be one way if ra is past meridian, and the other way if ra is before meridian.
* if ra is at meridian, it does not mater...
* 
* 
* Current state:
*   Understand/fix meridian flip at startup, direction at startup and how to "virtual flip" when moving RA...
*   Need to add new output 12V controler
*********************************************/

#include <stdint.h>
static uint16_t const keyMenu=  1<<0;
static uint16_t const keyGo=    1<<1;
static uint16_t const keyLeft=  1<<2;
static uint16_t const keySpd=   1<<3;
static uint16_t const keyUp=    1<<4;
static uint16_t const keyDown=  1<<5;
static uint16_t const keyEsc=   1<<6;
static uint16_t const keySync=  1<<7;
static uint16_t const keyRight= 1<<8;



#ifndef PC
#define printf(...) // no printf on devices!
#endif

#if defined(__AVR__)
// move I2C to IRQ? (better when screen off, worse when on), so decision is hard...
// move motor move to IRQ? should be better in all cases!

// Arduino Pin Layout
// pins 0-7 are on PORTD
// pins 8-13 are PB0-5. 13 is LED.
// pins 14-19 (A0-5) are PC0-5
// A6/A7 are analog only pins...
// pins 0/1: RX/TX
// pins 2-7: stepper control
// pins 8-10: steper serial pins
// pins 11,12,14-17: kbd
// pin 13: LED
// pins 18 (SDA),19 (SCL): I2c for the screen

// Stepper control pins. Have to be using 2 to 7...
static const int8_t raDirPin    = 2;  // this is port D
static const int8_t decDirPin   = 3; 
static const int8_t focDirPin   = 4; 
static const int8_t raStepPin   = 5; 
static const int8_t decStepPin  = 6;
static const int8_t focStepPin  = 7; 
static const int8_t motorSerialRa = 8; // This is port B
static const int8_t motorSerialDec = 9;
static const int8_t motorSerialFocus = 10;
// LCD control. This is for information only as the I2C hardware is hard wired on them anyway...
static const int8_t displayClk  = 18; // A4
static const int8_t displayIO   = 19; // A5

static int const maxPulsesPerSecond= 9200;
static void inline my_delay_us(uint16_t us)
{ // at 16mhz, we need 16cycles / us
    __asm__ __volatile__ (
        "1: sbiw %0,1" "\n\t" // 2 cycles
        "brne 1b" : "=w" (us) : "0" (us) // 2 cycles
    );
}
static void udelay(uint16_t us) { my_delay_us(us*4); }
__attribute__((always_inline)) inline void DoNotOptimize(const uint8_t &value) { asm volatile("" : "+m"(const_cast<uint8_t &>(value))); }
static void inline portSetup() 
{ 
    DDRD|= 0b11111100; PORTD= 0b00011100; // Set all motor pins to out and the direction pins to 1. pins 0/1 are used by serial and not an issue...
    #ifdef TMC    
        DDRB= 0b111111; PORTB= 0b011111;    // Set B port pins (motor serial + kbd rows0-1 + led) to out and high, except for portB5 which is pin 13=LED
    #else
        DDRB= 0b111111; PORTB= 0b011100;    // Set B port pins (motor enable + kbd rows0-1 + led) to out and high, except for RA and dec enable and portB5 which is pin 13=LED
    #endif
    DDRC= 0b001000; PORTC= 0b001111;    // set 3 kbd pins to pullup, one kbd pin to out and leave I2C alone...
}
static void inline PORTDSET(uint8_t p) { PORTD|=p; }
static void inline PORTDCLEAR(uint8_t p) { PORTD&=~p; }
static void inline PORTBSET(uint8_t p) { PORTB|=p; }
static void inline PORTBCLEAR(uint8_t p) { PORTB&=~p; }
static uint8_t inline PORTBGET() { return PINB; }
static void inline PORTBPULLUP(uint8_t pin) { PORTB|= pin; DDRB&= ~(pin); }
static void inline PORTBOUT(uint8_t pin) { PORTB|= pin; DDRB|= (pin); }
static void portBWritePin(int8_t pin, int8_t v) { PORTB= (PORTB&(~(1<<pin)))|(v<<pin); }
static void portCWritePin(int8_t pin, int8_t v) { PORTC= (PORTC&(~(1<<pin)))|(v<<pin); }
static uint8_t portCRead() { my_delay_us(1); return PINC; }

// Kbd matrix (3*3) // Warning, these are for AVR, pins 14 and above have a +3 numbering on the esp32...
//static uint8_t const kbdr0= 12, kbdr1= 11, kbdr2= 17; // rows are output. high by default to match the pullups
//static uint8_t const kbdc0= 14, kbdc1= 15, kbdc2= 16; // columns are input, pullup
// Sorry, I have made various PCB with slightly different kbd configurations...
// In some case, you will need to change this setting
// D11=PB3 = col 0
// D12=PB4 = col 1
// D17=PC3 = col 2
// D14=PC0 = row top
// D13=PC1 = row mid
// D16=PC2 = row bot
static uint16_t kbdValue()
{
    uint16_t keys;
    #ifndef WEIRED_KBD
        portBWritePin(3, 0); keys= (~portCRead())&0x7; portBWritePin(3, 1); 
        portBWritePin(4, 0); keys|= ((~portCRead())&0x7)<<3;  portBWritePin(4, 1); 
        portCWritePin(3, 0); keys|= uint16_t((~portCRead())&0x7)<<6;  portCWritePin(3, 1); 
    #else
        portBWritePin(3, 0); keys= ((~portCRead())&0x7)<<3; portBWritePin(3, 1); 
        portBWritePin(4, 0); keys|= ((~portCRead())&0x7);  portBWritePin(4, 1); 
        portCWritePin(3, 0); keys|= uint16_t((~portCRead())&0x7)<<6;  portCWritePin(3, 1); 
    #endif
    return keys;
}

#define reboot() // does not work on AVR because old bootloader sucks...
//static void reboot()
//{
  // Watchdog code does not work with old bootloader...
  //cli();  // Disable interrupts
  //WDTCSR = (1 << WDCE) | (1 << WDE); // If you do not do this, then the next write to watchdog will not be taken into concideration by CPU
  //WDTCSR = (1 << WDE) | (1 << WDP3) | (0 << WDP2) | (0 << WDP1) | (0 << WDP0); // set watchdog in reset mode in 4s. This gives time for a full restart and therefore for the clear of this setting after a reboot so that there is no eternal reboot
  //sei();  // Re-enable interrupts
  //while (1); // Wait for watchdog to time out (~15ms)
//}

namespace MSerial {
	// interrupt driven serial class. 700 bytes smaller than the standard Serial class...
	// always 38400 bauds, 64bytes buffers
	static uint8_t const BUFFER_SIZE= 64;
	static volatile char rxBuffer[BUFFER_SIZE];
    static volatile uint8_t rxHead= 0, rxTail= 0;
	static volatile char txBuffer[BUFFER_SIZE];
	static volatile uint8_t txHead= 0, txTail= 0;

	// USART Receive Complete interrupt
	ISR(USART_RX_vect) 
	{
	  char c= UDR0;
	  uint8_t nextHead= (rxHead+1) % BUFFER_SIZE;
	  if (nextHead==rxTail) return;  // Buffer full
	  rxBuffer[rxHead]= c; rxHead= nextHead;
	}
	// USART Data Register Empty interrupt
	ISR(USART_UDRE_vect) 
	{
	  if (txHead==txTail) { UCSR0B&= ~(1<<UDRIE0); return; } // Nothing to send, disable UDRE interrupt
	  UDR0= txBuffer[txTail];
	  txTail= (txTail+1) % BUFFER_SIZE;
	}
    void begin() 
    {
      cli(); // Disable global interrupts
      UBRR0H= 0; UBRR0L= 51; UCSR0A= 2; UCSR0C= 6; //38400 bauds
      UCSR0B= 0b00011000 | (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0); // enable rx, tx and irqs
      sei(); // Enable global interrupts
    }
	int16_t read() 
	{
	  if (rxHead==rxTail) return -1; // No data
	  char c= rxBuffer[rxTail];
	  rxTail= (rxTail+1) % BUFFER_SIZE;
	  return c;
	}
	void print(char c) // add to send buffer and send as soon as possible
	{
	  uint8_t nextHead= (txHead+1) % BUFFER_SIZE;
	  while (nextHead==txTail); // Wait if buffer is full
	  txBuffer[txHead]= c; txHead= nextHead;
	  UCSR0B|= 1<<UDRIE0; 	  // Enable UDRE interrupt
	}
    void print(char const *c) { while (*c!=0) print(*c++); } // Add string in output buffer..
	// on esp32 writes only send on a flush.
	void inline flush(char c) { print(c); }
};

namespace Time {
    int32_t t= 0;
    void begin()
    {
        TCCR1A= 0; // no output on compare match, normal, 0 to ffff count
        TCCR1B= 3; // normal count, clock at 1/64th (2 is 1/8th) see what is best later...
        // 1/64th gives 4micro s precision 1/8th gives 0.5micro s precision...
        TCCR1C= 0; // no output compares...
        OCR1AH= OCR1AL= OCR1BH= OCR1BL= 0; // output compares at 0 as not used
        ICR1H= ICR1L= 0; // same for input captures
        TIMSK1= 0; // no interrupts 
    }
    uint32_t unow() // now in microseconds. But with 4 microsecond precision only...
    {
        // TIFR1 bit 0 is high on overflow of 16 bit timer 4 to 32 times per second depending...
        if ((TIFR1&1)!=0) { TIFR1= 1; t+= 4; } // This happens every 260ms at one 64th divider...so no chances to loose one...
        uint8_t l= TCNT1L;
        uint8_t h= TCNT1H;
        return (uint32_t(t)<<16)|(uint32_t(h)<<10)|(uint16_t(l)<<2);
    }
    uint32_t mnow() // now in milliseconds.
    {
        if ((TIFR1&1)!=0) { TIFR1= 1; t+= 4; } // This happens every 260ms at one 64th divider...so no chances to loose one...
        uint8_t h= TCNT1H;
        return (uint32_t(t)<<6)|h;
    }
};

 #if defined(HASADC) && defined(TMC)
namespace CADC {
    void begin()
    {
        PRR&= ~1; // makes sure ADC is on in power reduction
        ADMUX= 0b01100111;  // ref=vcc, A7
        ADCSRB= 0b00000000; // comparator off, free running mode
        ADCSRA= 0b11100111; // enable, start conversion, auto trigger, no_interrupt, irq disable, 128 clock speed
    }
    uint8_t inline next() { return ADCH; }
};
#else
    #undef HASADC // for safety
#endif

// These are used in esp32 mode...
#define IRAM_ATTR
#define portSET_INTERRUPT_MASK_FROM_ISR() 0
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(a)
#undef HASGPS // just making sure! as there is no GPS in __AVR__ mode..



#elif !defined(PC) // if __AVR__ is not defined, AND PC is not defined... then the only option left is ESP. PC gets defined in the PC source code and __AVR__ in the Arduino system...
#define ESP
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
#define TMC // always defined in ESP mode...


#ifndef  HASADC
// ESP23 C3 pin Layout
// pins 5,6,7: kbd rows top, mid, bottom
// pins 8,9: SLC, SDA
// pins 10,20,21; kbd col left, mid, right
// pin 4: stepper serial
// pins 3, 2, 0: stepper step (ra, dec, foc)
// pin 1: stepper dir (dec)
static const int8_t raDirPin    = -1; 
static const int8_t decDirPin   = 1; 
static const int8_t focDirPin   = -1; 
static const int8_t raStepPin   = 0, raUartAddr= 3; 
static const int8_t decStepPin  = 2, decUartAddr= 1;
static const int8_t focStepPin  = 3, focUartAddr= 2; 
static const int8_t motorSerialRa = 4;
static const int8_t motorSerialDec = 4;
static const int8_t motorSerialFocus = 4;
static const int8_t krt= 5, krm= 6, krb= 7, kcr= 10, kcl= 20, kcm= 21;
static const gpio_num_t LCD_SLC= GPIO_NUM_8;
static const gpio_num_t LCD_SDA= GPIO_NUM_9;
#else // in V2 of the board we use the ADC to handle the keyboard and monitor power supply
static const int8_t raDirPin    = -1; 
static const int8_t decDirPin   = 6; 
static const int8_t focDirPin   = -1; 
static const int8_t raStepPin   = 5, raUartAddr= 3; 
static const int8_t decStepPin  = 7, decUartAddr= 1;
static const int8_t focStepPin  = 8, focUartAddr= 2; 
static const int8_t motorSerialRa = 21;
static const int8_t motorSerialDec = 21;
static const int8_t motorSerialFocus = 21;
static const gpio_num_t LCD_SLC= GPIO_NUM_10;
static const gpio_num_t LCD_SDA= GPIO_NUM_0;
#endif

static int const maxPulsesPerSecond= 10000;


#include "hal/gpio_types.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"

class CGPIO { public:
    static void output(uint64_t mask)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = mask;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
    }
    static void input(uint64_t mask, bool poolup)
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
    static void inline set(int pin, int value) { gpio_set_level(gpio_num_t(pin), value); }
};
static void GPIOSetup()
{
    CGPIO::output((1<<raStepPin)|(1<<decDirPin)|(1<<decStepPin)|(1<<focStepPin)
    #ifndef HASADC
        |(1<<kcl)|(1<<kcm)|(1<<kcr)
    #endif
    );
    CGPIO::set(raStepPin, 0);
    CGPIO::set(decDirPin, 0);
    CGPIO::set(decStepPin, 0);
    CGPIO::set(focStepPin, 0);
    #ifndef HASADC
    CGPIO::set(kcl, 1);
    CGPIO::set(kcm, 1);
    CGPIO::set(kcr, 1);
    CGPIO::input((1<<krt)|(1<<krm)|(1<<krb), true);
    #endif
}
#define PORTDSET(p) CGPIO::set(p, 1)
#define PORTDCLEAR(p) CGPIO::set(p, 0)

#ifdef HASADC
#include "esp_adc/adc_continuous.h"
namespace CADC {
    static int const nbChannels= 4;
    static int const nbSamples= 16;
    static int const sampleSize= nbChannels*nbSamples*4;
    int8_t adcChannelToPinId[7]= {-1, -1, -1, -1, -1, -1, -1};
    uint32_t res[nbChannels]= {4096,4096,4096,0};
    int power= 0; // latest power supply read in deci volts.
    adc_continuous_handle_t handle = NULL;
    static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *) // called around 10 times per second (4*16=64 samples per run at 20Khz) = 30 cycles per run
    {
        // edata is uint8_t *conv_frame_buffer; uint32_t size;
        // return true means recycle dma...
        if (edata->size!=sampleSize) return true;
        uint8_t *p= edata->conv_frame_buffer;
        uint32_t res2[nbChannels]; memset(res2, 0, sizeof(res2));
        uint32_t res2cnt[nbChannels]; memset(res2cnt, 0, sizeof(res2cnt));
        for (int i = nbChannels*nbSamples; --i>=0;) 
        {
            uint8_t b1= *p++; uint8_t b2= *p++; p+= 2;
            int chan= adcChannelToPinId[(b2>>5)&7]; if (chan==-1) continue;
            res2[chan]+= b1|(uint32_t(b2&0xf)<<8);
            res2cnt[chan]++;
        }
        for (int i=nbChannels; --i>=0;) if (res2cnt[i]==nbSamples) res[i]= res2[i]/nbSamples;
        return true;
    }

    void begin() 
    {
        uint8_t const adcPins[nbChannels]={2, 3, 4, 1};
        adc_continuous_handle_cfg_t adc_config = { .max_store_buf_size = sampleSize*4, .conv_frame_size = sampleSize, .flags= 0 };
        ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));
        adc_channel_t channel[nbChannels];
        adc_digi_pattern_config_t adc_pattern[nbChannels];
        for (int i=nbChannels; --i>=0;) 
        {
            adc_unit_t unused; adc_continuous_io_to_channel(adcPins[i], &unused, &channel[i]); adcChannelToPinId[channel[i]]= i; 
            adc_pattern[i].atten = ADC_ATTEN_DB_0;
            adc_pattern[i].channel = channel[i] & 0x7;
            adc_pattern[i].unit = ADC_UNIT_1;
            adc_pattern[i].bit_width = 12;
        }
        adc_continuous_config_t dig_cfg = { .pattern_num= nbChannels, .adc_pattern = adc_pattern, .sample_freq_hz = 20 * 1000, .conv_mode = ADC_CONV_SINGLE_UNIT_1, .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2 };
        ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));
        adc_continuous_evt_cbs_t cbs = { .on_conv_done = s_conv_done_cb, }; ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
        ESP_ERROR_CHECK(adc_continuous_start(handle));
        while (res[3]!=0) vTaskDelay(1); // forces a read to init power!
        power= res[3]/33; // /330 = *5.4/1780
    }
};

static uint16_t lastkbdValue1= 0, lastkbdValue2= 0, lastValid= 0;
static uint16_t kbdValue()
{
    CADC::power= CADC::res[3]/33; // /330 = *5.4/1780
    uint32_t *v= CADC::res; 
    // Adc values are: 2800 for col 1, 2000 for col 2 and 0 for col 3
    uint16_t keys= 0;
    if (v[0]<1000) keys|= keyEsc;
    else if (v[0]<2400) keys|= keySpd;
    else if (v[0]<3500) keys|= keyMenu;
    if (v[1]<1000) keys|= keySync;
    else if (v[1]<2400) keys|= keyUp;
    else if (v[1]<3500) keys|= keyGo;
    if (v[2]<1000) keys|= keyRight;
    else if (v[2]<2400) keys|= keyDown;
    else if (v[2]<3500) keys|= keyLeft;
    if (keys==lastkbdValue1 && keys==lastkbdValue2) return lastValid= keys; // only return a stable value that has been read thrice...
    lastkbdValue1= lastkbdValue2; lastkbdValue2= keys; return lastValid;
}
#else
static int inline kbdc()  { return CGPIO::read(krt) | (CGPIO::read(krm)<<1) | (CGPIO::read(krb)<<2); }
static uint16_t kbdValue()
{
    uint16_t keys;
    CGPIO::set(kcl, 0); keys= kbdc(); CGPIO::set(kcl, 1);
    CGPIO::set(kcm, 0); keys|= kbdc()<<3; CGPIO::set(kcm, 1);
    CGPIO::set(kcr, 0); keys|= kbdc()<<6; CGPIO::set(kcr, 1);
    return keys^0x1ff;
}
#endif

#include "driver/gptimer.h"
#include "freertos/semphr.h"
#include "register\soc\timer_group_struct.h"
namespace Time {
    gptimer_handle_t ustimer = NULL;
    void begin() 
    { 
        gptimer_config_t timer_config; memset(&timer_config, 0, sizeof(timer_config));
        timer_config.clk_src = GPTIMER_CLK_SRC_DEFAULT, timer_config.direction = GPTIMER_COUNT_UP, timer_config.resolution_hz = 1000000; // 1MHz, 1 tick=1us
        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &ustimer));
        ESP_ERROR_CHECK(gptimer_enable(ustimer));
        ESP_ERROR_CHECK(gptimer_start(ustimer));
    }
    uint32_t IRAM_ATTR unow()
    { 
        TIMERG0.hw_timer[0].update.tx_update = 1; while (TIMERG0.hw_timer[0].update.tx_update) { }
        return TIMERG0.hw_timer[0].lo.tx_lo;
    }
    uint32_t IRAM_ATTR mnow()
    {
        TIMERG0.hw_timer[0].update.tx_update = 1; while (TIMERG0.hw_timer[0].update.tx_update) { }
        return ((uint64_t)TIMERG0.hw_timer[0].hi.tx_hi << 22) | ((TIMERG0.hw_timer[0].lo.tx_lo)>>10);
    }
}

#include "driver/usb_serial_jtag.h"
namespace MSerial {
    void begin() { 
        // Configure USB SERIAL JTAG
        usb_serial_jtag_driver_config_t usb_serial_jtag_config = {
            .tx_buffer_size = 1024,
            .rx_buffer_size = 1024,
        };
        ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_jtag_config));
    }
    int16_t read() 
    { 
        char c;
        int len = usb_serial_jtag_read_bytes(&c, 1, 20 / portTICK_PERIOD_MS);
        if (len!=1) return -1; return c;
    }
    int read(uint8_t *d, int size) 
    { 
        return usb_serial_jtag_read_bytes((char*)d, size, 1000 / portTICK_PERIOD_MS);
    }
    char buf[400]; int bufl= 0;
    void inline print(char c) { buf[bufl++]= c; } // Add char in output buffer
    void print(char const *c) // Add string in output buffer..
	{ 
		int l= strlen(c); if (l==0) return; 
		if (l+bufl>sizeof(buf) { usb_serial_jtag_write_bytes(buf, bufl, 20 / portTICK_PERIOD_MS); bufl= 0; }
		memcpy(buf+bufl, c, l); bufl+= l; 
	}
    void flush(char c) { print(c); usb_serial_jtag_write_bytes(buf, bufl, 20 / portTICK_PERIOD_MS); bufl= 0; }
} MSerial;

#include "driver/uart.h"
int gpsGetData(char *b, int size) { return uart_read_bytes(UART_NUM_1, b, size, 10); }
void gpsDone() { vTaskDelete(nullptr); }
bool gpsBegin()
{
    const uart_config_t uart_config = { .baud_rate= 9600, .data_bits= UART_DATA_8_BITS, .parity= UART_PARITY_DISABLE, .stop_bits= UART_STOP_BITS_1, .flow_ctrl= UART_HW_FLOWCTRL_DISABLE };
    uart_driver_install(UART_NUM_1, 1024*2, 1024*2, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, -1, 20, -1, -1);
    return true;
}

#include "alpaca.h"
CAlpaca *alpaca= nullptr;
static uint32_t ipaddr= 0;

#define reboot esp_restart

// This is to deal with __AVR__ functions...
#define PROGMEM
uint8_t pgm_read_byte(uint8_t const *p) { return *p; }
uint16_t pgm_read_word(uint8_t const *p) { return p[0] | (((uint16_t)p[1])<<8); }
uint16_t pgm_read_word(uint16_t const *p) { return *p; }
#define memcpy_P memcpy
static inline void DoNotOptimize(const uint8_t &value) { }
#endif


#include "catalogs.h" // Messier, NGC and star list...

#ifdef HASGPS
namespace CGPS {
    // Typical data comming form the gps at 9600 baud:
    // Info on visible satelites...
    //$BDGSV,1,1,03,14,38,234,28,24,65,289,28,41,,,21,0*7C
    // position: time 16:23:04 UTC, 45°44.97320N 004°50.24199,E 152.8 alt
    //$GNGGA,162304.000,4544.97320,N,00450.24199,E,1,07,1.1,152.8,M,50.2,M,,*46
    // lat, long time. As above
    //$GNGLL,4544.97320,N,00450.24199,E,162304.000,A,A*4D
    // more satelite infos
    //$GNGSA,A,3,12,15,19,25,32,,,,,,,,2.4,1.1,2.1,1*3D
    // more satelite infos
    //$GNGSA,A,3,14,24,,,,,,,,,,,2.4,1.1,2.1,4*32
    // speed/direction information
    //$GNRMC,162304.000,A,4544.97320,N,00450.24199,E,0.00,0.00,141025,,,A,V*03
    // Track & Speed over ground
    //$GNVTG,0.00,T,,M,0.00,N,0.00,K,A*23
    // time and date... time date as 14th oct 2025 with local time offset (00)
    //$GNZDA,162304.000,14,10,2025,00,00*4B
    // 6 satelite info from gps. as in $BDGSV, the 2,1 and 2,2 means 2 lines, line 1 and line 2....
    //$GPGSV,2,1,06,10,,,26,12,69,244,33,15,23,177,27,19,34,059,27,0*5C
    //$GPGSV,2,2,06,25,35,246,32,32,23,316,28,0*6D
    // random text...
    //$GPTXT,01,01,01,ANTENNA OK*35

    // We will extract info from
    //$GNGGA,162304.000,4544.97320,N,00450.24199,E,1,07,1.1,152.8,M,50.2,M,,*46
    //$GNGLL,4544.97320,N,00450.24199,E,162304.000,A,A*4D
    //$GNZDA,162304.000,14,10,2025,00,00*4B

    float latitude, longitude, altitude; // Will be valid once hasPosInfo is true. angles in radians, altitude in meters...
    int h, m, s, D, M, Y; // will be valid once hasTimeInfo is true. Y is 2025
    bool hasPosInfo= false, hasTimeInfo= false, talking= false;
	bool waitGPS= false;                    // true if we are waiting on GPS init
    static int readint(char *&s, int nbchr) // read n chr in an int. return -1 if they was not n numerical chrs.... s points at end of number at the end
    { int res= 0; while (--nbchr>=0) { if (*s<'0' || *s>'9') return -1; res= res*10+*s++-'0'; } return res; }
    static float readAngle(char *&s, int nb) // read an angle in ddmm.frac_part form where frac_part has an undefined length. result in rad. returns 1000 on error. s points at end of number at the end
    {
        int d= readint(s,nb); if (d==-1) return 1000.0f;
        int m= readint(s,2); if (m==-1) return 1000.0f;
        float r= d+m/60.0f;
        if (*s=='.')
        {   s++; float f= 0.1f/60.0f;
            while (true) { int v= readint(s,1); if (v<0) break; r+= v*f; f/=10.0f; }
        }
        return r*M_PI/180.0f;
    }
    static bool skipComa(char *&s, char *end) // find a coma and skip it. return true if they was a coma before end...
    {
        while (*s!=',' && s<end) s++; if (*s!=',') return false; s++; return true;
    }
    // Task that reads incoming data from GPS and populate the data
    static void getInfo(void *) 
    { 
        char b[512]; int sze= 0;
        while (true)
        {
            if (hasPosInfo && hasTimeInfo) gpsDone(); // end of task. frees all that is needed...
            int r= gpsGetData(b+sze, sizeof(b)-sze-1);
            if (r<0) continue;
            sze+= r; b[sze]=0;
            while (true)
            {
                char *l= strchr(b, '$'); if (l==nullptr) break; talking= true;
                char *le= strchr(l, '\n'); if (le==nullptr) break;
                // here, we do have a full line! garantied!
                if (memcmp(l, "$GNGGA", 6)==0) { skipComa(l,le); goto pos; } // skip 2 ',' and then read as in $GNGLL
                if (memcmp(l, "$GNGLL", 6)==0) 
                { pos: 
                    if (!skipComa(l,le)) goto next; // skip 1 , and then read 4544.97320,N,00450.24199,E,1,07,1.1,152.8,M,50.2,M,,*46 where 152 is the altitude...
                    latitude= readAngle(l,2); if (!skipComa(l,le)) goto next; if (*l!='N') latitude= -latitude;
                    if (!skipComa(l,le)) goto next;
                    longitude= readAngle(l,3); if (!skipComa(l,le)) goto next; if (*l=='E') longitude= -longitude;
                    for (int i=0; i<4; i++) if (!skipComa(l,le)) goto next; // skip 4 commas...
                    if (*l<'0' || *l>'9') goto next;
                    altitude= 0; while (*l>='0' && *l<='9') altitude= altitude*10+*l++-'0';
                    hasPosInfo= true; goto next;
                }
                if (memcmp(l, "$GNZDA", 6)==0) 
                { 
                    if (!skipComa(l,le)) goto next; // skip 1 , and then read 162304.000,14,10,2025,00,00*4B (hms, d, m y)
                    h= readint(l,2); if (h==-1) goto next;
                    m= readint(l,2); if (m==-1) goto next;
                    s= readint(l,2); if (s==-1) goto next;
                    if (!skipComa(l,le)) goto next;
                    D= readint(l,2); if (D==-1) goto next;
                    if (!skipComa(l,le)) goto next;
                    M= readint(l,2); if (M==-1) goto next;
                    if (!skipComa(l,le)) goto next;
                    Y= readint(l,4); if (Y==-1) goto next;
                    hasTimeInfo= true; goto next;
                }
                next: memcpy(b, le+1, b+sizeof(b)-le-1); sze-= int(le+1-b);
            }
        }
    }

    // return true if gps system was started...
    bool begin() { if (!gpsBegin()) return false; xTaskCreate(getInfo, "GPS", 2048, NULL, 2, NULL); return true; }

    // Compute Local Sidereal Time (in hours)
        static double normalize24(double hours) { hours = fmod(hours, 24.0); if (hours < 0) hours += 24.0; return hours; }
    double localSiderealTime()
    {
        // Compute Julian Date from UTC date and time
        double utHours= h+m/60.0+s/3600.0;

        int month= M, year= Y;
        if (month<=2) { year -= 1; month += 12; }
        int A = year / 100;
        int B = 2 - A + A / 4;
        double JD = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + D + B - 1524.5;
        JD -= 2451545;
        double GMST_0 = 18.697374558 + 24.06570982441908*JD; // GMST at 0h UT (in hours)
        double GMST = GMST_0 + 1.00273790935*utHours; // GMST at given UT
        GMST = normalize24(GMST);
        double LST = GMST + longitude*(12.0/M_PI); // Convert to local sidereal time
        LST = normalize24(LST);
        return LST;
    }

};
#endif

static uint32_t inline Abs(int32_t v) { return v>=0 ? v: uint32_t(-v); }

//********************************************
// motor pin control.
// knows where it is and can go to position at given speed, handles acceleration/decelerations
// can work in step units and in an arbitrary "user" unit
// can add an extra, always on, uncounted movement to system (sideral)
class Ctmc2209 { public:
    uint8_t const serialPin, dir;
#ifdef TMC
    static uint16_t const tmc2209Delay= 8; // 8micro s = 125Kb/s = 12.5KB/s = 1.6millis for a read/modify/write of a register... Trying 500kb/s did not work...
    // This can be "solved" by keeping a copy of the last sent value of the register so that we do not need to do a read but can just write using the last known copy..
    // Hend dropping from 20 bytes (4 out, 8 in, 8 out) to only 8 bytes out and  .6msec
    uint8_t UARTAdr= 3;
    static uint8_t const GCONFAdr= 0;
    static uint8_t const CHOPCONFAdr= 0x6C;
    static uint8_t const IHOLD_IRUNAdr= 0x10;
    uint8_t GCONF[8], CHOPCONF[8];
    uint8_t const IHold;
    Ctmc2209(uint8_t serial, uint8_t dir, uint8_t IHold): serialPin(1<<(serial-8)), dir(1<<dir), IHold(IHold) { }

    void begin()
    {
        // Read register to make sure motor is OK... But don't care what it is or does...
        #ifndef PC // Thiw will not work on PC!
            uint8_t v[4]; while (readRegister(0, v)!=0) { udelay(10000); UARTAdr= (UARTAdr+1)&3; } // Auto discovert uart addr!
        #endif
        static uint8_t const IGCONF[8]=    { 5, UARTAdr, GCONFAdr|0x80, 0, 0, 1, 0b11000001 };       memcpy(GCONF, IGCONF, 8);       // GCONF.pdn_disable=1, GCONF.mstep_reg_select = 1 (use MRES for step count)
        static uint8_t const ICHOPCONF[8]= { 5, UARTAdr, CHOPCONFAdr|0x80, 0x10, 0x00, 0x00, 0x53 }; memcpy(CHOPCONF, ICHOPCONF, 8); // TOFF=3, microsteps=0(256), INTPOL=1, HSTRT= 5, TBL=0, double edge off (because we can do up/down in 1 function)
        sendPacket(GCONF, 8); sendPacket(CHOPCONF, 8);
        uint8_t IHOLD_IRUN[8]= { 5, UARTAdr, IHOLD_IRUNAdr|0x80, 0, 1, 31, IHold }; sendPacket(IHOLD_IRUN, 8); // set power in hold mode to 1/4 power...
    }
    static uint8_t swuart_calcCRC(uint8_t const * s, uint8_t l)
    {
        uint8_t crc= 0; // CRC located in last byte of message
        do {
            uint8_t currentByte = *s++; // Retrieve a byte to be sent from Array
            for (uint8_t j=0; j<8; j++) 
            {
                if ((crc>>7) ^ (currentByte&0x01)) crc= (crc<<1) ^ 0x07;
                else crc= crc<<1;
                currentByte>>= 1;
            }
        } while (--l!=0);
        return crc;
    }
    // send packet. Will calculate the checksum for you. size/s has to be the full packet lenght (with crc)
    // Assumes that line is high before and output mode
    void sendPacket(uint8_t *s, uint8_t size)
    {
        uint8_t t= PORTD; PORTDCLEAR(dir); 
        s[size-1]= swuart_calcCRC(s, size-1);
        do { // send size bytes
            uint8_t v= *s++;
            noInterrupts(); // Time sensitive
            PORTBCLEAR(serialPin); udelay(tmc2209Delay); // start bit
            for (uint8_t i= 0; i<8; i++) { if ((v&1)==1) PORTBSET(serialPin); else PORTBCLEAR(serialPin); udelay(tmc2209Delay); v>>=1; }
            PORTBSET(serialPin); udelay(tmc2209Delay); // stop bit
            interrupts();
        } while (--size!=0);
        PORTD= t;
    }
    // l is the size WITH the crc and s has to be l bytes long...
    // return >=0 on packet ok. Else -1: no start bit, -2: no end bit, -3: crc error
    // will take 160 microseconds for 8 byte packet
    int8_t readPacket(uint8_t *s, uint8_t l)
    {
        int8_t er;
        noInterrupts(); // Time sensitive.. will be 8*10=80 microseconds!
        uint8_t t= PORTD; PORTDCLEAR(dir); 
        PORTBPULLUP(serialPin);
        for (int8_t j= 0; j<l; j++) // For all packets
        {
            uint16_t t= 10000; while ((PORTBGET()&serialPin)!=0) if (--t==0) { er= -1; goto err; } // wait for start bit
            udelay(tmc2209Delay+tmc2209Delay/2); // wait for 1.5 delay
            uint8_t b= 0; for (int8_t i=0; i<8; i++) { b=b>>1; if ((PORTBGET()&serialPin)!=0) b|= 0x80; udelay(tmc2209Delay); } s[j]= b; // Get 8 bits
            t= 1000; while ((PORTBGET()&serialPin)==0) if (--t==0) { er= -2; goto err; } // Wait for stop bit
        }
        // Back to normal operations
        PORTBOUT(serialPin);
        PORTD= t;
        interrupts();
        if (swuart_calcCRC(s, l-1)==s[l-1]) return 0; // verify crc and return if ok
        er= -3;
    err: PORTD= t; PORTBOUT(serialPin); interrupts(); return er; // return to normal and return error code
    }
    int8_t readRegister(uint8_t r, uint8_t *v)
    {
        uint8_t p[8]= { 5, UARTAdr, r }; sendPacket(p, 4);
        int8_t er= readPacket(p, 8); if (er!=0) return er;
        memcpy(v, p+3, 4); return 0;
    }
    void writeRegister(uint8_t r, uint8_t const *v)
    {
        uint8_t p[7]= { 5, UARTAdr, uint8_t(r|0x80) }; memcpy(p+3, v, 4); sendPacket(p, 8);
    }

    void TOFF(uint8_t val) // update CHOPCONF.TOFF 0:stop stepper, 3:start stepper
    { CHOPCONF[6]= (CHOPCONF[6]&~15)|val; sendPacket(CHOPCONF, 8); }
    // native is 0=256microsteps, 1:128, ..., 7:2, 8:1
    void microsteps(uint8_t val) // update CHOPCONF.MRES
    { CHOPCONF[3]= (CHOPCONF[3]&~15)|val; sendPacket(CHOPCONF, 8); }
    void shaft(uint8_t dir) // update GCONF.shaft 0:direction 1, 1:direction 0
    { GCONF[6]= (GCONF[6]&~8)|(dir<<3); sendPacket(GCONF, 8); }
#else
    Ctmc2209(uint8_t serial, uint8_t dir, uint8_t IHold): dir(1<<dir), serialPin(0) { }
#endif
};


class CMotor : public Ctmc2209 { public:
        uint8_t const stp;                 // pins
        int32_t pos=0, dst=0; int32_t requestedSpd=0;  // Current pos, destination and desired speed (req speed it absolute value).
        uint32_t maxPos=0;                      // maximum positions in steps (min is 0!)...
        uint32_t spdMax=0, accMax=0;            // max speeds and accelerations in steps/s, steps**2/ms (carefull, this 2nd one is in units per mili seconds, not seconds!)
        #ifndef TMC
            static const
        #endif
        int8_t lnPulsesPerPulses= 0;            // one step pulse = 2^0=1 step on motor...
        bool invertDir= false;                  // used for Dec to deal with meridian side...
        int16_t backlash= 0;
        bool lastDirection= false;              // false when positive...

    CMotor(uint8_t dir, uint8_t stp, uint8_t serial, uint8_t IHold): Ctmc2209(serial, dir, IHold), stp(1<<stp) { } // port configurations Handled at global level. This makes it CPU agnostic and saves RAM in AVR

    void init(uint32_t maxPos, uint32_t maxStpPs, uint32_t msToFullSpeed, int32_t minPosReal, int32_t maxPosReal, int32_t initPosReal, bool invert)
    {
        invertDir= invert;
        kill();
        this->maxPos= maxPos;
        spdMax= maxStpPs, accMax= int32_t(maxStpPs)/int32_t(msToFullSpeed);
        this->minPosReal= minPosReal; this->maxPosReal= maxPosReal;
        dst= pos= realToPos(initPosReal);
    }
    #ifdef TMC
    void powerOn() { begin(); lnPulsesPerPulses= 0; }// begin will set to 256 micro steps...
    #endif

    // movement controls in steps...
    bool inline isMoving() { return requestedSpd!=0; }
    void setToSteps(uint32_t position) { if (position>maxPos) dst= pos= maxPos; else dst= pos= position; } // set current position in steps unit
    void inline goToSteps(uint32_t destination) { goToSteps(destination, spdMax); } 
    void goToSteps(uint32_t destination, uint32_t spdStepsPS)
    { 
        if ((int32_t(destination)<pos)!=lastDirection)
        {
          if (lastDirection) destination+= backlash, lastDirection= false; // was going down, now needs to go up, so need to do backlash more steps, so we need to add backlash from destination!
          else { if (int32_t(destination)>=backlash) destination-= backlash; else destination= 0; lastDirection= true; }              // reverse
        }
        if (destination>maxPos) destination= maxPos; dst= destination;
        if (spdStepsPS>spdMax) spdStepsPS= spdMax;
        #ifdef TMC
            uint8_t i=0; while (i<8 && (spdStepsPS>>i)>maxPulsesPerSecond) i++;
            if (i!=lnPulsesPerPulses) microsteps(lnPulsesPerPulses=i); // update speed multiplier...
            pos&= 0xffffff<<lnPulsesPerPulses; dst&= 0xffffff<<lnPulsesPerPulses; // position and destinations have to be rounded to lnPulsesPerPulses
            maxPos&= 0xffffff<<lnPulsesPerPulses;
        #endif
        requestedSpd= spdStepsPS;
    }
    void guide(int32_t steps, uint32_t speed) { if (speed>maxPulsesPerSecond) speed=maxPulsesPerSecond; if (minPosReal<maxPosReal) steps= -steps; goToSteps(pos+steps, speed); }
    void goUp(int32_t spd) { if (spd<0) return goDown(-spd); goToSteps(maxPos, spd); return; }
    void goDown(int32_t spd) { if (spd<0) return goUp(-spd); goToSteps(0, spd); return; }
    void inline stop() { requestedSpd=0; } // controled stop to here... migt be better to calculate end pos to avoid double back...
    void inline kill() { dst= pos; currentSpd= requestedSpd=0; if (!invertDir) PORTDSET(dir); else PORTDCLEAR(dir); } // hard stop!

    // movement controls in user units...
    // internally, the position is kepts in steps...
    // but users might want to have it in some arbirary unit
    // to avoid using floats, we use 2 numbers so that steps*stepsInRealN/stepsInRealD= real value with calcualtions done in 64 bits to avoid issues...
    // At one point I thought about adding 8 bits of precision, but this would not change much/help much.
    // The main advantage woudl be the disapearance of the sub unit counter in the uncounted step system, but they are no other gains...
    int32_t minPosReal, maxPosReal;
    static int32_t muldiv(int32_t v, int32_t n, int32_t d) { return int32_t((int64_t(v)*n)/d); } // returns v*n/d using 64 bits
    int32_t posToReal(int32_t steps) { return muldiv(steps, maxPosReal-minPosReal, maxPos)+minPosReal; } // Assumes input is valid (0-maxPos)
    int32_t realToPos(int32_t real) // make sure input is in range!
    {
        if (minPosReal<maxPosReal)
        {
            if (real<minPosReal) real= minPosReal;
            if (real>maxPosReal) real= maxPosReal;
        } else {
            if (real>minPosReal) real= minPosReal;
            if (real<maxPosReal) real= maxPosReal;
        }
        return muldiv(real-minPosReal, maxPos, maxPosReal-minPosReal);
    }
    int32_t stepsFromReal(int32_t real) // give the number of steps to move by real. This is does not account for the origin shift nor does it test the validity of real
    {
        return Abs(muldiv(real, maxPos, maxPosReal-minPosReal));
    }
    int32_t stepsFromRealNoAbs(int32_t real) // give the number of steps to move by real. This is does not account for the origin shift nor does it test the validity of real
    {
        return muldiv(real, maxPos, maxPosReal-minPosReal);
    }
    int32_t posInReal() { return posToReal(pos); }
    int32_t dstInReal() { return posToReal(dst); }
    void goToReal(int32_t dstReal, int32_t spdRealPS) { goToSteps(realToPos(dstReal), stepsFromReal(spdRealPS)); }
    void goToReal(int32_t dstReal) { goToSteps(realToPos(dstReal)); }
    void goUpReal(uint32_t spdReal) { goUp(stepsFromReal(spdReal)); }
    void goDownReal(uint32_t spdReal) { goDown(stepsFromReal(spdReal)); }
    void goUpRealNoAbs(uint32_t spdReal) { goUp(stepsFromRealNoAbs(spdReal)); }
    void goDownRealNoAbs(uint32_t spdReal) { goDown(stepsFromRealNoAbs(spdReal)); }
    void setToReal(int32_t real) { setToSteps(realToPos(real)); }

    ///////////////////////////////////////////
    // All this is about actually moving!!!
    int32_t currentSpd=0;         // current speed in steps per second. sign indicates direction... can not be 0 if not done moving...
    uint32_t nextMs=0;            // next time we need update speed and recalculate intervals (when now arrives there...(ms timer))
    uint32_t nextStepmus=0;       // next time we need to step
    uint32_t deltaBetweenSteps=0; // delta between 2 steps during this ms slot...

    // issue steps if/as needed...
    void next(uint32_t now)
    {
        if (currentSpd==0) 
        {
            if (requestedSpd==0) return;
            nextStepmus= now; nextMs= now; // we were stopped, but now we are moving. So resync all movements...
        }
        //printf("next pos:%d dst:%d reqSpd: %d spd:%d\r\n", pos, dst, requestedSpd, currentSpd);

        if (int32_t(now-nextMs)>=0)
        {
            nextMs+= 1000;
            // Quantisation on a milisecond.
            // Every ms, update speed (+ or - acc) up to desirned speed...
            // then update mus delta between steps
            int32_t oldSpd= currentSpd;
            if (int32_t(Abs(currentSpd))>requestedSpd) // Slow down if we are going too fast!
            { 
                // Can we slow down to requestedSpd directly? or would that be to jerky?
                if (Abs(requestedSpd-currentSpd)<accMax) 
                { 
                    if (currentSpd<0) currentSpd= -requestedSpd; else currentSpd= requestedSpd; 
                    if (requestedSpd==0) dst= pos; // if speed 0 requested, then we are at destination..
                }
                else { if (currentSpd>0) currentSpd-= accMax; else currentSpd+= accMax; }
            } 
            else {
                int8_t changeSign;
                int32_t stepsLeft= dst-pos;
                if (Abs(stepsLeft)<(1<<lnPulsesPerPulses) && Abs(currentSpd)<=accMax) goto dstReached;   // We have arrived and are going slow enough to stop. do it
                else if (stepsLeft<0 && currentSpd>0) changeSign= -4;           // past or at destination and going the wrong direction. we need to decelerate agressively...
                else if (stepsLeft>0 && currentSpd<0) changeSign= 4;            // same the other way around!
                else {
                    // calculate time, then distance to speed=0: sum(i=0 to n, spd-i*maxAcc)
                    uint32_t msToSpd0= Abs(currentSpd)/accMax; // number of ms until we are slow enough to hard stop...
                    //1/2*accMax*t²...
                    // accMax is in the order of 100 at this point in time...
                    // assuming it takes less than 1s to full speed... t² is smaller than 1million... and t²*accMax smaller than 1billion by a factor 10... so u32 are ok..
                    // But speed is in steps per second and t in ms... so there is a division by 1e3 that will need to happen at one point as accMAx is in /ms unit...
                    uint32_t distSlowDown= (accMax*msToSpd0*msToSpd0)>>11; // /2000;
                    changeSign= (stepsLeft>0)?1:-1;                        // assumes the need to accelerate in the right dirrection
                    if (distSlowDown>=Abs(stepsLeft)) 
                    {
                        changeSign*= -4; // unless we need to slow down!
                    }
                }
                currentSpd+= accMax*changeSign;
                if (currentSpd>requestedSpd) currentSpd= requestedSpd;
                if (currentSpd<-requestedSpd) currentSpd= -requestedSpd;
            }
            if (currentSpd!=oldSpd)
            {
                if (currentSpd==0) currentSpd= accMax>>1;
                if ((currentSpd>=0) ^ invertDir) PORTDSET(dir); else PORTDCLEAR(dir);
                //printHex2(PORTD, 2); MSerial::print(" portD\n");
                uint32_t newDeltaBetweenSteps= 1000000UL / (Abs(currentSpd)>>lnPulsesPerPulses);
                if (newDeltaBetweenSteps<deltaBetweenSteps) nextStepmus-= deltaBetweenSteps-newDeltaBetweenSteps;
                deltaBetweenSteps= newDeltaBetweenSteps;
                //static uint32_t lastPos= 0; printf("deltaBetweenSteps:%d speed:%d(%d) pos:%d delta:%d dst:%d\r\n", deltaBetweenSteps, currentSpd, requestedSpd, pos, pos-lastPos, dst); lastPos= pos;
            }
        }

        if (int32_t(now-nextStepmus)<0) return;    // nothing to do. we wait
        if (Abs(pos-dst)<(1<<lnPulsesPerPulses) && Abs(currentSpd)<=accMax) // destination reached!
        { 
            dstReached: 
            if (pos==dst) { kill(); return; }
            #ifdef TMC        
                lnPulsesPerPulses= 0; microsteps(0);  // can not be here in non microstep mode normally
            #endif
            return; 
        } 
        PORTDSET(stp);            // step pulse up Minimum 1.9micros until down... or 31 cycles...
        DoNotOptimize(stp);
        nextStepmus+= deltaBetweenSteps;           // program allarm
        if (currentSpd>=0) pos+= 1<<lnPulsesPerPulses; else pos-= 1<<lnPulsesPerPulses;      // increase pos
        DoNotOptimize(stp);
        PORTDCLEAR(stp);           // step pulse down. It takes 17 cycles to add 2 32 bit numbers! here I have at least 2 addition, so I should be OK on time!!!
    }
};

static struct { int32_t ra, dec; bool flip= false; } savedGotoForFlip;
static void flip();

class CMotorUncounted : public CMotor { public:
  CMotorUncounted(uint8_t dir, uint8_t stp, uint8_t serialPin, uint8_t IHold): CMotor(dir, stp, serialPin, IHold) { }
    // Tell the system to add some "alway on" rotation which is not counted in the position... a continious drift...
    // Assumes that this is in the positive direction!
        uint32_t NextUncountedSteps= 0;         // Next time we need to step
        uint32_t deltaBetweenUncountedSteps= 0; // delta between 2 steps in micro seconds (1s/1e6)..
        int16_t unstep= 0;                      // number of uncounted steps*256 for precision...
        int16_t unitsPerSteps;                  // number of units per steps*256
        int32_t uncountedMaxRealVal;            // make sure min real is always smaller than this. allign maxReal
        uint32_t countAllUncountedSteps= 0;     // count ALL the uncounted steps to sync with PC
        int16_t _guide= 0;                      // Steps to add or remove in the automatic direction...
        uint8_t sideralMove= 0;                 // just to save the data..
    void killUncountedSpeed() { deltaBetweenUncountedSteps= 0; }
    void setUncountedSpeed(uint32_t seconds, int32_t unitsToMove, uint32_t now, int32_t uncountedMaxRealVal) // MRa.setUncountedSpeed(23*3600UL+56*60+4, 24*3600UL, micros()); // every 23h56m4s do a full turn
    {
        this->uncountedMaxRealVal= uncountedMaxRealVal;
        NextUncountedSteps= now; unstep= 0;
        // every now and then, we need to shift the real min/max to stay in sync...
        // Let us add a *256 to the number just to be on the safe side of things...
        // how many steps do we need for 1 real unit?
        unitsPerSteps= int16_t((int64_t(maxPos)<<8)/Abs(maxPosReal-minPosReal)); // here, by default, this will be around 1230 (or -1230)... meaning that 1230/256 steps is 1 unit
        unitsToMove= muldiv(unitsToMove, maxPos, maxPosReal-minPosReal);     // transform to steps... Assumes no loss of data as we have large number of both...
        if (unitsToMove<0) unitsToMove= -unitsToMove, unitsPerSteps= -unitsPerSteps;
        deltaBetweenUncountedSteps= muldiv(seconds, 1000000L, unitsToMove);  // delta, in micros between uncounted steps...
    }
    int32_t nextGuideStep= 0, guideStepSize= 0;
    uint8_t skipNSteps= 0;
    // TODO: make division performed by PC, not me!!!
    void guide(int16_t steps, uint32_t speed, uint32_t now) { guideStepSize= 1000000UL/speed; if (unitsPerSteps>0) steps= -steps; _guide= steps; nextGuideStep= now; }
    bool lastSpeedWasNegative= false;
    uint16_t slop;
    void next(uint32_t now)
    {
        CMotor::next(now); 
        if (currentSpd<0) lastSpeedWasNegative= true;
        if (currentSpd!=0 || deltaBetweenUncountedSteps==0 || savedGotoForFlip.flip) return; // if moving, or no slow moves... just return...
        if (lastSpeedWasNegative && slop!=0) { 
            printf("detected back move. move up by %d steps\r\n", slop);
            lastSpeedWasNegative= false; guide(slop, spdMax>>3, now); return; } // if stopped moving backward 

        // if we moved for a while, now-NextUncountedSteps will increment and lots of steps will be issued at once after to catch up...
        // what will it do for slow down? no clue! maybe we should back correct and not do it the other way around...
        // to be seen after...

        if (guideStepSize!=0 && int32_t(now-nextGuideStep)>=0) 
        {
            printf("guiding %d left\r\n", _guide);
            nextGuideStep+=guideStepSize; 
            if (_guide==0) guideStepSize= 0; // end of guiding...
            else if (_guide>0) { _guide--; goto step; } // We need to issue an extra step! go for it...
            else { skipNSteps++; _guide++; }            // skip next step
        }

        if (int32_t(now-NextUncountedSteps)<0) return; // no steps to issue now...
        NextUncountedSteps+= deltaBetweenUncountedSteps; if (skipNSteps!=0) { skipNSteps--; return; }

        step:
          countAllUncountedSteps++; 
          #ifdef TMC
            if (lnPulsesPerPulses!=0) microsteps(lnPulsesPerPulses=0); // if speed is not slow... slow down to max!
          #endif
          PORTDSET(stp); DoNotOptimize(stp);                              // step pulse up Minimum 1.9micros until down... or 31 cycles...
          unstep+=256; // 256microsteps per step...
          // correct real min/max. unitsPerSteps sign indicate if the moves are in the same, or in the oposit direction as the real units
          if (unitsPerSteps>0)
          {   // in telescopes, we always are going the other way, so this is never used...
              // so if memory needs to be gained, this could be commented out...
              if (pos<=0) { flip(); return; }
              pos--;
              while (unstep>=unitsPerSteps) 
              { 
                  unstep-= unitsPerSteps; minPosReal--, maxPosReal--; 
                  if (minPosReal<0 || maxPosReal<0) minPosReal+= uncountedMaxRealVal, maxPosReal+= uncountedMaxRealVal; // Actually needs to check on 
              }  
          } else if (unitsPerSteps<0)
          {
              pos++;
              if (pos>=maxPos) { flip(); return; }
              while (unstep>=-unitsPerSteps)
              { 
                  unstep-= -unitsPerSteps; minPosReal++, maxPosReal++; 
                  if (min(maxPosReal, minPosReal)>=uncountedMaxRealVal) minPosReal-= uncountedMaxRealVal, maxPosReal-= uncountedMaxRealVal;
              }
          }
          DoNotOptimize(stp); PORTDCLEAR(stp);                              // step pulse down. It takes 4 cycles for an out, 12 cycles to add/substract 1. comparisons of 2 16 bit numbers is 6 cycles
    }
};

// The 3 motors!
static CMotorUncounted MRa(raDirPin,   raStepPin, motorSerialRa, 31);  // full power on hold (anyhow, never stops)
static CMotor MDec(decDirPin, decStepPin, motorSerialDec, 16);      // 1/2 power on hold
static CMotor MFocus(focDirPin, focStepPin, motorSerialFocus, 7);   // 1/4 power on hold (assuming not off when not in use)

#ifdef TMC
    static int8_t MDecIsOn= 0;
    static void MFocusOn() { if (MDecIsOn==2) return; MDecIsOn=2; MDec.TOFF(0); MFocus.TOFF(3); }
    static void MDecOn() { if (MDecIsOn==1) return; MDecIsOn=1; MFocus.TOFF(0); MDec.TOFF(3); }
#else // This can only happen in __AVR__ mode so no need to #def around __AVR__
    static void MFocusOn() { PORTB = (PORTB&0b11111000)|0b010; }  // Focus&ra on, dec off...
    static void MDecOn() { PORTB = (PORTB&0b11111000)|0b100; }  // Focus off, ra&dec on...
#endif

//********************************************
// I square C for display and display after this..
// This is a non irq driven code (why? Because I don't know how to do IRQ on arduino!)
#ifdef __AVR__
class CI2C { public:
    // I2C is <400Khz (300k should work). at ~4000bits per frame, we could do 100fps!!! so no wories!
    // SCLf= CpuClock/(16+2*TWBR*Prescaler)
    static void begin() { TWBR= 33; TWCR= 68; TWSR= 0; } // around 200khz, 20kbytes/s max speed, prescaler at 1 by default = 50fps...
                                                         // Except that we can usually do only 10K loop per second, so I²C should rarely be buzy...
    int16_t nb= 0; uint8_t *d= nullptr;
    void send(uint8_t *i, int16_t nb) 
    { 
        if (d!=nullptr) return;
        d= i; this->nb= nb;
        TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);     // Send START condition
    }
    bool next() // return true if buzy
    {
        if (d==nullptr) goto retfalse;          // nothing to do...
        if (!(TWCR & (1<<TWINT))) goto rettrue; // HW buzy sending someting can not do anything... Most of the time it will not jump...
        if (nb==0) goto eot; // end of transmission?
        nb--;
        TWDR = *d; *d++= 0; // Load data into TWDR register and then erase it...
        TWCR = (1<<TWINT) | (1<<TWEN); // Clear TWINT bit in TWCR to start transmission of address
        rettrue: return true;
        retfalse: return false;
        eot: // end of transmission
        d= nullptr; TWCR = (1<<TWINT)|(1<<TWEN)| (1<<TWSTO);    // Transmit STOP condition
        return true; // still buzy sending stop... on next 'next' it will be free...
    }
} I2C;
#elif defined (ESP)
#include "driver/i2c_master.h"
class CI2C { public:
    i2c_master_bus_handle_t bus_handle; i2c_master_dev_handle_t dev_handle; bool hasBeenInitialize= false;
    void begin()
    { 
        if (hasBeenInitialize) return; hasBeenInitialize= true;
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = LCD_SDA,
            .scl_io_num = LCD_SLC,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = { .enable_internal_pullup = 1 }
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x3C, 
            .scl_speed_hz = 200000, // 200khz...
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
    }
    void send(uint8_t *d, int nb) 
    { 
        // Assumes that d starts with chip address because that is how it used to work on arduino. So we skip it here...
        d++, nb--;
        i2c_master_transmit(dev_handle, d, nb, 1000 / portTICK_PERIOD_MS);
        memset(d, 0, nb);
    }
    bool next() { return false; }
} I2C;
#endif

//********************************************
// Framebuffer based display This has the framebuffer which is sent through I2C...
// This class has all the drawing functions... and uses the I2C above...
class CDisplay { public: 
    uint8_t screenisOn;
    uint8_t preCommand[6]; // stores the start of what has to be send to the HW to display the screen... just before the fb so we can just point there and send all...
    uint8_t fb[128/8*32]; // 512 bytes of RAM for the frame buffer. by packs of 8 lines wih 1 byte = 8 vertical pixels!!!! crapy formatting!!!!
    void clear() { memset(fb, 0, sizeof(fb)); } 

      static uint8_t const lcdInit[31]; // the list of commands to send to initialize the LCD...
      bool sendCommand= false;
    void begin() 
    {
        memcpy_P(preCommand, lcdInit, sizeof(lcdInit));
        I2C.begin(); I2C.send(preCommand, sizeof(lcdInit)); // init LCD
        while (I2C.next()); // wait for init sent..
        screenOn(); dopreCommand();
    }
    void dopreCommand()
    {
        preCommand[0] = 0x3C<<1; preCommand[1]= 0; preCommand[2]= screenisOn; preCommand[3]= 0xb0; preCommand[4]= 0x3C<<1; preCommand[5]= 0x40;    
    }
    void reset()
    {
        while (I2C.next()); // complete current operation...
        begin(); // reset/restart the process...
    }
    void screenOn() { screenisOn= 0xAF; }
    void screenOff() { screenisOn= 0xAE; }
    bool inline isScreenOff() { return screenisOn==0xAE; }
    void disp() // send the whole screen and wait until sent to continue... Not used in this project
    { 
        dopreCommand();
        I2C.send(preCommand, 4); while (I2C.next());
        I2C.send(preCommand+4, 512+2); while (I2C.next());
    }
    bool next() // each time you call this, it will send the next byte to the screen, allowing more "pooling" type sending...
    // return true if se are starting a new screen! Should be at 50hz in our case... This can "drive" the UI rate...
    { /// it should be interupt driven, but I do not know how to do it in arduino :-)
        if (I2C.next()) return false;
        sendCommand= !sendCommand;
        if (sendCommand) { dopreCommand(); I2C.send(preCommand, 4); return true; }
        I2C.send(preCommand+4, 512+2);
        return false;
    }
    // Framebuffer manipulations
    // The organization in the framebuffer is non-trivial and non-intuitive
    bool pixel(int8_t x, int8_t y) { return (fb[x+(y>>3)*128] & (1<<(y&7)))!=0; }
    void pixon(int8_t x, int8_t y) { fb[x+(y>>3)*128]|= 1<<(y&7); }
    void pixoff(int8_t x, int8_t y) { fb[x+(y>>3)*128]&= ~(1<<(y&7)); }
    void hline(int8_t x, uint8_t w, int8_t y, bool on=true) // Horizontal line
    { 
        uint8_t m= 1<<(y&7);
        uint8_t *d= fb+x+(y>>3)*128;
        if (on) while (w!=0) { w--; *d++|= m; } else { m= ~m; while (w!=0) { w--; *d++&=m; }  }
    }
    void rect(int8_t x, int8_t y, uint8_t w, int8_t h, bool on=true) { while (--h>=0) hline(x, w, y++, on); }
    void vline(int8_t x, int8_t y, int8_t h, bool on=true) // could be speed on... // Vertical liune
    { if (on) while (h!=0) { h--; pixon(x, y++); } else while (h!=0) { h--; pixoff(x, y++); } }

      static uint8_t const font8[];
    uint8_t dspChar(char s, uint8_t x, uint8_t y, uint8_t eraseLastCol= 1) // disp 1 8*6 pixel character
    {
        uint8_t const *f= font8+(s-32)*8;
        uint8_t h= 8; if (y>24) h= 24-y;
        for (int i=0; i<h; i++) 
        { 
            uint8_t m= 1<<((y+i)&7);
            uint8_t *p= fb+x+((y+i)>>3)*128;
            uint8_t v= pgm_read_byte(f+i);
            if ((v&1)!=0)  *p++|=m; else *p++&=~m; 
            if ((v&2)!=0)  *p++|=m; else *p++&=~m; 
            if ((v&4)!=0)  *p++|=m; else *p++&=~m; 
            if ((v&8)!=0)  *p++|=m; else *p++&=~m; 
            if ((v&16)!=0) *p++|=m; else *p++&=~m; 
            if (eraseLastCol!=3) { if ((v&32)!=0) *p++|=m; else *p++&=~m; }
        }
        return x+6;
    }
    uint8_t dspChar2(char s, uint8_t x, uint8_t y, uint8_t eraseLastCol= 1) // displ 1 16*12 pixels character
    {
        uint8_t const *f= font8+(s-32)*8;
        uint8_t h= 16; if (y>16) h= 32-y;
        for (int8_t i=0; i<h; i++) 
        { 
            uint8_t m= 1<<((y+i)&7);
            uint8_t *p= fb+x+((y+i)>>3)*128;
            uint8_t v= pgm_read_byte(f+(i>>1));
            if ((v&1)!=0) { *p++|=m; *p++|=m; } else { *p++&=~m; *p++&=~m; }
            if ((v&2)!=0) { *p++|=m; *p++|=m; } else { *p++&=~m; *p++&=~m; }
            if ((v&4)!=0) { *p++|=m; *p++|=m; } else { *p++&=~m; *p++&=~m; }
            if ((v&8)!=0) { *p++|=m; *p++|=m; } else { *p++&=~m; *p++&=~m; }
            if ((v&16)!=0) { *p++|=m; *p++|=m; } else { *p++&=~m; *p++&=~m; }
            if (eraseLastCol!=3) { if ((v&32)!=0) { *p++|=m; *p++|=m; } else { *p++&=~m; *p++&=~m; } }
        }
        return x+12;
    }
    uint8_t text(char const *s, int8_t x, int8_t y, uint8_t eraseLastCol= 1, int8_t nb=127) // display in 8*6 pixel font. if eraseLastCol=0 then the last character only displays 5 columns
    { 
        while (*s!=0 && --nb>=0)
        {
            char c= *s++;
            if (eraseLastCol==0 && *s==0) eraseLastCol= 3;
            x= dspChar(c, x, y, eraseLastCol);
        }
        return x;
    }
    uint8_t text2(char const *s, int8_t x, int8_t y, uint8_t eraseLastCol= 1, int8_t nb=127)  // display in 16*10 pixel font. if eraseLastCol=0 then the last character only displays 10 columns
    { 
        while (*s!=0 && --nb>=0)
        {
            char c= *s++;
            if (eraseLastCol==0 && (nb==0 || *s==0)) eraseLastCol= 3;
            x= dspChar2(c, x, y, eraseLastCol);
        }
        return x;
    }
    // b is in PROGMEM and is width, height and then the bitmap bits, vertically, with the next column starting as soon as the previous is done (no empty space)
    void blit(uint8_t const *b, uint8_t x, uint8_t y)
    {
        int8_t w= pgm_read_byte(b++); int8_t h= pgm_read_byte(b++); uint8_t B=0; int8_t bs= 1;
        uint8_t *P= fb+x+(y>>3)*128; y= 1<<(y&7);
        while (--w>=0)
        {
            uint8_t *p= P; uint8_t v= *p; uint8_t m= y; int8_t r= h;
            while (true)
            {
                if (--bs==0) { bs= 8; B= pgm_read_byte(b++); }
                if ((B&1)==0) v&= ~m; else v|= m;
                B>>=1; 
                if (--r==0) break;
                m<<=1; if (m==0) { *p= v; p+=128; m= 1; v= *p; }
            }
            *p= v;
            P++;
        }

    }
} display;

        uint8_t const CDisplay::font8[] PROGMEM ={ // 6*8 pixel font with only chr 32 to 127
            /* */ 0,0,0,0,0,0,0,0, /*!*/ 4,4,4,4,4,0,4,0, /*"*/ 10,10,10,0,0,0,0,0, /*#*/ 10,10,31,10,31,10,10,0, /*$*/ 4,30,5,14,20,15,4,0, /*%*/ 3,19,8,4,2,25,24,0, /*&*/ 2,5,5,2,21,9,22,0, /*'*/ 4,4,4,0,0,0,0,0, /*(*/ 8,4,2,2,2,4,8,0, /*)*/ 2,4,8,8,8,4,2,0, /***/ 0,10,4,31,4,10,0,0, /*+*/ 0,4,4,31,4,4,0,0, /*,*/ 0,0,0,0,6,6,4,2, /*-*/ 0,0,0,31,0,0,0,0, /*.*/ 0,0,0,0,0,6,6,0, /*/*/ 0,16,8,4,2,1,0,0,
            /*0*/ 14,17,25,21,19,17,14,0, /*1*/ 4,6,4,4,4,4,14,0, /*2*/ 14,17,16,12,2,1,31,0, /*3*/ 14,17,16,14,16,17,14,0, /*4*/ 8,12,10,9,31,8,8,0, /*5*/ 31,1,15,16,16,17,14,0, /*6*/ 12,2,1,15,17,17,14,0, /*7*/ 31,16,8,4,2,2,2,0, /*8*/ 14,17,17,14,17,17,14,0, /*9*/ 14,17,17,30,16,8,6,0, /*:*/ 0,6,6,0,6,6,0,0, /*;*/ 0,6,6,0,6,6,4,2, /*<*/ 8,4,2,1,2,4,8,0, /*=*/ 0,0,31,0,31,0,0,0, /*>*/ 1,2,4,8,4,2,1,0, /*?*/ 14,17,16,8,4,0,4,0,
            /*@*/ 14,17,21,29,5,1,30,0, /*A*/ 14,17,17,31,17,17,17,0, /*B*/ 15,17,17,15,17,17,15,0, /*C*/ 14,17,1,1,1,17,14,0, /*D*/ 7,9,17,17,17,9,7,0, /*E*/ 31,1,1,15,1,1,31,0, /*F*/ 31,1,1,15,1,1,1,0, /*G*/ 14,17,1,1,25,17,30,0, /*H*/ 17,17,17,31,17,17,17,0, /*I*/ 14,4,4,4,4,4,14,0, /*J*/ 16,16,16,16,17,17,14,0, /*K*/ 17,9,5,3,5,9,17,0, /*L*/ 1,1,1,1,1,1,31,0, /*M*/ 17,27,21,21,17,17,17,0, /*N*/ 17,17,19,21,25,17,17,0, /*O*/ 14,17,17,17,17,17,14,0,
            /*P*/ 15,17,17,15,1,1,1,0, /*Q*/ 14,17,17,17,21,9,22,0, /*R*/ 15,17,17,15,5,9,17,0, /*S*/ 14,17,1,14,16,17,14,0, /*T*/ 31,4,4,4,4,4,4,0, /*U*/ 17,17,17,17,17,17,14,0, /*V*/ 17,17,17,10,10,4,4,0, /*W*/ 17,17,17,21,21,27,17,0, /*X*/ 17,17,10,4,10,17,17,0, /*Y*/ 17,17,10,4,4,4,4,0, /*Z*/ 31,16,8,4,2,1,31,0, /*[*/ 14,2,2,2,2,2,14,0, /*\*/ 0,1,2,4,8,16,0,0, /*]*/ 14,8,8,8,8,8,14,0, /*^*/ 4,10,17,0,0,0,0,0, /*_*/ 0,0,0,0,0,0,31,0,
            /*`*/ 2,2,4,0,0,0,0,0, /*a*/ 0,0,14,16,30,17,30,0, /*b*/ 1,1,15,17,17,17,15,0, /*c*/ 0,0,30,1,1,1,30,0, /*d*/ 16,16,30,17,17,17,30,0, /*e*/ 0,0,14,17,31,1,14,0, /*f*/ 4,10,2,7,2,2,2,0, /*g*/ 0,0,14,17,17,30,16,14, /*h*/ 1,1,15,17,17,17,17,0, /*i*/ 4,0,6,4,4,4,14,0, /*j*/ 8,0,12,8,8,8,9,6, /*k*/ 1,1,9,5,3,5,9,0, /*l*/ 6,4,4,4,4,4,14,0, /*m*/ 0,0,11,21,21,21,17,0, /*n*/ 0,0,15,17,17,17,17,0, /*o*/ 0,0,14,17,17,17,14,0,
            /*p*/ 0,0,15,17,17,15,1,1, /*q*/ 0,0,30,17,17,30,16,16, /*r*/ 0,0,29,3,1,1,1,0, /*s*/ 0,0,30,1,14,16,15,0, /*t*/ 2,2,7,2,2,10,4,0, /*u*/ 0,0,17,17,17,17,30,0, /*v*/ 0,0,17,17,17,10,4,0, /*w*/ 0,0,17,17,21,21,10,0, /*x*/ 0,0,17,10,4,10,17,0, /*y*/ 0,0,17,17,17,30,16,14, /*z*/ 0,0,31,8,4,2,31,0, /*{*/ 12,2,2,1,2,2,12,0, /*|*/ 4,4,4,4,4,4,4,0, /*}*/ 6,8,8,16,8,8,6,0, /*~*/ 0,0,2,21,8,0,0,0, /*¦*/ 7,5,7,0,0,0,0,0,
        };
        // LCD init sequence
        uint8_t const CDisplay::lcdInit[31] PROGMEM = { 0x3C<<1, 0,
            0xAE, 0xD5,0x80 , 0xA8,0x1f, // SSD1306_DISPLAYOFF, SSD1306_SETDISPLAYCLOCKDIV, 0x80 (ratio), SSD1306_SETMULTIPLEX, 0x1F (HEIGHT - 1)
            0xD3,0x0, 0x40, 0x8D,0x14, // SSD1306_SETDISPLAYOFFSET, 0x0, SSD1306_SETSTARTLINE | 0x0, SSD1306_CHARGEPUMP, 0x14,
            0x20,0x00, 0xA0, 0xC0, // SSD1306_MEMORYMODE, 0x00, SSD1306_SEGREMAP | 0x1, SSD1306_COMSCANDEC; // memory mode 0x0 act like ks0108
            // w*h=128*32: comPins = 0x02
            // w*h=128*64: comPins = 0x12;
            // w*h=96*12: comPins = 0x2;
            // else: comPins = 0x02;
            0xDA, 0x02, // SSD1306_SETCOMPINS comPins
            0x81, 0x00, // SSD1306_SETCONTRAST contrast
            0xD9, 0xf1, // SSD1306_SETPRECHARGE 0xF1h
            0xDB,0x40, 0xA4, 0xA6, 0x2E, 0xAF, // SSD1306_SETVCOMDETECT, 0x40, SSD1306_DISPLAYALLON_RESUME, SSD1306_NORMALDISPLAY, SSD1306_DEACTIVATE_SCROLL, SSD1306_DISPLAYON // Main screen turn on
            0x21,0x0,0x7F, // Set columns 0 to 127
            //0xb0 // set we are at top of screen
        };




///////////////////////////////////////////////////////////////////////
// EEprom for setting saving to eeprom...
  static uint8_t toStr(char *s, uint32_t v);
#ifdef __AVR__
  static void EEPROM_update(uint8_t *d, int8_t size)
  {
      for (uint8_t i=0; i<size; i++)
      {
          EEAR = i; EEDR = *d++; /* Set up address and Data Registers */
          EECR = 1<<EEMPE; /* Write logical one to EEMPE */
          EECR = 1<<EEPE; /* Start eeprom write by setting EEPE */
          while (EECR & (1<<EEPE)); /* Wait for completion of write */
      }
  }
  static void EEPROM_read(uint8_t *d, int8_t size)
  {
      for (uint8_t i=0; i<size; i++)
      {
          EEAR = i; /* Set up address register */
          EECR = (1<<EERE); /* Start eeprom read by writing EERE */
          *d++= EEDR; /* Return data from Data Register */
      }
  }
#elif defined(ESP)
  static void EEPROM_update(uint8_t *d, int size) { alpaca->save("CdBEqControl", d, size); }
  static void EEPROM_read(uint8_t *d, int size) { if (!alpaca->load("CdBEqControl", d, size)) memset(d, 0xff, size); } // load data and set to FF if error
#else // PC case...
  static uint32_t readHex(char *&s, int8_t cnt); // read an hex value from a string...
  static char EEPROM[]= "00906500E3200100C8000000AAA25400BDF00000C800000035110000F02319009C61020010049001420022001F0030752003C8000000F000191D01000000000000000000000000000000000026";
  static void EEPROM_update(uint8_t *d, int8_t size)  { }
  static void EEPROM_read(uint8_t *d, int8_t size) { 
      char *s= EEPROM;
      while (--size>=0) { *d++= readHex(s, 2); s+= 2; }
  }
#endif


static uint32_t const sideralSpeeds[5]= {0, 23*3600UL+56*60+4, 24*3600UL+50*60, 24*3600UL, 1299188UL}; // King
#pragma pack(1)
class CSavedData { public:
    static CSavedData savedData;
    struct { 
      uint32_t maxPos, maxSpd, msToSpd; 
      void dsp(char const *t, uint8_t x, uint8_t under) // Used by setup code to display the motor settings...
      {
          uint8_t x2= display.text(t, x, 0)+6;
          char n[9];
          x2= display.text("nb:", x, 8);   if (under==0) display.hline(x, x2-x, 8+7); toStr(n, maxPos); display.text(n, x2, 8);
          x2= display.text("spd:", x, 16); if (under==1) display.hline(x, x2-x, 16+7); toStr(n, maxSpd); display.text(n, x2, 16);
          x2= display.text("acc:", x, 24); if (under==2) display.hline(x, x2-x, 24+7); toStr(n, msToSpd); display.text(n, x2, 24);
      }
    } ra, dec;
    int32_t timeComp; // drift adjustment between internal clock and real time...
    int32_t Latitude, Longitude; 
    uint16_t Altitude, FocalLength, Diameter_mm, Area_cm2; 
    uint16_t FocStepdum, focMaxStp, focMaxSpd, focAcc; // FocStepdum is in thenth of microns..
    uint16_t decBacklash, raAmplitude; // raAmplitude is the amplitude of the RA movement in degree! allows to pass the meridian... decBacklash is in steps...
    uint8_t guideRateRA, guideRateDec; // used by driver only... in steps/s
    uint8_t invertAxes; // 1 bit per motor... For some reason dec, ra and then focus... dec is before RA... don't ask...
    uint8_t guidingBits; // 0:ra pier invert, 1:ra invert, 2:ra stop, 3:dec pier invert, 4:dec invert, 5:dec stop (these are server side stuff)
                         // 6: AP mode (true if access point mode, esp32 only)
    uint16_t raBacklash, focBacklash;
    uint8_t raSettle; // server side. will cause pulse guide after ra negative to avoid slack...
    uint8_t extra[11]; // for future...
    uint8_t crc;
    uint8_t calcCrc()
    {
        uint8_t sum= 0; uint8_t *d= (uint8_t*)this; for (int8_t i=sizeof(*this)-1; --i>=0;) sum+= d[i];
        return sum;
    }
    #ifdef TMC
    	static uint32_t const microSteps= 256;
    #else
	    static uint32_t const microSteps= 32;
    #endif
    void init() 
    { // default values for EQ3-2
      memset(this, 0, sizeof(*this));
      ra={130UL*200*microSteps, 200*microSteps*4, 200 }; 
      dec={65UL*200*microSteps*20/12, 200*microSteps*4, 200}; 
      timeComp= 0; Latitude=45*3600ULL, Longitude=4*3600, Altitude=1040, FocalLength=400, Diameter_mm=66, Area_cm2= 34, FocStepdum= 31; 
      focMaxStp=65532; focMaxSpd=200*4; focAcc= 200;
      decBacklash= 0;
      raAmplitude= 195; // plus 30mn on each side...
      guideRateRA= (130UL*200*microSteps)*7/(360*3600ULL);
      guideRateDec= (65UL*200*microSteps*20/12)*7/(360*3600ULL);
      save();
    } 
    bool testCrc() { return crc==calcCrc(); }
    void load() // load from eprom, verify checksum and if bad, re-init
    {
        EEPROM_read((uint8_t*)this, sizeof(*this));
        if (testCrc()) initMotors();
        else init();
    }
    void save()
    { 
        crc= calcCrc(); EEPROM_update((uint8_t*)this, sizeof(*this));
        initMotors();
    }
    static void initUncountedStep2(int32_t dsideral) // enable/disable sideral movement with time compensation...
    {
      MRa.sideralMove= 0; while (MRa.sideralMove<5 && dsideral!=sideralSpeeds[MRa.sideralMove]) MRa.sideralMove++;
      if (dsideral!=0)
      {
        // when HW time is > PCTime (i.e: HW ticks faster), then timeComp will be negative...
        // In this case, dsideral needs to be a little bit bigger to cause the delta between steps to be longer to compensate...
        // dsideral is 23h56'4" . Lunar day is 24h50 dsolar is 24h
        // int32_t dsideral= 23*3600UL+56*60+4; // This used to be here to handle it. But now that we can change uncounted steps, it has been moved elsewhere...
        dsideral-=(int64_t(savedData.timeComp)*dsideral)>>24;
        MRa.setUncountedSpeed(dsideral, 24*3600L, Time::unow(), 24*3600L); // every 23h56m4s do a full turn
      } else MRa.killUncountedSpeed();
    }
    static void initMotors() // init the 3 motors based on saved data..
    {
        // the /2 are because we only use 1/2 of the range of the motor!
        MDec.init(savedData.dec.maxPos>>1, savedData.dec.maxSpd, savedData.dec.msToSpd, -90*3600L, 90*3600L, 90*3600L, (CSavedData::savedData.invertAxes&1)!=0);
        int32_t amp= 6*3600L/180*savedData.raAmplitude;
        MRa.init(int64_t(savedData.ra.maxPos)*savedData.raAmplitude/360,  savedData.ra.maxSpd,  savedData.ra.msToSpd, 6*3600L+amp, 6*3600L-amp, 6*3600L, (CSavedData::savedData.invertAxes&2)!=0);
        initUncountedStep2(23*3600UL+56*60+4);
        MFocus.init(uint32_t(savedData.focMaxStp)<<8, uint32_t(savedData.focMaxSpd)<<8,  savedData.focAcc, -int32_t(savedData.focMaxStp)*savedData.FocStepdum/20, int32_t(savedData.focMaxStp)*savedData.FocStepdum/20, 0, (CSavedData::savedData.invertAxes&4)!=0);
        MFocus.backlash= savedData.focBacklash<<8;
        MRa.backlash= savedData.raBacklash;
        MRa.slop= savedData.raSettle;
        MDec.backlash= savedData.decBacklash;
    }
} CSavedData::savedData;















#if 1
#define hasPlanets
namespace Planets {
//////////////////////////////////////////////
// Planetary stuff. Look for planetPos function at the bottom
typedef int32_t Cartesian; // Coordinates in AU in 1+7+24 format (1 bit sign, 7 bit IP, 24 bit FP) for a max value of 127AU which is OK here...
Cartesian Cmul(Cartesian v1, Cartesian v2) { return Cartesian((int64_t(v1)*v2)>>24); }
Cartesian Cmul(Cartesian v1, Cartesian v2, Cartesian v3) { return Cartesian((((int64_t(v1)*v2)>>24)*v3)>>24); }
Cartesian Cdiv(Cartesian v1, Cartesian v2) { return Cartesian((int64_t(v1)<<24)/v2); }

Cartesian sqrt(Cartesian a, Cartesian b, bool sign=false) // return sqrt a*a+/-b*b (substract b if sign = true)
{
    int64_t v= int64_t(b)*b; // do the multiplication in 64 bit mode...
    if (sign) v= -v;
    v+= int64_t(a)*a;
    if (v<=0) return 0;
    // Here, v is a^2+/-b^2 with 48 bit precision...
    uint32_t t= uint32_t(v>>48); uint8_t bits= 0; while (t!=0) { bits++; t>>=1; } // count the number of bits of ip to generate the initial value close to the sqrt(v)
    int64_t x= v>>((bits>>1)+24); // remove the extra 24 bits of precision from the mul and shift by bits/2 bits as first guess on sqrt..
    for (int8_t i=10; --i>=0; ) x= (x+v/x)>>1; // 10 iterations of newton for sqrt
    return int32_t(x);
}

typedef uint16_t Angle; // [0, 65536[ correspond to [0,360deg[
// sinTable[i]=65536*sin(i in 256th of circle. One full circle is 256)
static uint16_t const sinTable2[65] PROGMEM ={0, 1608, 3215, 4821, 6423, 8022, 9616, 11204, 12785, 14359, 15923, 17479, 19024, 20557, 22078, 23586, 25079, 26557, 28020, 29465, 30893, 32302, 33692, 35061, 36409, 37736, 39039, 40319, 41575, 42806, 44011, 45189, 46340, 47464, 48558, 49624, 50660, 51665, 52639, 53581, 54491, 55368, 56212, 57022, 57797, 58538, 59243, 59913, 60547, 61144, 61705, 62228, 62714, 63162, 63571, 63943, 64276, 64571, 64826, 65043, 65220, 65358, 65457, 65516, 65535};
static uint16_t sinTable(uint8_t v) { return pgm_read_word(sinTable2+v); }
// sin (a+b) = sin(a)*cos(b) + cos(a)*sin(b)
// if b is small cos(b)=1 and  sin(b)=b
// sin (a+b) = sin(a) + b*cos(a)
// here we choose b to be between 0 and 2pi/256 or 0 and 0.024rad
// we use a table for sin/cos for values for 0 to 2pi with 8 bit accuracy...
// The max error is 0.00032 with an average error of 0.000074!
static Cartesian sin(Angle a) // a: [0, 65536[ correspond to [0,360deg[. Returns sin(a)*(1<<24)
{
    int32_t s, c;
    uint8_t a2= a>>8; // separte a into a (high part) and a2 (low part)
    // find sin/cos of a2 in table (with 16 bit precision)
    if (a2<0x40) s= sinTable(a2), c= sinTable(64-a2);
    else if (a2<0x80) s= sinTable(128-a2), c= -int32_t(sinTable(a2-64));
    else if (a2<0xC0) s= -int32_t(sinTable(a2-128)), c= -int32_t(sinTable(192-a2));
    else s= -int32_t(sinTable(uint8_t(0-a2))), c= sinTable(a2-192);
    int32_t b= (a&0xff)*25; // b has to be in radiant. a is in 1/2^16 part of 0..2pi interval.
                            // convert a&0xff to a "real number", by multiply by 2pi and divide by 2^16..
                            // Here we will take 18 bit precision. a having 16 bits, and 2*pi*4=25 adding an extra 2 bits
    b= (b*c)>>10;           // multiply by cos with 16 bits for 34 bit precision (with room to spare as our numbers are <<1)... and remove 10 bits to get to our final 24 bits
    return (s<<8)+b;        // return sin(a) with 24 bit precsion + b*cos(a) with also the right precsion..
}
Cartesian cos(Angle v) { return sin(v+0x4000); }

Angle atan2(Cartesian y, Cartesian x)
{ 
    if (x==0) // x=0, can be +/-90 depending on y
    {
        if (y==0) return 0; // error!!!
        if (y>0) return 0x4000;
        return 0xC000;
    }
    // We first divide the circle in 8 zones (45 deg areas) where y/x can be reduced in the [0..1] interval. Then we calculate atan([0..1]) using a 4th order taylor polinomial and then correct for zone...
    // The max error is 0.002514 rad with an average of 0.000364
    // changes  table changes encodes the 8 zone possibilities with low order bit indicating an inversion and high order bits a number to add at the end
    uint8_t const changes[8]= { 0x00, 0x41, 0x81, 0x40, 0x01, 0xC0, 0x80, 0xC1 };
    uint8_t addAtEnd= changes[((y>>29)&0b100)+((x>>30)&0b10)+(Abs(x)>Abs(y)?0:1)]; // Get the right number in the table...
    // now calculate abs(y/x) or abs(x/y) so that v=y/x is in [0..1]
    Cartesian v; if (Abs(x)<Abs(y)) v= Cdiv(x,y); else v= Cdiv(y,x); if (v<0) v=-v;
    // approximate arctan(x) by a 4th order polynomial centered at x=0.5 (taylor series)...
    // 0.463647609001+0.8*(x-0.5)-0.32*(x-0.5)^2-4.26666666667e-2*(x-0.5)^3+0.1536*(x-0.5)^4
    // But the coefitians here are for a result in radians.. and we want a result in our angle unit, so multiply by 0x8000/pi=10430
    // so the coefs becomes: 4836, 8344, -3338, -445, 1602
    // We will use a ((ax)*x+b)*x... type polynomial evaluation to optimize...
    // so, when it comes to the coefs for x3, and lower, we can use  a version shifted by 24 bits for more precision...
    // X3:-7466355650, X2:-55997667375, X:139994168438, X0: 81134951838
    int64_t X= v-(1L<<23); // X=v-0.5
    int64_t r= ((1602*X-7466355650LL)*X)>>24;
    r= ((r-55997667375LL)*X)>>24;
    r= ((r+139994168438LL)*X)>>24;
    r= r+81134951838LL;
    if (r<0) r=-r;
    uint16_t r2= uint16_t(r>>24);
    if ((addAtEnd&1)!=0) r2= -r2;
    r2+= uint16_t(addAtEnd&0xf0)<<8;
    return r2;
}

// Table of orbital elements
//  N = longitude of the ascending node
//  i = inclination to the ecliptic (plane of the Earth's orbit)
//  w = argument of perihelion
//  a = semi-major axis, or mean distance from Sun
//  e = eccentricity (0=circle, 0-1=ellipse, 1=parabola)
//  M = mean anomaly (0 at perihelion; increases uniformly with time)
struct Toe { uint16_t N; uint16_t N2; uint16_t i; uint16_t w; int16_t w2; uint32_t a; uint32_t e; uint16_t M; int32_t M2; };
Toe const OE[8] PROGMEM ={
    //N      N2   i     w      w2   a           e         M      M2
    { 0,     0,   0,    51507, 561, 16777216L,  280330L,  64816, 11758669L }, // sun. or the earth with negative coordinates...
    { 8798,  387, 1275, 5301,  121, 6494427L,   3449982L, 30702, 48823452L }, // mercury
    { 13959, 294, 617,  9992,  165, 12135464L,  113632L,  8739,  19114158L }, // venus
    { 9021,  251, 336,  52156, 349, 25563242L,  1567075L, 3386,  6251811L },  // mars
    { 18287, 330, 237,  49857, 196, 87284472L,  813661L,  3621,  991246L },   // jupiter
    { 20691, 285, 453,  61784, 355, 160302112L, 931907L,  57702, 399005L },   // saturne
    { 13471, 166, 140,  17596, 364, 321815680L, 793864L,  25957, 139894L },   // uranus
    { 23989, 359, 322,  49670, -71, 504293920L, 144384L,  47376, 71524L },    // neptune
};

static void objPos(int32_t d, int8_t obj, Cartesian &x, Cartesian &y, Cartesian &z)
{
  Toe oe[1]; memcpy_P(&oe, &OE[obj], sizeof(Toe)); obj= 0;
    // get an object position in Sun centered, ecliptic centered coordinates (ecept for the earth where it is the reverse)
    Angle N= oe[obj].N+((oe[obj].N2*d)>>16), w= oe[obj].w+((oe[obj].w2*d)>>16), M= oe[obj].M+((oe[obj].M2*d)>>16);
    Angle E= M+Angle( (Cmul( oe[obj].e, sin(M), (1L<<24)+Cmul(oe[obj].e, cos(M)))*10430LL)>>24); // excentric anomaly E from the mean anomaly M and from the eccentricity e:
    if (obj!=0)  // not needed for the sun...
      for (int8_t i=5; --i>=0;) 
        E= E - Angle( ((int64_t(E-M)<<24) - Cmul(oe[obj].e, sin(E))*10430LL) / ((1L<<24) - Cmul(oe[obj].e, cos(E)) ) ); // not used for sun...
    Cartesian xv= Cmul(oe[obj].a, cos(E)-oe[obj].e);
    Cartesian yv= Cmul(oe[obj].a, sqrt(1L<<24, oe[obj].e, true), sin(E));
    Angle v= atan2(yv, xv);
    Cartesian r= sqrt(xv,yv);
    Angle lon = v + w; //compute the longitude. i and N are actually 0 for sun... but we are size optimizing here...
    Cartesian clon= cos(lon), slon= sin(lon);
    Cartesian sloncosi= Cmul(slon, cos(oe[obj].i));
    x = Cmul(r, (int64_t(cos(N))*clon - int64_t(sin(N))*sloncosi )>>24);
    y = Cmul(r, (int64_t(sin(N))*clon + int64_t(cos(N))*sloncosi )>>24);
    z = Cmul(r, slon, sin(oe[obj].i)); 
}

static char const planetNames[8][8] PROGMEM = { "Soleil", "Mercure", "Venus", "Mars", "Jupiter", "Saturne", "Uranus", "Neptune" };

int8_t year=23, month=1, day=1; // Parameters for planetPos
void planetPos(int p, int32_t &ra, int32_t &dec) // return planet position. 0 is sun, 1, 2 are mercury, venus, 3-7 are Mars to Neptune
{
    int32_t d= 367*year - (7*((year+2000L)+((month+9)/12)))/4 + (275*month)/9 + day - (730530L-367*2000L);
    Cartesian sex, sey, ox, oy, oz, t;
    objPos(d, 0, sex, sey, t);  // earth position (sun is -earth)
    objPos(d, p, ox, oy, oz);   // object position
    ox= ox+sex, oy= oy+sey;    // object position compared with earth... sunz=0...
    // Need to rotate around earth center by ecliptic!
    int64_t const sinecl= 6673596LL, cosecl= 15392794LL; // sin and cos of ecliptic
    t= (oy*cosecl - oz*sinecl)>>24; oz= (oy*sinecl + oz*cosecl)>>24; oy= t;    // ecliptic rotation...
    int32_t raf= atan2(oy, ox), decf= atan2(oz, sqrt(ox,oy));  // cartesian to spherical transform...
    ra= (raf*5400)>>12; dec= (decf*5062)>>8; // to my fixed point system
    if (dec>180*3600L) dec= dec-360*3600L; // modulo...
}
};
#endif

//********************************************
// UI part of thing

// graphics that get dispalyed...
static uint8_t const star[] PROGMEM = { 17, 17, 0, 1, 0, 2, 144, 36, 65, 62, 1, 255, 129, 254, 11, 254, 15, 252, 31, 255, 255, 241, 127, 224, 255, 160, 255, 2, 255, 1, 249, 4, 73, 18, 128, 0, 0, 1, 0}; // picture of a star
static uint8_t const scopeE[] PROGMEM = { 5, 10, 16, 35, 195, 131, 48, 1, 3};
static uint8_t const scopeW[] PROGMEM = { 5, 10, 1, 11, 195, 3, 50, 16, 3};
static uint8_t const XImage[] PROGMEM = { 5, 10, 3, 51, 3, 3, 51, 3, 3};

// state variables...
enum { UIMain, UIMessier, UINgc, UIStars, UIPlanets, UICadwell, UIFocus, UIWifi};
static uint8_t UI= 0; // UI will be one of the above and indicate the UI screen...
static uint8_t UIConfig=0;                      // in config screen: indicates the selected parameter
static int8_t findMessier= 0, findCadwell=0;    // last select messiers et cadwell
static uint8_t findPlanet= 4, findPlanetUI= 0;  // last selected planet, and what is the current UI entry in this screen
static uint8_t planetRecalc=0; int32_t planetra, planetdec; // cache for ra/dec calculation for planet. if recalc is 0, then it needs to be recalculated
static uint8_t countToNextSelect; int8_t delayToNextSelect= 15; // to accelerate when press&hold keys in messier/ngc selection
static uint8_t returnToScreen1= 0;             // down counter to go back to screen 0 UI (ticks every screen refresh)
static int16_t lastkbd= 0;                     // status of last keyboard scann for debounce
static bool stopMovingOnKeyRelease= false;     // if true, then once no key is pressed, stop moving... records who said move/don't move
static uint32_t spiralI= 0; static int32_t spiralDD=0, spiralDR=0; // used to handle the spiral key...
static bool blockSync= false;                  // if true, do not execute sync key (used to avoid exit from star menu with sync leads to sync going on...)
static const uint32_t UIDelay= 50000;           // Pool time of keyboard. UI unit of time. 20 times per second...
static const uint16_t timeToScreenOffConst= 10*20;   // 10s timout on the screen. The 20 is the UI delay unit of time...
static uint16_t timeToScreenOff= timeToScreenOffConst; // countdown to screen off...
static const int8_t nbSpeeds= 6;
static uint8_t manualSpeed= 3;                 // current speed in manualSpeeds
static int16_t const manualSpeeds[]= { 60, 60*15, 60*30, 60*60, 2*60*60, 4*60*60 }; // list of possible speed in manual mode in deg per second. WARNING, RA is in h, not deg units!
static char const manualSpeedsTxt[]=" 1'15'30' 1\x7f 2\x7f 4\x7f"; // text for the above

// utility functions. Can not trust % as it is undefined for negatives. plus, 99% of the time, we will have only 1 "loop"... so faster than /
static int32_t round24(int32_t r)
{
    while (r>=24*3600L) r-= 24*3600L;
    while (r<0) r+= 24*3600L;
    return r;
}
// RA motor pos in seconds, rounded to 24 (MRa pos will be from h to h+12 and if h>12, then h+12 will be larger than 24h!)
static int32_t MRaposInReal() { return round24(MRa.posInReal()); }

static bool isRaFlipEnabled()
{ // noFlip
    return Abs(MRa.minPosReal-MRa.maxPosReal)<=22*3600L;
}

// return true if no need to do a flip...
static bool sameSideOfMeridian(int32_t targetRa)
{
    if (!isRaFlipEnabled()) return true;
    // RA.pos are always growing... but inverted! so if maxPos is > 12, then minPos is >24!
    // so if this is the case, then add 24 to target to get in range...
    if (targetRa<MRa.maxPosReal) targetRa+= 24*3600L;
    return targetRa>=MRa.maxPosReal && targetRa<MRa.minPosReal;
}

static void sync(int32_t ra, int32_t dec) // Set scope position to coordinates..
{
    // The dificulty lies in the fact that this might require a "virtual" meridian flip and that the
    // motors min/max position, as well as pos, might need to be adjusted...
    if (MDec.isMoving() || MRa.isMoving()) return; // If moving, then refuse syncing!
    if (ra<0 || ra>24*3600L || dec<-90*3600L || dec>90*3600L) return; // error detection.. had issue with bad serial commands (missed inbound byte)
    // Ra: steps 0-max is a 12h span with realStart>realEnd (unit inverted), current pos is new ra, calculate real min/max by calculating the ratio of pos in max and transforming this to units... and then removing this from ra... get max by removing 12h
    // Here, we do not change the physical position of the motor as it drifts compare with the ra coordinate... There is an assumption that it was at 6h at start of the mount....
    int32_t d= 12*3600L/180*CSavedData::savedData.raAmplitude;
    if (!isRaFlipEnabled()) d= 24*3600L;
    MRa.maxPosReal= round24(ra-MRa.muldiv(MRa.maxPos-MRa.pos, d, MRa.maxPos)); MRa.minPosReal= MRa.maxPosReal+d;
    // Dec is easier...
    // Here, we assume that the physical motor position was badly setup at startup, hence the shift between what we think is dec and the real one...
    // Except that there is the side of peir issue. Are we going the right way around? If done through the pad, then the user should have flipped as needed..
    // But if done through the PC, then it's an unknown...
    MDec.setToReal(dec);
}

static void flipRa()
{
    if (!isRaFlipEnabled()) return;
    MRa.minPosReal+= 12*3600L; MRa.maxPosReal+= 12*3600L;
    if (MRa.maxPosReal>=24*3600L) MRa.minPosReal-= 24*3600L, MRa.maxPosReal-= 24*3600L; // let us remember that maxPos is always minPos-12h!
}
static void flipDec()
{
    MDec.kill(); MDec.invertDir= !MDec.invertDir;
}

void flip() // This only works if amplitude > 180° and scope is close to edge! Not tested at this point in time... Assumes user is not dumb!
{
    stopMovingOnKeyRelease= false;
    savedGotoForFlip.ra= MRaposInReal(); savedGotoForFlip.dec= MDec.posInReal();
    savedGotoForFlip.flip= true;
    MRa.goToSteps(MRa.maxPos/2); MDec.goToSteps(MDec.maxPos);
    return;
}

static void goTo(int32_t ra, int32_t dec)
{
    if (savedGotoForFlip.flip) return; // no goto while doing a flip...
    if (ra<0 || ra>24*3600L || dec<-90*3600L || dec>90*3600L) return; // error detection.. had issue with bad serial commands (missed inbound byte)
    // The telescope can only access stars when the counterweight are bellow the center of gravity.
    // This, in essence, limits the RA coordinates to 12h instead of 24h...
    // To access the other 12h of RA, we need to realize that polar coordinages Ra/Dec point to the same place as coordinaes Ra+12h/180-Dec
    // because (doing polar to cartesian conversions, here using degres for both axes)
    // X=cos(RA+180)*cos(180-Dec)=cos(RA)*cos(Dec)
    // Y=sin(Ra+180)*cos(180-Dec)=sin(RA)*cos(Dec)
    // Z=Sin(180-Dec)=Sin(Dec)
    // So, if we detect that we can not access the requested Ra coordinates, we do the following:
    // 1: go pointing north
    // 2: shift the real coordinates for the Ra axis by 180° and invert the dec direction
    // 3: and go where we need to....
    // this is done by saving the target, moving the motors "by hand", and when at north continuing the process
    MDecOn();
    stopMovingOnKeyRelease= false;
    if (!sameSideOfMeridian(ra))
    { // go to true north and enable flip... Can not be here if flip not enabled!
        // MSerial.print("gf"); printHex2(ra,6); printHex2(dec,6); printHex2(MRa.maxPosReal,6); printHex(MRa.minPosReal,6); MSerial.flush(); // debug stuff...
        savedGotoForFlip.ra= ra; savedGotoForFlip.dec= dec; savedGotoForFlip.flip= true;
        MRa.goToSteps(MRa.maxPos/2); MDec.goToSteps(MDec.maxPos);
        return;
    }
    // now, ra should be in the range... But, because MRa min/max does not wrap around, we need to udpate ra if/as needed. And remember than maxPos is smaller than minPos!
    if (ra<MRa.maxPosReal) ra+= 24*3600L;
    MRa.goToReal(ra), MDec.goToReal(dec); // no need to meridian flip, jut go to!
}

static void goTo2(int32_t ra, int32_t dec, int16_t time) // move in direction at speed
{
    if (savedGotoForFlip.flip) return; // no goto while doing a flip...
    MDecOn();
    stopMovingOnKeyRelease= false;
    if (!sameSideOfMeridian(ra)) // meridian flip?
    { 
        savedGotoForFlip.ra= ra; savedGotoForFlip.dec= dec; savedGotoForFlip.flip= true;
        MRa.goToSteps(MRa.maxPos/2); MDec.goToSteps(MDec.maxPos);
        return;
    }
    // now, ra should be in the range... But, because MRa min/max does not wrap around, we need to udpate ra if/as needed. And remember than maxPos is smaller than minPos!
    if (ra<MRa.maxPosReal) ra+= 24*3600L;
    int32_t pra= MRa.realToPos(ra), pdec= MDec.realToPos(dec);
    int32_t dstra= pra-MRa.pos, dstdec= pdec-MDec.pos;
    if (time!=1000) dstra= dstra*1000/time, dstdec= dstdec*1000/time;
    MRa.goUp(dstra), MDec.goUp(dstdec); // no need to meridian flip, jut go to!
}

static void enableFlip(bool en)
{
    if (isRaFlipEnabled()==en) return;
    int32_t mid= (MRa.maxPosReal+MRa.minPosReal)/2;
    if (en)
    { // enable meridian flip... 
        int32_t amp= 6*3600L/180*CSavedData::savedData.raAmplitude; MRa.maxPosReal= mid-amp; MRa.minPosReal= mid+amp;
        int32_t t= int64_t(CSavedData::savedData.ra.maxPos)*CSavedData::savedData.raAmplitude/360;
        amp= MRa.maxPos-t; MRa.maxPos= t;
        MRa.pos-= amp/2;
        if (MRa.pos<0) MRa.pos= 0; if (MRa.pos>int32_t(MRa.maxPos-1)) MRa.pos= MRa.maxPos-1;
    } else { // disable meridian flip. maxr/minr are mid+/-12 max from saved data, pos+= (newMax-currentMax)/2
        MRa.maxPosReal= mid-12*3600LL; MRa.minPosReal= mid+12*3600LL;
        MRa.pos+= (CSavedData::savedData.ra.maxPos-MRa.maxPos)/2;
        MRa.maxPos= CSavedData::savedData.ra.maxPos;
    } 
}

static uint8_t toStr(char *s, uint32_t v) // convert number to dec representation... Returns number of characters...
{
    uint8_t nb= 0; while (true) { s[nb++]= '0'+(v%10); v/= 10; if (v==0) break; } s[nb]=0; // Write number, but backward...
    for (int8_t i=nb/2; --i>=0;) { char r= s[i]; s[i]= s[nb-1-i]; s[nb-1-i]= r; } // flip it!
    return nb;
}
// convert number [0..99] into number representation with leading 0.
static void toStr2(char *s, uint8_t v) { s[1]= '0'+(v%10); s[0]= '0'+(v/10); }

    uint8_t const column[] PROGMEM = { 6, 14, 60, 15, 207, 195, 243, 240, 60, 0, 0, 0, 0}; // Fat 2 dots....
    uint8_t const degree[] PROGMEM = { 6, 6, 0b11111111, 0b00111111, 0b11001111, 0b11111111, 0b1111};

static void dispRaDec(int32_t ra, int32_t dec) // dispaly ra/dec on screen
{ 
    char radecbuf[3];
    const uint8_t y= 18; uint8_t t, x;
    // disp ra
    ra= round24(ra)/60; t= ra%60; ra/=60;
    toStr2(radecbuf, ra); x= display.text2(radecbuf, 0, y, 1, 2); // display h
    display.blit(column, x, y); // display ':'
    toStr2(radecbuf, t); display.text2(radecbuf, x+6, y, 1, 2); // display minutes
    // Disp DEC
    dec/=60;
    if (dec<-90*60) dec+= 360*60; // dec can not be smaller than -90. In this case add 360°.
    if (dec<0) radecbuf[0]= '-', dec= -dec; else radecbuf[0]= ' ';
    t= dec%60; dec/=60;
    toStr2(radecbuf+1, dec); x= display.text2(radecbuf, 60, y,1, 3);
    display.blit(degree, x, y);
    toStr2(radecbuf, t); display.text2(radecbuf, x+8, y, 0, 2);
}

static bool scopeWest()
{
    return MDec.invertDir != ((CSavedData::savedData.invertAxes&1)!=0); // west if different from initial!
}

// if go, or sync pressed, then execute as needed... Saves memory to put it there...
static void testGoSync(uint16_t newKeyDown, int32_t ra, int32_t dec)
{
    if ((newKeyDown&keyGo)!=0)  // validation button
    { 
        goTo(ra, dec);
        UI= UIMain;
    }
    if ((newKeyDown&keySync)!=0) 
    {
       sync(ra, dec);   // Sync telescope on object..
       UI= UIMain;
       blockSync= true;
    }

    dispRaDec(ra, dec); 
    bool s= sameSideOfMeridian(ra);
    if (!s) display.blit(scopeWest() ? scopeE:scopeW, 122, 0);
    else display.rect(122, 0, 5, 10, false);
}

static bool lastpower= false; // true if power is on!



// Main UI function!
static void doUI()
{
    char t[22]; // Text buffer...
    if (!display.next()) return; // update UI upon screen send complete

    //////////////////////////////////////////////
    // in 128*32 OLED mode UI is:
    //   can display 10*2 characters 12*16 chr (+8 spaces)... or 21*4 6*8 chrs
    // menu key change screen (with a 5s timeout), and esc key returns to screen 1
    // Screen 1: keys: dir (move), spd (change move speed), spiral (move in spiral), Esc (stop all movements)
    //          Press and hold esc, then press Sync to change the dec direction. press and hold esc, then press right to enable/disable sideral move
    //     Ra      dec
    //     hh:mm-dd:mm
    // Screens Messier, NGC, Stars, Cadwell: keys: dir (for slow/fast change), go (goto object), sync (sync to object)
    //     Messier ???/NGC/Star
    //     Ra:hh:mm Dec:-dd:mm
    // planets: right/left to select item, up/down to change, go (goto object), sync (sync to object)
    //   dd:mm:yy planet
    //   ra/dec
    // Focus: keys: dir to move the focus
    //     Focus ?????
    // Last Screen: setup go/sync to select item, directions to change value. Esc to cancel, Menu to validate...

    uint16_t keys= kbdValue(); uint16_t newKeyDown= keys&~lastkbd; lastkbd= keys;

	#ifdef HASGPS
	    if (CGPS::waitGPS)
	    {
	        if (!CGPS::talking) display.text2("INIT GPS", 16, 8); else  display.text2("WAIT GPS", 16, 8);
	        if ((newKeyDown&keyEsc)!=0) CGPS::waitGPS= false; // esc key. stop where we are...
	        return;
	    }
	#endif

    if (keys!=0) display.screenOn(), returnToScreen1= 20*10, timeToScreenOff= timeToScreenOffConst;   // if a key is pressed, we reset the returnToScreen1 timer...
    if (returnToScreen1==0) UI= UIMain; else returnToScreen1--; // at 0, reset UI. else tick down timer
	#ifndef PC // no screen auto off in PC mode...
	    if (timeToScreenOff==0) { display.screenOff(); return; }
	    timeToScreenOff--;
	#endif

    // display.clear(); // No need as screen is erased as we go...

    if (savedGotoForFlip.flip) // Meridian swapping!
    {
        display.text2("Flip", 32, 0);
        dispRaDec(MRaposInReal(), MDec.posInReal());
        if ((newKeyDown&keyEsc)!=0) { savedGotoForFlip.flip= false; } // esc key. stop where we are...
        return;
    }

    if (UI==UIMain) // Main screen. move, speed, menu and spiral
    {
        display.text2("Ra", 0, 0); display.text2("Dec", 94, 0, 0); dispRaDec(MRaposInReal(), MDec.posInReal());
        uint8_t x= display.text("spd:   /s", (94+24-9*6-1-7)/2, 0); display.text(manualSpeedsTxt+manualSpeed*3,(94+24-9*6-1-7)/2+24,0, 1,3);
        #ifdef HASADC // only in this case do we know if there is power...
            if (!lastpower) display.hline((94+24-9*6-1-7)/2, x-(94+24-9*6-1-7)/2, 4);
        #endif
        display.blit(scopeWest() ? scopeW:scopeE, x+1, 0);
        if (!isRaFlipEnabled()) display.hline(x+2, 3, 5);
        static char const speeds[6][8]= {"Stopped", "Sideral", "Moon", "Sun", "King", "Unknow"};
        display.text(speeds[MRa.sideralMove], 30, 8);


        if ((newKeyDown&keyLeft)!=0) { stopMovingOnKeyRelease= true; MRa.goDownReal(manualSpeeds[manualSpeed]/15); }
        if ((newKeyDown&keyRight)!=0) { stopMovingOnKeyRelease= true; MRa.goUpReal(manualSpeeds[manualSpeed]/15); }
        if ((newKeyDown&keyUp)!=0) { stopMovingOnKeyRelease= true; MDecOn(); MDec.goUpRealNoAbs(manualSpeeds[manualSpeed]); }
        if ((newKeyDown&keyDown)!=0) { stopMovingOnKeyRelease= true; MDecOn(); MDec.goDownRealNoAbs(manualSpeeds[manualSpeed]); }
        if ((keys&keyUp)!=0 && MDec.pos==MDec.maxPos)
        {
            flipRa(); flipDec();
            MDec.pos--;  // avoids, on next loop, a reflip the other way around!
            MDec.goDownRealNoAbs(manualSpeeds[manualSpeed]);
        }
        if ((newKeyDown&keySpd)!=0) manualSpeed= (manualSpeed+1)%nbSpeeds;
      
        if ((keys&keyEsc)!=0 && (newKeyDown&keySync)!=0) flipDec(); // press and hold Esc, then press sync to change the dec direction...
        if ((keys&keyEsc)!=0 && (newKeyDown&keyRight)!=0)  // press and hold Esc, then press right to stop the sideral move
            CSavedData::savedData.initUncountedStep2(sideralSpeeds[(MRa.sideralMove+1)%(sizeof(sideralSpeeds)/sizeof(sideralSpeeds[0]))]); 
        if ((keys&keyEsc)!=0 && (newKeyDown&keyUp)!=0) enableFlip(!isRaFlipEnabled()); // press and hold Esc, then press UP to disable meridian flip
        if ((keys&keyEsc)!=0 && (newKeyDown&keyGo)!=0) display.reset(); // reset the display (in case it fucks up)
        if ((keys&keyEsc)!=0 && (newKeyDown&keyLeft)!=0) reboot(); // reboot the cpu...

        if ((keys&keySync)!=0)  // Sync = spiral... BUT, should not start on sync from menu!!!!
        { 
            if (!blockSync)
            {
              stopMovingOnKeyRelease= true;
              if (!MDec.isMoving() && !MRa.isMoving()) 
              {
                  spiralI++;
                  // spiralI%4 = 0:up, 1:left, 2:down, 3:right
                  // destination is spiralI/2 away
                  if ((spiralI&3)==1)      spiralDD-= (1+spiralI)/2*MDec.stepsFromReal(manualSpeeds[manualSpeed]/4);
                  else if ((spiralI&3)==2) spiralDR+= (1+spiralI)/2*MRa.stepsFromReal(manualSpeeds[manualSpeed]/60);
                  else if ((spiralI&3)==3) spiralDD+= (1+spiralI)/2*MDec.stepsFromReal(manualSpeeds[manualSpeed]/4);
                  else if ((spiralI&3)==0) spiralDR-= (1+spiralI)/2*MRa.stepsFromReal(manualSpeeds[manualSpeed]/60);
                  MDecOn(); MDec.goToSteps(spiralDD); spiralDD= MDec.dst;
                  MRa.goToSteps(spiralDR); spiralDR= MRa.dst;
              }
            }
        } else { blockSync= false; spiralDD= MDec.pos; spiralDR= MRa.pos; spiralI= 0; }

        // if move was started from keyboard and no keys. stop. If Esc, stop...
        if ((stopMovingOnKeyRelease && (keys&(keyUp|keyDown|keyRight|keyLeft|keySync))==0) || (keys&keyEsc)!=0) { MRa.stop(); MDec.stop(); }
    }




    #ifndef ESP
        if ((newKeyDown&keyMenu)!=0) { MDecOn(); if (UI==UIFocus) UI= UIMain; else UI++; } // menu change
    #else
        if (UI==UIWifi) // ESP32 setup: wifi...
        {
            char adr[20]; sprintf(adr, "%ld.%ld.%ld.%ld", ipaddr&255, (ipaddr>>8)&255, (ipaddr>>16)&255, ipaddr>>24);
            display.text(adr, 0, 0);
            display.text2(alpaca->wifi, 0, 8);
            if ((CSavedData::savedData.guidingBits&0x40)==0)
            {
                display.text("Station Down for Ap", 0, 24);
                if ((newKeyDown&keyDown)!=0) { CSavedData::savedData.guidingBits|=0x40; CSavedData::savedData.save(); }  // load back original setting. discard changes
            } else {
                display.text("AccessPnt Up for Sta", 0, 24);
                if ((newKeyDown&keyUp)!=0) { CSavedData::savedData.guidingBits&=~0x40; CSavedData::savedData.save(); }  // load back original setting. discard changes
            }
        }
        if ((newKeyDown&keyMenu)!=0) { MDecOn(); if (UI==UISetup) UI= UIMain; else UI++; } // menu change
    #endif
    if ((newKeyDown&keyEsc)!=0) UI= UIMain; // escape menu. Menu key itself is lower as there is a special handeling in the case of the setup menu




    if (UI==UIMessier || UI==UINgc || UI==UIStars || UI==UICadwell) // menu: Messier, Ngc selection and focusser
    {   // display object list and position. direction select object. go goes on object, sync resync the scope, esc exits
        int8_t move= 0;
        if ((keys&keyLeft)!=0)       { if (countToNextSelect==0) { move= -1;  countToNextSelect= delayToNextSelect; delayToNextSelect-=4; } else countToNextSelect--; } // Accelerating selection up and down....
        else if ((keys&keyRight)!=0) { if (countToNextSelect==0) { move=  1;  countToNextSelect= delayToNextSelect; delayToNextSelect-=4; } else countToNextSelect--; }
        else if ((keys&keyDown)!=0)  { if (countToNextSelect==0) { move=-10; countToNextSelect= delayToNextSelect; delayToNextSelect-=4; } else countToNextSelect--; }
        else if ((keys&keyUp)!=0)    { if (countToNextSelect==0) { move= 10; countToNextSelect= delayToNextSelect; delayToNextSelect-=4; } else countToNextSelect--; }
        else countToNextSelect= 0, delayToNextSelect= 15; // no keys press. restart slow
        if (delayToNextSelect<4) delayToNextSelect= 4; // max speed

        uint8_t x=0;
        int32_t ra, dec;
        if (UI==UIMessier) 
        { 
            findMessier+= move; if (findMessier<0) findMessier= 0; if (findMessier>=nbMessier) findMessier=nbMessier-1; 
            x= display.text("Messier", 0, 0); 
            toStr(t, findMessier+1);
            getRaDecPos(NgcDiff[read10bits(MessierToNgc10, findMessier)].radec, ra, dec);
        }
        if (UI==UICadwell) 
        { 
            findCadwell+= move; if (findCadwell<0) findCadwell= 0; if (findCadwell>=nbCadwell) findCadwell=nbCadwell-1; 
            x= display.text("Cadwell", 0, 0); 
            toStr(t, findCadwell+1);
            getRaDecPos(NgcDiff[read10bits(CadwellToNgc10, findCadwell)].radec, ra, dec);
        }
        if (UI==UINgc) 
        { 
            while (move<0) move++, Ngc.prev(); while (move>0) move--, Ngc.next(); 
            x= display.text("Ngc", 0, 0); 
            toStr(t, Ngc.externalId);
            Ngc.pos(ra, dec);
        }
        if (UI==UIStars) 
        { 
            while (move<0) move++, stars.prev(); while (move>0) move--, stars.next(); 
            display.blit(star, 0, 0); x= 18; 
            int8_t l= stars.name(t);
            if (l>8) { display.text(t, x, 4); t[0]= 0; }
            stars.pos(ra, dec);
        }
        display.text2(t, x, 0);
        testGoSync(newKeyDown, ra, dec);
    }




    if (UI==UIFocus) // focusser
    { 
        if (CSavedData::savedData.focMaxSpd==0) returnToScreen1= 0; 
        else {
            display.text2("Focusser", 0, 0);
            int32_t v= MFocus.posInReal();
            strcpy(t, "+00.000mm"); if (v<0) t[0]='-', v= -v;
            ldiv_t d= ldiv(v, 100); toStr2(t+5, uint8_t(d.rem));
            d= ldiv(d.quot, 10); t[4]='0'+uint8_t(d.rem); toStr2(t+1, uint8_t(d.quot));
            display.text2(t, 0, 16);
            MFocusOn();
            if ((newKeyDown&keyDown)!=0)       MFocus.goDown(MFocus.spdMax/40), stopMovingOnKeyRelease= true;
            else if ((newKeyDown&keyUp)!=0)    MFocus.goUp(MFocus.spdMax/40), stopMovingOnKeyRelease= true;
            else if ((newKeyDown&keyLeft)!=0)  MFocus.goDown(MFocus.spdMax), stopMovingOnKeyRelease= true;
            else if ((newKeyDown&keyRight)!=0) MFocus.goUp(MFocus.spdMax), stopMovingOnKeyRelease= true;
            if ((stopMovingOnKeyRelease && (keys&(keyUp|keyDown|keyRight|keyLeft))==0) || (keys&keyEsc)!=0) MFocus.stop(); 
            returnToScreen1= 1; // This is a "never timeout" feature here
        }
    }



    #ifdef hasPlanets
    if (UI==UIPlanets)
    {
        // Doing no repeat gains 60 bytes!
        if ((newKeyDown&keyLeft)!=0)       findPlanetUI= (findPlanetUI-1)&3;
        else if ((newKeyDown&keyRight)!=0) findPlanetUI= (findPlanetUI+1)&3;
        else if ((newKeyDown&keyDown)!=0)  
        { 
            if (findPlanetUI==0 && Planets::day>1) Planets::day--;
            else if (findPlanetUI==1 && Planets::month>1) Planets::month--;
            else if (findPlanetUI==2 && Planets::year>1) Planets::year--;
            else if (findPlanetUI==3) findPlanet= (findPlanet-1)&0x7;
            planetRecalc= 0;
        }
        else if ((newKeyDown&keyUp)!=0)    
        { 
            if (findPlanetUI==0 && Planets::day<31) Planets::day++;
            else if (findPlanetUI==1 && Planets::month<12) Planets::month++;
            else if (findPlanetUI==2 && Planets::year<50) Planets::year++;
            else if (findPlanetUI==3) findPlanet= (findPlanet+1)&0x7;
            planetRecalc= 0;
        }

        if (planetRecalc==0) { planetRecalc= 1; Planets::planetPos(findPlanet, planetra, planetdec); } // recalculate if needed

        toStr2(t, Planets::day); uint8_t x= display.text(t, 0, 0, 1, 2); x= display.text("/", x, 0);
        toStr2(t, Planets::month); x= display.text(t, x, 0, 1, 2); x= display.text("/", x, 0);
        toStr2(t, Planets::year); x= display.text(t, x, 0, 1, 2);
        memcpy_P(t, Planets::planetNames[findPlanet], 8);
        uint8_t x2= display.text(t, x+4, 0);
        if (findPlanetUI<3) display.hline(findPlanetUI*18, 12, 8); else display.hline(x+4, x2-x-4, 8);

        testGoSync(newKeyDown, planetra, planetdec);
    }
    #endif
}

// From here, we find mostly what is needed to talk to the ASCOM driver through the serial port...
static void printHex2(uint32_t v, int8_t l) // send a l digit hex number to serial
{
    char const hexstr[]= "0123456789ABCDEF";
    while (--l>=0) MSerial::print(hexstr[((v>>(4*l)))&15]);
}

static uint32_t readHex(char *&s, int8_t cnt) // read an hex value from a string...
{
    uint32_t v= 0;
    while (--cnt>=0)
    {
        if (*s>='0' && *s<='9') v= (v<<4) + *s++-'0';
        else if (*s>='a' && *s<='f') v= (v<<4) + *s++-'a'+10;
        else if (*s>='A' && *s<='F') v= (v<<4) + *s++-'A'+10;
        else return v;
    }
    return v;
}




static char input[40];                         // stores serial input
static uint8_t in= 0;                          // current char in serial input
bool decGuiding= false; // will be set to true when a dec guiding command is sent. back to false when dec movement is stopped..
uint8_t powercnt= 0;
static void inline loop() 
{
  #ifdef HASADC
    bool power= CADC::next()>90; if (lastpower!=power) // if power was applied, needs to reset the motors...
    {
        udelay(10000); 
        lastpower= power;
        if (power) 
        { 
            powercnt++;
            MRa.powerOn(); MDec.powerOn(); MFocus.powerOn(); MDecIsOn=0; MDecOn();
        }
    }
  #else
    bool const power= true;
  #endif
  uint32_t now= Time::unow();
  MRa.next(now); MDec.next(now); MFocus.next(now); // move motors as needed
  if (savedGotoForFlip.flip) // Meridian swapping!
    if (!MDec.isMoving() && !MRa.isMoving()) // wait until we reach north...
    {
      // now flip the coordinates on the 2 axes and then continue goto to saved positions...
      flipDec(); flipRa();
      savedGotoForFlip.flip= false;
      goTo(savedGotoForFlip.ra, savedGotoForFlip.dec);
    }

  doUI(); // Do UI.

  ///////////////////////////////////////////
  // serial input processing
  // ! -> returns 56 hex characters. pos in Dec(6), Ra(6), focusser(6), 1 byte (2 chr) of status bits, millis_timer(6), meridian_value(6), uncounted_steps(6), raPos(8), decPos(8), power and sideralspeed(2). Then a #
  // & -> returns SavedData as a hex sequence ending with #. The last byte is the CRC...
  // @Hex# is the reverse to set the settings... crc HAS to be correct!
  // For all other commands, lines start with ':' and end with '#' or '\n' Go look at them, they are not that many of them!!!!

  while (true)
  {
    int16_t c= MSerial::read(); if (c==-1) break;            // no data...
    if (c<=' ') continue;                                    // ignore blanks
    if (c=='!')   // get info command
    {
        printHex2(MDec.posInReal(), 6); printHex2(MRaposInReal(), 6); printHex2(MFocus.pos>>8, 6);
        bool decMove= MDec.isMoving(); if (!decMove) decGuiding= false; 
        // bit 0: moving, bit 1: focus moving, bit 2: side of pier, bit 3: meridian swapping, bit 4: flip disabled,
        //   bit 5: tracking disabled, bit 6: power, bit 7: guiding
        printHex2(((decMove||MRa.isMoving())?1:0) | (MFocus.isMoving()?2:0) | (scopeWest() ? 4:0) | (savedGotoForFlip.flip?8:0) | 
                   (isRaFlipEnabled()?0:16) | (MRa.deltaBetweenUncountedSteps==0?32:0) | (power?64:0) | ((decGuiding||(MRa._guide!=0))?128:0), 2);
        printHex2(now/1000, 6);                             // this allows to verify time drift
        printHex2(Abs(MRa.minPosReal+MRa.maxPosReal)/2, 6); // This allows to check if something will cause a flip or not...
        printHex2(MRa.countAllUncountedSteps, 6);           // this is also a time drift check
        printHex2(MRa.pos, 8); printHex2(MDec.pos, 8);      // motor mechanical position ra for flip calculation. dec not used at this point
        printHex2((powercnt&0x1f) | (MRa.sideralMove<<5), 2); 
        MSerial::flush('#');
        continue;
    }
    if (c=='&') 
    { // serial write hex represenation of CSavedData (get settings)
        CSavedData::savedData.crc= CSavedData::savedData.calcCrc(); uint8_t *d= (uint8_t*)&CSavedData::savedData;
        for (uint8_t i=0; i<sizeof(CSavedData::savedData); i++) printHex2(*d++, 2);
        #ifdef ESP
            uint8_t *p= (uint8_t*)alpaca->wifi;
            for (uint8_t i=0; i<2*sizeof(alpaca->wifi); i++) printHex2(p[i], 2);
            printHex2(ipaddr, 8);
        #endif
        MSerial::flush('#');
        continue;
    }
    if (c=='@')
    { // serial read hex representation of CSavedData (save settings)
        #ifndef ESP
            now+= 1000000ULL; // 1s timeout...
            for (uint8_t i=0; i<sizeof(CSavedData); i++)
            {
                char t[2]; for (int8_t p=0; p<2; p++)  // read 2 chars (1 byte)
				  while (true) { if (now<Time::unow()) goto er; int c= MSerial::read(); if (c<0) continue; t[p]= c; break; }
                char *T= t; ((uint8_t*)&CSavedData::savedData)[i]= readHex(T,2);
            }
            if (CSavedData::savedData.testCrc()) CSavedData::savedData.save(); else { er: CSavedData::savedData.load(); } // verify crc!
        #else
            uint8_t buf[2*(sizeof(CSavedData)+sizeof(alpaca->wifi)+sizeof(alpaca->wifip))]; int pos= 0; // buff for inbound data...
            // copy data that we already have in buffer...
            if (nb<=sizeof(buf)) { pos= nb; memcpy(buf, d, nb); nb= 0; } else { pos= sizeof(buf); memcpy(buf, d, sizeof(buf)); nb-= sizeof(buf); d+= sizeof(buf); }
            // get missing data...
            while (pos<sizeof(buf)) if ((nb= MSerial.read(buf+pos, sizeof(buf)-pos))!=0) pos+=nb; else goto er;
            {
                char *t= (char*)buf; for (uint8_t i=0; i<sizeof(buf)/2; i++) buf[i]= readHex(t,2); // To binary...
                if (((CSavedData*)buf)->testCrc()) // check crc and save data if valid...
                { 
                    memcpy(&CSavedData::savedData, buf, sizeof(CSavedData)); CSavedData::savedData.save(); 
                    memcpy(alpaca->wifi, buf+sizeof(CSavedData), sizeof(alpaca->wifi)+sizeof(alpaca->wifip)); 
                    alpaca->save("wifi", alpaca->wifi);
                    alpaca->save("wifip", alpaca->wifip);
                }
            } er: 
        #endif
		reboot(); // reboot the system!
    }

    if (in==0 && c!=':') continue;                         // nothing until ':' (line start)...
    input[in++]= char(c); if (in==sizeof(input)) { in= 0; continue; } // save new character and overflow detection...
    if (c!='#' && c!='\n') continue;                       // not end of line... get next character
    in=0;                                                  // reset line
    char *s= input+3; // in most cases, numbers start at input+3. init once only
    // now look for commands in line...
#define t1(c) input[1]==c
#define t2(c1,c2) input[1]==c1 && input[2]==c2
    if (t2('$', 'P')) { flipDec(); continue; }
    if (t2('$', 'f')) { enableFlip(input[3]!='0'); continue; } // flip on/off
    if (t1('Q')) { savedGotoForFlip.flip= false; MFocus.stop(); MDec.stop(); MRa.stop(); continue; } // :Q# stop driven movements (including focusser)

    if (t1('T')) // track. provides dst in ra/dec as int24, time to be there in ms as int16. then a crc as int8
    { s--;
      int32_t ra= readHex(s,6); int32_t dec= readHex(s,6); if ((dec&0x800000)!=0) dec|= 0xff000000; // negative extend.
      int16_t time= readHex(s,4);
      uint8_t crc= readHex(s,2);
      uint8_t tcrc= uint8_t(ra)+uint8_t(ra>>8)+uint8_t(ra>>16) + uint8_t(dec)+uint8_t(dec>>8)+uint8_t(dec>>16) + uint8_t(time)+uint8_t(time>>8);
      if (tcrc!=crc) continue;
      goTo2(ra, dec, time); continue;
    }
    if (t2('M', 'R')) reboot(); // reboot the system!
    // All the commands from there on will take 1 or 2 inputs. In 8 char hex form most significant nible first in our input... We read them...
    int32_t n1= readHex(s,8); int32_t n2= readHex(s,8); 
    if (t2('M', 'G')) { goTo(n1, n2); continue; } // MOVE:  :MS# (slew) // after sending Sr and Sd, will ask to move!. print(0)= slew is possible 
    if (t2('M', 'g')) { MRa.goToSteps(n1); MDec.goToSteps(n2); stopMovingOnKeyRelease= false; continue; } // go to steps
    if (t2('M', 'S')) { sync(n1, n2); continue; } // :CM# assumes Sr and Sd have been processed sync current position with input
    if (t2('M', 'd')) { MDecOn(); MDec.goUpRealNoAbs(n1); stopMovingOnKeyRelease= false; continue; } // :Mdspd(8)# (move dec in direction at speed)
    if (t2('M', 'r')) { MRa.goUpRealNoAbs(n1); stopMovingOnKeyRelease= false; continue; } // :Mrspd(8)# (move ra in direction at speed)
    if (t2('M', 'f')) { flip(); continue; } // // force a meridian flip (assumes that the user has verified that it was possible!)
    if (t2('p', 'r')) { MRa.guide(n1, n2, now); stopMovingOnKeyRelease= false; continue; } // :prstp(8)spd(8)# pulse guide ra for step count at speed
    if (t2('p', 'd')) { MDecOn(); decGuiding= true; MDec.guide(n1, n2); stopMovingOnKeyRelease= false; continue; } // :pdstp(8)spd(8)# pulse guide dec for step count at speed
    if (t2('$', 'T')) { CSavedData::savedData.initUncountedStep2(n1); continue; } // Track at speed to allow lunar/solar... parameter is second per full turn...
    // Focusser commands
    if (t2('F', 'G')) { MFocusOn(); MFocus.goToSteps(n1<<8, MFocus.spdMax); stopMovingOnKeyRelease= false; continue; } // -> start the motor toward destination
    if (t2('F', 'M')) { MFocusOn(); MFocus.goUp(n1<<8); stopMovingOnKeyRelease= false; continue; } // :FMxxxxxxxx# move out if x is >0, else in. x is 8 hex chars
  }
}

static void setup()
{
    #ifdef HASADC
	    CADC::begin();
	#endif
    Time::begin();
    portSetup(); // Should have the default values for the pins...
    MSerial::begin();
    display.begin();
    CSavedData::savedData.load(); // motors are initialized here..
}
#ifndef PC
int main() 
{
    setup();
    while (true) loop();
}
#endif
