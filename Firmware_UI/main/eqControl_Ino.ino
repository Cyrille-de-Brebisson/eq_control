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

#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"

static const gpio_num_t LCD_SLC= GPIO_NUM_9;
static const gpio_num_t LCD_SDA= GPIO_NUM_8;

namespace CGPIO {
    void output(uint64_t mask)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = mask;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
    }
    void input(uint64_t mask, bool poolup)
    {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = mask;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = poolup?GPIO_PULLUP_ENABLE:GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
    }
    int inline read(int pin) { return gpio_get_level(gpio_num_t(pin)); }
    #define GPIO_OUT_W1TS_REG_R     (*((volatile uint32_t*)0x60004008))
    #define GPIO_OUT_W1TC_REG_R     (*((volatile uint32_t*)0x6000400C))
    #define GPIO_IN_REG_R  (*((volatile uint32_t*)0x6000403C))
    //#define GPIO_OUT_W1TS_REG_R     (*((volatile uint32_t*)GPIO_OUT_W1TS_REG))
    //#define GPIO_OUT_W1TC_REG_R     (*((volatile uint32_t*)GPIO_OUT_W1TC_REG))
    int pins= 0;
    void inline IRAM_ATTR set(int pin, int value) 
    { 
        if (value) GPIO_OUT_W1TS_REG_R= 1<<pin, pins|= 1<<pin; else GPIO_OUT_W1TC_REG_R= 1<<pin, pins&= ~(1<<pin);
        //gpio_set_level(gpio_num_t(pin), value); 
    }
    void inline IRAM_ATTR toggle(int pin_mask)  // WARNING, the pin is pre-shifted here!!
    { 
        pins^= pin_mask;
        if ((pins&pin_mask)!=0) GPIO_OUT_W1TS_REG_R= pin_mask; else GPIO_OUT_W1TC_REG_R= pin_mask;
        //gpio_set_level(gpio_num_t(pin), value); 
    }
    void inline IRAM_ATTR set(int pin) 
    { 
        GPIO_OUT_W1TS_REG_R= 1<<pin; pins|= 1<<pin;
    }
    void inline IRAM_ATTR clear(int pin) 
    { 
        GPIO_OUT_W1TC_REG_R= 1<<pin; pins&= ~(1<<pin);
    }
};
static void GPIOSetup()
{

    CGPIO::input(0b1100000000010010011111, true);
}
static uint16_t kbdValue()
{
    uint16_t keys= 0;
    if (CGPIO::read(0)==0) keys|= keyRight;
    if (CGPIO::read(1)==0) keys|= keySpd;
    if (CGPIO::read(2)==0) keys|= keyUp;
    if (CGPIO::read(3)==0) keys|= keyEsc;
    if (CGPIO::read(4)==0) keys|= keySync;
    if (CGPIO::read(7)==0) keys|= keyDown;
    if (CGPIO::read(10)==0) keys|= keyLeft;
    if (CGPIO::read(20)==0) keys|= keyGo;
    if (CGPIO::read(21)==0) keys|= keyMenu;
    return keys;
}

static int const maxPulsesPerSecond= 10000;

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
    uint32_t IRAM_ATTR mnow() // THIS IS NOT milliseconds, but 1024th of seconds!
    {
        TIMERG0.hw_timer[0].update.tx_update = 1; while (TIMERG0.hw_timer[0].update.tx_update) { }
        return ((uint64_t)TIMERG0.hw_timer[0].hi.tx_hi << 22) | ((TIMERG0.hw_timer[0].lo.tx_lo)>>10);
    }
}

class CSerial { public:
    virtual int16_t read() = 0;
    virtual int read(uint8_t *d, int size) = 0;
    virtual void flush(char const *s, int size) = 0;
    void flush(char const *s) { flush(s, strlen(s)); }

    char buf[400]; int bufl= 0;
    void inline print(char c) { buf[bufl++]= c; } // Add char in output buffer
    void print(char const *c) // Add string in output buffer..
	{ 
		int l= strlen(c); if (l==0) return; 
		if (l+bufl>sizeof(buf)) { flush(buf, bufl); bufl= 0; }
		memcpy(buf+bufl, c, l); bufl+= l; 
	}
    void flush(char c) { print(c); flush(buf, bufl); bufl= 0; }
};

#define reboot esp_restart

#define PROGMEM
static uint8_t inline pgm_read_byte(uint8_t const *p) { return *p; }
static uint16_t inline pgm_read_word(uint8_t const *p) { return p[0] | (((uint16_t)p[1])<<8); }
static uint16_t inline pgm_read_word(uint16_t const *p) { return *p; }
#define memcpy_P memcpy










///////////////////////////////////////////////////////////
// Here is the real start of the whole program!!!!

#include "catalogs.h" // Messier, NGC and star list...

static uint32_t inline Abs(int32_t v) { return v>=0 ? v: uint32_t(-v); }

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
 
} I2C;
//********************************************
// Framebuffer based display This has the framebuffer which is sent through I2C...
// This class has all the drawing functions... and uses the I2C above...
class CDisplay { public: 
    uint8_t preCommand[4]; // stores the start of what has to be send to the HW to display the screen... just before the fb so we can just point there and send all...
    uint8_t fb[128/8*32]; // 512 bytes of RAM for the frame buffer. by packs of 8 lines wih 1 byte = 8 vertical pixels!!!! crapy formatting!!!!
    void clear() { memset(fb, 0, sizeof(fb)); } 

    void begin() 
    {
        I2C.setupDevice(0, 0x3c, 125000);  // 125khz... Screen is around 530 bytes with control ~= 30fps which means that UART and keyboard work at that speed...
        // LCD init sequence
        static uint8_t const lcdInit[] = { 0,
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
        I2C.send(0, lcdInit, sizeof(lcdInit)); // init LCD
        screenOn();
    }
    void screenOn() { preCommand[1]= 0xAF; }
    void screenOff() { preCommand[1]= 0xAE; }
    bool inline isScreenOff() { return preCommand[1]==0xAE; }
    void disp() // send the whole screen and wait until sent to continue... Not used in this project
    { 
        preCommand[0]= 0; preCommand[2]= 0xb0;
        I2C.send(0, preCommand, 3);
        fb[-1]= 0x40; I2C.send(0, fb-1, 512+1);
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
            uint8_t v= f[i];
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
            uint8_t v= f[i>>1];
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
    // b is in and is width, height and then the bitmap bits, vertically, with the next column starting as soon as the previous is done (no empty space)
    void blit(uint8_t const *b, uint8_t x, uint8_t y)
    {
        int8_t w= *b++; int8_t h= *b++; uint8_t B=0; int8_t bs= 1;
        uint8_t *P= fb+x+(y>>3)*128; y= 1<<(y&7);
        while (--w>=0)
        {
            uint8_t *p= P; uint8_t v= *p; uint8_t m= y; int8_t r= h;
            while (true)
            {
                if (--bs==0) { bs= 8; B= *b++; }
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

bool display_pixel(int x, int y) { return display.pixel(x,y); }

        uint8_t const CDisplay::font8[] ={ // 6*8 pixel font with only chr 32 to 127
            /* */ 0,0,0,0,0,0,0,0, /*!*/ 4,4,4,4,4,0,4,0, /*"*/ 10,10,10,0,0,0,0,0, /*#*/ 10,10,31,10,31,10,10,0, /*$*/ 4,30,5,14,20,15,4,0, /*%*/ 3,19,8,4,2,25,24,0, /*&*/ 2,5,5,2,21,9,22,0, /*'*/ 4,4,4,0,0,0,0,0, /*(*/ 8,4,2,2,2,4,8,0, /*)*/ 2,4,8,8,8,4,2,0, /***/ 0,10,4,31,4,10,0,0, /*+*/ 0,4,4,31,4,4,0,0, /*,*/ 0,0,0,0,6,6,4,2, /*-*/ 0,0,0,31,0,0,0,0, /*.*/ 0,0,0,0,0,6,6,0, /*/*/ 0,16,8,4,2,1,0,0,
            /*0*/ 14,17,25,21,19,17,14,0, /*1*/ 4,6,4,4,4,4,14,0, /*2*/ 14,17,16,12,2,1,31,0, /*3*/ 14,17,16,14,16,17,14,0, /*4*/ 8,12,10,9,31,8,8,0, /*5*/ 31,1,15,16,16,17,14,0, /*6*/ 12,2,1,15,17,17,14,0, /*7*/ 31,16,8,4,2,2,2,0, /*8*/ 14,17,17,14,17,17,14,0, /*9*/ 14,17,17,30,16,8,6,0, /*:*/ 0,6,6,0,6,6,0,0, /*;*/ 0,6,6,0,6,6,4,2, /*<*/ 8,4,2,1,2,4,8,0, /*=*/ 0,0,31,0,31,0,0,0, /*>*/ 1,2,4,8,4,2,1,0, /*?*/ 14,17,16,8,4,0,4,0,
            /*@*/ 14,17,21,29,5,1,30,0, /*A*/ 14,17,17,31,17,17,17,0, /*B*/ 15,17,17,15,17,17,15,0, /*C*/ 14,17,1,1,1,17,14,0, /*D*/ 7,9,17,17,17,9,7,0, /*E*/ 31,1,1,15,1,1,31,0, /*F*/ 31,1,1,15,1,1,1,0, /*G*/ 14,17,1,1,25,17,30,0, /*H*/ 17,17,17,31,17,17,17,0, /*I*/ 14,4,4,4,4,4,14,0, /*J*/ 16,16,16,16,17,17,14,0, /*K*/ 17,9,5,3,5,9,17,0, /*L*/ 1,1,1,1,1,1,31,0, /*M*/ 17,27,21,21,17,17,17,0, /*N*/ 17,17,19,21,25,17,17,0, /*O*/ 14,17,17,17,17,17,14,0,
            /*P*/ 15,17,17,15,1,1,1,0, /*Q*/ 14,17,17,17,21,9,22,0, /*R*/ 15,17,17,15,5,9,17,0, /*S*/ 14,17,1,14,16,17,14,0, /*T*/ 31,4,4,4,4,4,4,0, /*U*/ 17,17,17,17,17,17,14,0, /*V*/ 17,17,17,10,10,4,4,0, /*W*/ 17,17,17,21,21,27,17,0, /*X*/ 17,17,10,4,10,17,17,0, /*Y*/ 17,17,10,4,4,4,4,0, /*Z*/ 31,16,8,4,2,1,31,0, /*[*/ 14,2,2,2,2,2,14,0, /*\*/ 0,1,2,4,8,16,0,0, /*]*/ 14,8,8,8,8,8,14,0, /*^*/ 4,10,17,0,0,0,0,0, /*_*/ 0,0,0,0,0,0,31,0,
            /*`*/ 2,2,4,0,0,0,0,0, /*a*/ 0,0,14,16,30,17,30,0, /*b*/ 1,1,15,17,17,17,15,0, /*c*/ 0,0,30,1,1,1,30,0, /*d*/ 16,16,30,17,17,17,30,0, /*e*/ 0,0,14,17,31,1,14,0, /*f*/ 4,10,2,7,2,2,2,0, /*g*/ 0,0,14,17,17,30,16,14, /*h*/ 1,1,15,17,17,17,17,0, /*i*/ 4,0,6,4,4,4,14,0, /*j*/ 8,0,12,8,8,8,9,6, /*k*/ 1,1,9,5,3,5,9,0, /*l*/ 6,4,4,4,4,4,14,0, /*m*/ 0,0,11,21,21,21,17,0, /*n*/ 0,0,15,17,17,17,17,0, /*o*/ 0,0,14,17,17,17,14,0,
            /*p*/ 0,0,15,17,17,15,1,1, /*q*/ 0,0,30,17,17,30,16,16, /*r*/ 0,0,29,3,1,1,1,0, /*s*/ 0,0,30,1,14,16,15,0, /*t*/ 2,2,7,2,2,10,4,0, /*u*/ 0,0,17,17,17,17,30,0, /*v*/ 0,0,17,17,17,10,4,0, /*w*/ 0,0,17,17,21,21,10,0, /*x*/ 0,0,17,10,4,10,17,0, /*y*/ 0,0,17,17,17,30,16,14, /*z*/ 0,0,31,8,4,2,31,0, /*{*/ 12,2,2,1,2,2,12,0, /*|*/ 4,4,4,4,4,4,4,0, /*}*/ 6,8,8,16,8,8,6,0, /*~*/ 0,0,2,21,8,0,0,0, /*¦*/ 7,5,7,0,0,0,0,0,
        };




///////////////////////////////////////////////////////////////////////
// EEprom for setting saving to eeprom...
  static uint8_t toStr(char *s, uint32_t v);

static uint32_t const sideralSpeeds[5]= {0, 23*3600UL+56*60+4, 24*3600UL+50*60, 24*3600UL, 1299188UL}; // King

#if 1 // This can probably be updated to float on esp32! But this code is so beautifull on Arduino :-)
#define hasPlanets
namespace Planets {
//////////////////////////////////////////////
// Planetary stuff. Look for planetPos function at the bottom
typedef int32_t Cartesian; // Coordinates in AU in 1+7+24 format (1 bit sign, 7 bit IP, 24 bit FP) for a max value of 127AU which is OK here...
Cartesian Cmul(Cartesian v1, Cartesian v2) { return Cartesian((int64_t(v1)*v2)>>24); }
Cartesian Cmul(Cartesian v1, Cartesian v2, Cartesian v3) { return Cartesian((((int64_t(v1)*v2)>>24)*v3)>>24); }
Cartesian Cdiv(Cartesian v1, Cartesian v2) { return Cartesian((int64_t(v1)<<24)/v2); }

Cartesian sqrt(int64_t v);
Cartesian sqrt(Cartesian a, Cartesian b, bool sign=false) // return sqrt a*a+/-b*b (substract b if sign = true)
{
    int64_t v= int64_t(b)*b; // do the multiplication in 64 bit mode...
    if (sign) v= -v;
    v+= int64_t(a)*a;
    if (v<=0) return 0;
    // Here, v is a^2+/-b^2 with 48 bit precision...
    return sqrt(v); // let us hope for a compiler reorder and fall through!
}
Cartesian sqrt(int64_t v) // return sqrt v with 48 bit precision
{
    uint32_t t= uint32_t(v>>48); uint8_t bits= 0; while (t!=0) { bits++; t>>=1; } // count the number of bits of ip to generate the initial value close to the sqrt(v)
    int64_t x= v>>((bits>>1)+24); if (x==0) x= 1; // remove the extra 24 bits of precision from the mul and shift by bits/2 bits as first guess on sqrt..
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
static int8_t findMessier= 0, findCadwell=0;    // last select messiers et cadwell
static uint8_t findPlanet= 4, findPlanetUI= 0;  // last selected planet, and what is the current UI entry in this screen
static uint8_t planetRecalc=0; int32_t planetra, planetdec; // cache for ra/dec calculation for planet. if recalc is 0, then it needs to be recalculated
static uint8_t countToNextSelect; int8_t delayToNextSelect= 15; // to accelerate when press&hold keys in messier/ngc selection
static uint8_t returnToScreen1= 0;             // down counter to go back to screen 0 UI (ticks every screen refresh)
static int16_t lastkbd= 0;                     // status of last keyboard scann for debounce
static uint32_t spiralA= 0; static int32_t spiralra=0, spiraldec=0;             // spiralA is the spiral Angle with 1<<16 = 1 full turn (so 16 bit precision) the 2 others are the "center of the spiral"
static bool blockSync= false;                  // if true, do not execute sync key (used to avoid exit from star menu with sync leads to sync going on...)
static const uint32_t UIDelay= 50000;           // Pool time of keyboard. UI unit of time. 20 times per second...
static const uint16_t timeToScreenOffConst= 10*20;   // 10s timout on the screen. The 20 is the UI delay unit of time...
static uint16_t timeToScreenOff= timeToScreenOffConst; // countdown to screen off...
static bool stopMovingOnKeyRelease= true;     // if true, then once no key is pressed, stop moving... records who said move/don't move
static uint8_t manualSpeed= 3;                 // current speed in manualSpeeds
static uint16_t const manualSpeeds[]= { 60, 60*15, 60*30, 60*60, 2*60*60, 4*60*60, 8*60*60, 12*60*60 }; // list of possible speed in manual mode in deg per second. WARNING, RA is in h, not deg units!
static const int8_t nbSpeeds= sizeof(manualSpeeds)/sizeof(manualSpeeds[0]);
static char const manualSpeedsTxt[]=" 1'15'30' 1\x7f 2\x7f 4\x7f 8\x7f""12\x7f"; // text for the above

static bool isRaFlipEnabled()
{ // noFlip
    return !noFlip;
}

static void toHex(char *s, uint32_t v, int8_t l) // send a l digit hex number to serial
{
    char const hexstr[]= "0123456789ABCDEF";
    while (--l>=0) *s++= hexstr[((v>>(4*l)))&15];
}

class CMotor { public:
    CMotor(char t) : type(t) { }
    char type;
    void goDownReal(int speed) { char c[]=":M 00000000#"; c[2]= type; toHex(c+3, speed, 8); command(c); }
    void goUpReal(int speed) { char c[]=":M 00000000#"; c[2]= type; toHex(c+3, -speed, 8); command(c); }
    void stop() { command(":Q#"); }
} MRa('r'), MDec('d');

class CMotor2 { public:
    void goDown(int speed) { char c[]=":FM00000000#"; toHex(c+3, speed, 8); command(c); }
    void goUp(int speed) { goDown(-speed); }
    void stop() { command(":Q#"); }
} MFocus;

// return true if no need to do a flip...
static bool sameSideOfMeridian(int32_t targetRa)
{
    if (!noFlip) return true;
    targetRa-=midPos;
    if (abs(targetRa)<6*3600+1800) return true;
    if (abs(targetRa)<18*3600-1800) return true;
    return false;
}

static void sync(int32_t ra, int32_t dec) // Set scope position to coordinates..
{
    char t[22]= ":MS0000000000000000#";
    toHex(t+3, ra, 8); toHex(t+3+11, dec, 8);
    command(t);
}

static void goTo(int32_t ra, int32_t dec, bool visual)
{
    char t[22]= ":MG0000000000000000#";
    toHex(t+3, ra, 8); toHex(t+3+11, dec, 8);
    command(t);
}

static void flipDec()
{
    command(":$P#");
}

void flip() // This only works if amplitude > 180° and scope is close to edge! Not tested at this point in time... Assumes user is not dumb!
{
    command(":Mf#");
}

static void enableFlip(bool en)
{
    char t[]= ":$f0#"; if (en) t[3]='1';
    command(t);
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
    uint8_t const earth[] PROGMEM = { 8, 8, 60, 114, 161, 137, 157, 181, 126, 60};

static void dispRaDec(int32_t ra, int32_t dec) // dispaly ra/dec on screen
{ 
    char radecbuf[3];
    const uint8_t y= 18; uint8_t t, x;
    // disp ra
    ra= ra/60; t= ra%60; ra/=60;
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
    return sideOfPeir;
}

// if go, or sync pressed, then execute as needed... Saves memory to put it there...
static void testGoSync(uint16_t newKeyDown, int32_t ra, int32_t dec)
{
    if ((newKeyDown&keyGo)!=0)  // validation button
    { 
        goTo(ra, dec, true);
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
static void doUI() // Display takes around 5ms...
{
    char t[22]; // Text buffer...
 
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
    if (keys!=0) display.screenOn(), returnToScreen1= 20*10, timeToScreenOff= timeToScreenOffConst;   // if a key is pressed, we reset the returnToScreen1 timer...
    if (returnToScreen1==0) UI= UIMain; else returnToScreen1--; // at 0, reset UI. else tick down timer
	#ifndef PC // no screen auto off in PC mode...
	    if (timeToScreenOff==0) { display.screenOff(); display.disp(); return; }
	    timeToScreenOff--;
	#endif

    display.clear(); // No need as screen is erased as we go...

    if (flipping) // Meridian swapping!
    {
        display.text2("Flip", 32, 0);
        dispRaDec(MRAPos, MDecPos);
        if ((newKeyDown&keyEsc)==0) return display.disp();
        MRa.stop(); // stops all
    }

    if ((newKeyDown&keyMenu)!=0) { if (UI>=UIWifi) UI= UIMain; else UI++; } // menu change
    if ((newKeyDown&keyEsc)!=0) UI= UIMain; // escape menu.

    if (UI==UIMain) // Main screen. move, speed, menu and spiral
    {
        display.text2("Ra", 0, 0); display.text2("Dec", 94, 0, 0); dispRaDec(MRAPos, MDecPos);
        uint8_t x= display.text("spd:   /s", (94+24-9*6-1-7)/2, 0); display.text(manualSpeedsTxt+manualSpeed*3,(94+24-9*6-1-7)/2+24,0, 1,3);
        // if (!lastpower) display.hline((94+24-9*6-1-7)/2, x-(94+24-9*6-1-7)/2, 4);
        display.blit(scopeWest() ? scopeW:scopeE, x+1, 0);
        if (!isRaFlipEnabled()) display.hline(x+2, 3, 5);
        static char const speeds[6][8]= {"Stopped", "Sideral", "Moon", "Sun", "King", "Unknow"};
        x= display.text(speeds[sideralSpeed], 30, 8);


        if ((newKeyDown&keyLeft)!=0) { stopMovingOnKeyRelease= true; MRa.goDownReal(manualSpeeds[manualSpeed]/15); }
        if ((newKeyDown&keyRight)!=0) { stopMovingOnKeyRelease= true; MRa.goUpReal(manualSpeeds[manualSpeed]/15); }
        if ((newKeyDown&keyUp)!=0) { stopMovingOnKeyRelease= true; MDec.goUpReal(manualSpeeds[manualSpeed]); }
        if ((newKeyDown&keyDown)!=0) { stopMovingOnKeyRelease= true; MDec.goDownReal(manualSpeeds[manualSpeed]); }
        if ((newKeyDown&keySpd)!=0)
        {
            manualSpeed= (manualSpeed+1)%nbSpeeds;
        }
      
        if ((keys&keyEsc)!=0 && (newKeyDown&keySync)!=0) flipDec(); // press and hold Esc, then press sync to change the dec direction...
        if ((keys&keyEsc)!=0 && (newKeyDown&keyRight)!=0) // press and hold Esc, then press right to stop the sideral move
        {
            sideralSpeed= (sideralSpeed+1)%(sizeof(sideralSpeeds)/sizeof(sideralSpeeds[0]));
            char t[20]= ":$T00000000#"; toHex(t+3, sideralSpeeds[sideralSpeed], 8); command(t);
        }
        if ((keys&keyEsc)!=0 && (newKeyDown&keyUp)!=0) enableFlip(!isRaFlipEnabled()); // press and hold Esc, then press UP to disable meridian flip
        if ((keys&keyEsc)!=0 && (newKeyDown&keyGo)!=0) display.begin(); // reset the display (in case it fucks up)
        if ((keys&keyEsc)!=0 && (newKeyDown&keyLeft)!=0) reboot(); // reboot the cpu...

        if ((keys&keySync)!=0)  // Sync = spiral... BUT, should not start on sync from menu!!!!
        { 
            if (!blockSync) // Sync does not cause spiralling if sync key was just used to do a sync!!!!!
            {
              // The bais parametric equation of a spiral is (x,y)=r*(cos(angle),sin(angle))
              // I want constant speed on the perimeter of the elipse.
              // At any time T, the radius is R and the perimeter is 2*PI*R, so I need to move by speed/2*PI*R fraction of turn per second
              // so my angle needt to grow by speed/(2*PI*R*movements_per_second) fraction of a circle per interval.
              // 
              // The radius will be growing in a constant way. The spacing between 2 successive point on the same "angle" of the elipse has to be the same
              // some value proportional to the 2^speed_modifier. So that if at speed=0, we have a spacing of 60 arc", at speed=1, we have 120, and at speed=4 we have 16*60~= 1/4°
              // so we take the last used angle, expressed in turns, and we multiply it by 2^speed and by our 60arc" to get the radius.
              // 
              // So, we first calculate radius based on last angle. Then we calculate new angle, then we calculate 

              // spiralA in 16.16 format. trig takes 16 bit number for turn and return val<<24 precision result
              //int32_t spiralR= ((spiralA>>8)*(1<<manualSpeed)*60)>>8; // in arc". the >>16 is because spiralA is in turn<<16
              /* To be redone here...
              int32_t spiralR= spiralA>>(10-manualSpeed); // This line is equivalent to the line above, with the 60 replaced by a 64 (the 60 being the arc" separatinon from circle to circle). But this saves 24 bytes!
              #define fps 32
              #define fpsFactor 207
              spiralA+= (uint32_t(manualSpeeds[manualSpeed])<<16)/(fpsFactor*spiralR); // increment on angle to get constant speed... This could be partially precomputed, but the division would still be there :-(
              int32_t nra= (spiralR*(Planets::cos(spiralA)>>8))>>16, ndec= (spiralR*(Planets::sin(spiralA)>>8))>>16; // next x/y of spiral
              nra= MRa.stepsFromRealNoAbs(nra/16)+spiralra, ndec= MDec.stepsFromRealNoAbs(ndec)+spiraldec; // convert to steps. Note that ra is in 24 units, not 360! so the /16 (which is close enough to the nominal 15)
              MRa.goUp((nra-MRAPos)*fps), MDec.goUp((ndec-MDecPos)*fps); // go in the direction that is needed... Note that we probably should add some ra offset for lost stellar movement!
              stopMovingOnKeyRelease= true;
              */
            }
        } else { blockSync= false; spiralra= MRAPos; spiraldec= MDecPos; spiralA= 1U<<15; } // if sync key not pressed. Save current position and init angle at 1/2 turn (could be 1/4), can not be 0 as it would cause a /0 error.

        // if move was started from keyboard and no keys. stop. If Esc, stop...
        if ((stopMovingOnKeyRelease && (keys&(keyUp|keyDown|keyRight|keyLeft|keySync))==0) || (keys&keyEsc)!=0) { MDec.stop(); }
        display.disp(); return;
    }

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
        display.disp(); return;
    }




    if (UI==UIFocus) // focusser
    { 
        if (true)
        {
            display.text2("Focusser", 0, 0);
            int32_t v= MFocusPos;
            strcpy(t, "+00.000mm"); if (v<0) t[0]='-', v= -v;
            ldiv_t d= ldiv(v, 100); toStr2(t+5, uint8_t(d.rem));
            d= ldiv(d.quot, 10); t[4]='0'+uint8_t(d.rem); toStr2(t+1, uint8_t(d.quot));
            display.text2(t, 0, 16);
            if ((newKeyDown&keyDown)!=0)       MFocus.goDown(MFocusspdMax/40), stopMovingOnKeyRelease= true;
            else if ((newKeyDown&keyUp)!=0)    MFocus.goUp(MFocusspdMax/40), stopMovingOnKeyRelease= true;
            else if ((newKeyDown&keyLeft)!=0)  MFocus.goDown(MFocusspdMax), stopMovingOnKeyRelease= true;
            else if ((newKeyDown&keyRight)!=0) MFocus.goUp(MFocusspdMax), stopMovingOnKeyRelease= true;
            if ((stopMovingOnKeyRelease && (keys&(keyUp|keyDown|keyRight|keyLeft))==0) || (keys&keyEsc)!=0) MFocus.stop(); 
            returnToScreen1= 1; // This is a "never timeout" feature here
            display.disp(); return;
        } else { UI++; display.disp(); return; } // not supported. Move to next thing!
    }

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
        display.disp(); return;
    }
    UI= UIMain; // did not find anything to display, so back to square one!
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