#include "system.h"
#ifdef NO_UI
bla;;;
void doUI() { vTaskDelay(33/portTICK_PERIOD_MS); } // 30 loop per second for serial
void beginUI() {}
bool display_pixel(int x, int y) { return false; }
#else

#ifdef ESP_PLATFORM
#include "esp_adc/adc_continuous.h"
namespace CADC {
    static uint8_t const adcPins[]={2, 3, 4, 1};
    static int const powerPin= 3; // power pin index!
    static int const nbChannels= sizeof(adcPins)/sizeof(adcPins[0]);
    static int const nbSamples= 16;
    static int const sampleSize= nbChannels*nbSamples*4;
    int8_t adcChannelToPinId[7]= {-1, -1, -1, -1, -1, -1, -1};
    uint32_t res[nbChannels]= {4096,4096,4096,0};
    int power= 0; // latest power supply read in deci volts.
    int next() { return power; } // This is for existing reasons....
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
        while (res[powerPin]!=0) vTaskDelay(1); // forces a read to init power!
        power= res[powerPin]/33; // I get a reading of 1780 for 5.4V. Since the value is in deci volts a division by /33 (1780/54) gives the value
    }
};
void kbdStart() { CADC::begin(); }

static uint32_t lastkbdValue1= 0, lastkbdValue2= 0, lastValid= 0;
// Keyboard is set as 3 analog inputs with 3 keys on each line using resistance ladder
static uint32_t kbdValue()
{
    CADC::power= CADC::res[3]/33; // I get a reading of 1780 for 5.4V. Since the value is in deci volts a division by /33 (1780/54) gives the value
    uint32_t *v= CADC::res; 
    // Adc values are: 2800 for col 1, 2000 for col 2 and 0 for col 3
    uint32_t keys= 0;
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
#else // windows
static void kbdStart() { }
uint32_t kbdMask= 0; // on windows this will be set from an external function..
static uint32_t kbdValue() { return kbdMask; }
#endif

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
    void microNb(uint32_t v, int x, int y, int nb)
    {
        //  XX    X   XX   XX    X  XXXX  XX  XXXX  XX   XX                                                            
        // X  X  XX  X  X X  X  X   X    X      X  X  X X  X                                                           
        // X  X   X    X    XX X X   XX  XXX   X    XX   XXX                                                           
        // X  X   X   X   X  X XXXX    X X  X X    X  X    X                                                           
        //  XX  XXXX XXXX  XX    X  XXX   XX  X     XX   XX                                                             
        static uint32_t const fnt[10]= {0b01101001100110010110, 0b00100110001000101111, 0b01101001001001001111, 0b01101001001110010110, 0b00100100101011110010, 
                                        0b11111000011000011110, 0b01101000111010010110, 0b11110010010010001000, 0b01101001011010010110, 0b01101001011100010110};
        x+= (nb-1)*5;
        while (--nb>=0)
        {
            uint32_t c= fnt[v%10]; v/= 10;
            for (int i=5; --i>=0; )
                for (int j=4; --j>=0; )
                {
                    if ((c&1)!=0) pixon(x+j, y+i);
                    c>>= 1;
                }
            x-= 5;
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

static uint32_t const sideralSpeeds[5]= {0, 23*3600UL+56*60+4, 24*3600UL+50*60, 24*3600UL, 1299188UL}; // King

// graphics that get dispalyed...
static uint8_t const star[] = { 17, 17, 0, 1, 0, 2, 144, 36, 65, 62, 1, 255, 129, 254, 11, 254, 15, 252, 31, 255, 255, 241, 127, 224, 255, 160, 255, 2, 255, 1, 249, 4, 73, 18, 128, 0, 0, 1, 0}; // picture of a star
static uint8_t const scopeE[] = { 5, 10, 16, 35, 195, 131, 48, 1, 3};
static uint8_t const scopeW[] = { 5, 10, 1, 11, 195, 3, 50, 16, 3};
static uint8_t const XImage[] = { 5, 10, 3, 51, 3, 3, 51, 3, 3};

// state variables...
enum { UIMain, UIMessier, UINgc, UIStars, UICadwell, UIFocus};
static uint8_t UI= 0; // UI will be one of the above and indicate the UI screen...
static int8_t findMessier= 0, findCadwell=0;    // last select messiers et cadwell
static uint8_t countToNextSelect; int8_t delayToNextSelect= 15; // to accelerate when press&hold keys in messier/ngc selection
static uint8_t returnToScreen1= 0;             // down counter to go back to screen 0 UI (ticks every screen refresh)
static int16_t lastkbd= 0;                     // status of last keyboard scann for debounce
static uint32_t spiralA= 0; static int32_t spiralra=0, spiraldec=0;             // spiralA is the spiral Angle with 1<<16 = 1 full turn (so 16 bit precision) the 2 others are the "center of the spiral"
static bool blockSync= false;                  // if true, do not execute sync key (used to avoid exit from star menu with sync leads to sync going on...)
static const uint32_t UIDelay= 50000;           // Pool time of keyboard. UI unit of time. 20 times per second...
static const uint16_t timeToScreenOffConst= 10*20;   // 10s timout on the screen. The 20 is the UI delay unit of time...
static uint16_t timeToScreenOff= timeToScreenOffConst; // countdown to screen off...
static uint8_t manualSpeed= 3;                 // current speed in manualSpeeds
static uint16_t const manualSpeeds[]= { 60, 60*15, 60*30, 60*60, 2*60*60, 4*60*60, 8*60*60, 12*60*60 }; // list of possible speed in manual mode in deg per second. WARNING, RA is in h, not deg units!
static const int8_t nbSpeeds= sizeof(manualSpeeds)/sizeof(manualSpeeds[0]);
static char const manualSpeedsTxt[]=" 1'15'30' 1\x7f 2\x7f 4\x7f 8\x7f""12\x7f"; // text for the above

static uint8_t toStr(char *s, uint32_t v) // convert number to dec representation... Returns number of characters...
{
    uint8_t nb= 0; while (true) { s[nb++]= '0'+(v%10); v/= 10; if (v==0) break; } s[nb]=0; // Write number, but backward...
    for (int8_t i=nb/2; --i>=0;) { char r= s[i]; s[i]= s[nb-1-i]; s[nb-1-i]= r; } // flip it!
    return nb;
}
// convert number [0..99] into number representation with leading 0.
static void toStr2(char *s, uint8_t v) { s[1]= '0'+(v%10); s[0]= '0'+(v/10); }

    uint8_t const column[] = { 6, 14, 60, 15, 207, 195, 243, 240, 60, 0, 0, 0, 0}; // Fat 2 dots....
    uint8_t const degree[] = { 6, 6, 0b11111111, 0b00111111, 0b11001111, 0b11111111, 0b1111};
    uint8_t const earth[] = { 8, 8, 60, 114, 161, 137, 157, 181, 126, 60};

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

// if go, or sync pressed, then execute as needed... Saves memory to put it there...
static void testGoSync(uint16_t newKeyDown, int32_t ra, int32_t dec)
{
    dispRaDec(ra, dec); 
    if (scope.altitudeOf(ra/3600.0, dec/3600.0)<scope.minAltitude)
        display.blit(XImage, 122, 0), newKeyDown&= ~keyGo;
    else if ((scope.destinationsideofpier(ra/3600.0, true)==0) != scope.scopeEast)
        display.blit(scope.scopeEast ? scopeW:scopeE, 122, 0);

    if ((newKeyDown&keyGo)!=0)  // validation button
    { 
        scope.slew(ra/3600.0, dec/3600.0, true);
        UI= UIMain;
    }
    if ((newKeyDown&keySync)!=0) 
    {
       scope.synctocoordinates(ra/3600.0, dec/3600.0);   // Sync telescope on object..
       UI= UIMain;
       blockSync= true;
    }

}

static bool lastpower= false; // true if power is on!


#include "catalogs.h"

// Main UI function!
void doUI()
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

    uint32_t keys= kbdValue(); uint32_t newKeyDown= keys&~lastkbd; uint32_t newKeyUp= lastkbd&~keys; lastkbd= keys;
    
    if (keys!=0) display.screenOn(), returnToScreen1= 20*10, timeToScreenOff= timeToScreenOffConst;   // if a key is pressed, we reset the returnToScreen1 timer...
    if (returnToScreen1==0) UI= UIMain; else returnToScreen1--; // at 0, reset UI. else tick down timer
	#ifndef PC // no screen auto off in PC mode...
	    if (timeToScreenOff==0) { display.screenOff(); goto done; }
	    timeToScreenOff--;
	#endif

    display.clear(); // No need as screen is erased as we go...

    if (scope.flipping) // Meridian swapping!
    {
        display.text2("Flip", 32, 0);
        dispRaDec(int32_t(scope.raActual*3600), int32_t(scope.decActual*3600));
        if ((newKeyDown&keyEsc)==0) goto done;
        scope.abortslew(); // esc key. stop where we are...
    }

    if ((newKeyDown&keyMenu)!=0) { if (UI>=UIFocus) UI= UIMain; else UI++; } // menu change
    if ((newKeyDown&keyEsc)!=0) UI= UIMain; // escape menu.

    if (UI==UIMain) // Main screen. move, speed, menu and spiral
    {
        display.text2("Ra", 0, 0); display.text2("Dec", 94, 0, 0); dispRaDec(int32_t(scope.raActual*3600), int32_t(scope.decActual*3600));
        display.rect(60, 22, 4, 4, false);
        // RA/DEC space between 54 et 60 (6pix, avec 12pix de -)
        // minutes to flip... since RA just goes down, it's really the time to get to pos=0
        uint32_t mins= uint32_t(motor1.getPos()*24*60ULL/savedData.ra.maxPos);
        uint32_t hours= mins/60; mins%= 60;
        display.microNb(hours, 54+(hours<10?5:0), 18, hours<10?1:2); display.microNb(mins, 54, 18+6, 2);
        uint8_t x= display.text("spd:   /s", (94+24-9*6-1-7)/2, 0); display.text(manualSpeedsTxt+manualSpeed*3,(94+24-9*6-1-7)/2+24,0, 1,3);
        #ifdef HASADC // only in this case do we know if there is power...
            if (!lastpower) display.hline((94+24-9*6-1-7)/2, x-(94+24-9*6-1-7)/2, 4);
        #endif
        display.blit(scope.scopeEast ? scopeE:scopeW, x+1, 0);
        static char const speeds[6][8]= {"Stopped", "Sideral", "Moon", "Sun", "King", "Unknow"};
        x= display.text(speeds[scope.tracking?scope.sideralRateType+1:0], 30, 8);
        #ifdef HASGPS
            if (CGPS::hasPosInfo) display.blit(earth, x, 8);
        #endif


        if ((newKeyDown&keyLeft)!=0)  scope.moveRa(manualSpeeds[manualSpeed]/3600.0);
        if ((newKeyDown&keyRight)!=0) scope.moveRa(-manualSpeeds[manualSpeed]/3600.0);
        if ((newKeyDown&keyUp)!=0)    scope.moveDec(manualSpeeds[manualSpeed]/3600.0);
        if ((newKeyDown&keyDown)!=0)  scope.moveDec(-manualSpeeds[manualSpeed]/3600.0);
        // if ((keys&keyUp)!=0 && MDec.pos==MDec.maxPos)
        // {
        //     flipRa(); flipDec();
        //     MDec.pos--;  // avoids, on next loop, a reflip the other way around!
        //     MDec.goDownRealNoAbs(manualSpeeds[manualSpeed]);
        // }
        if ((newKeyDown&keySpd)!=0)
        {
            do { manualSpeed= (manualSpeed+1)%nbSpeeds; } while (motor1.maxSpdInReal()<manualSpeeds[manualSpeed]/15.0/3600.0);
        }
      
        if ((keys&keyEsc)!=0 && (newKeyDown&keySync)!=0) scope.scopeEast= !scope.scopeEast; // press and hold Esc, then press sync to change the dec direction...
        if ((keys&keyEsc)!=0 && (newKeyDown&keyRight)!=0)  // press and hold Esc, then press right to stop the sideral move
        {
            if (!scope.tracking) { scope.setSideralRate(0); scope.setTracking(true); }
            else { if (scope.sideralRateType==2) scope.setTracking(false); else scope.setSideralRate(scope.sideralRateType+1); }
        }
        if ((keys&keyEsc)!=0 && (newKeyDown&keyGo)!=0) display.begin(); // reset the display (in case it fucks up)
        if ((keys&keyEsc)!=0 && (newKeyDown&keyLeft)!=0) reboot(); // reboot the cpu...

        /*
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
              // int32_t spiralR= ((spiralA>>8)*(1<<manualSpeed)*60)>>8; // in arc". the >>16 is because spiralA is in turn<<16
              int32_t spiralR= spiralA>>(10-manualSpeed); // This line is equivalent to the line above, with the 60 replaced by a 64 (the 60 being the arc" separatinon from circle to circle). But this saves 24 bytes!
              #ifdef PC
                #define fps 6
                int const fpsFactor= int(2*3.14f*fps);
              #else // for AVR and ESP, they are both tunned at 30 fps or so
                #define fps 32
                #define fpsFactor 207
              #endif
              spiralA+= (uint32_t(manualSpeeds[manualSpeed])<<16)/(fpsFactor*spiralR); // increment on angle to get constant speed... This could be partially precomputed, but the division would still be there :-(
              int32_t nra= (spiralR*(Planets::cos(spiralA)>>8))>>16, ndec= (spiralR*(Planets::sin(spiralA)>>8))>>16; // next x/y of spiral
              nra= MRa.stepsFromRealNoAbs(nra/16)+spiralra, ndec= MDec.stepsFromRealNoAbs(ndec)+spiraldec; // convert to steps. Note that ra is in 24 units, not 360! so the /16 (which is close enough to the nominal 15)
              MRa.goUp((nra-MRa.pos)*fps), MDec.goUp((ndec-MDec.pos)*fps); // go in the direction that is needed... Note that we probably should add some ra offset for lost stellar movement!
            }
        } else { blockSync= false; spiralra= MRa.pos; spiraldec= MDec.pos; spiralA= 1U<<15; } // if sync key not pressed. Save current position and init angle at 1/2 turn (could be 1/4), can not be 0 as it would cause a /0 error.
        */

        // if move was started from keyboard and no keys. stop. If Esc, stop...
        if ((newKeyUp&(keyUp|keyDown|keyRight|keyLeft|keySync))!=0 || (keys&keyEsc)!=0) scope.abortslew();
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
    }

    if (UI==UIFocus) // focusser
    { 
        if (savedData.focMaxSpd!=0) 
        {
            display.text2("Focusser", 0, 0);
            int32_t v= int32_t(motorFocus.posInReal());
            memcpy(t, "+00.000mm", 10); if (v<0) t[0]='-', v= -v;
            ldiv_t d= ldiv(v, 100); toStr2(t+5, uint8_t(d.rem));
            d= ldiv(d.quot, 10); t[4]='0'+uint8_t(d.rem); toStr2(t+1, uint8_t(d.quot));
            display.text2(t, 0, 16);
            if ((newKeyDown&keyDown)!=0)       motorFocus.goAtSpeed(-savedData.focMaxSpd/40);
            else if ((newKeyDown&keyUp)!=0)    motorFocus.goAtSpeed(savedData.focMaxSpd/40);
            else if ((newKeyDown&keyLeft)!=0)  motorFocus.goAtSpeed(-savedData.focMaxSpd);
            else if ((newKeyDown&keyRight)!=0) motorFocus.goAtSpeed(savedData.focMaxSpd);
            if ((keys&(keyUp|keyDown|keyRight|keyLeft))==0) motorFocus.stop(); 
            returnToScreen1= 1; // This is a "never timeout" feature here
        }
    }

    done: display.disp();
}

void beginUI() { kbdStart(); display.begin(); }
#endif