// LET US BE VERY CLEAR!!!!
// THIS CODE IS CRAP! It's only purpose is to allow testing of the main code on windows...
// Note that it will open a tcp socket that the ascom driver can connect to!
// To do so, "double click" in the ascom driver on the "com" name (label) and it will attempt to connect to here...
// This makes ascom testing quite convinient...
// Appart from this, the code here (which #includes the .ino code) is full of test code, and other atrocities that where usefull at some point
// and left in because you never know!


#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <stdlib.h>
#include <Windows.h>
#include <fstream>
#include <string>
#include <iterator>
#include "MiniFB.h"
#include <atlbase.h>
#include <math.h>
#include <stdio.h>
#pragma comment(lib, "ws2_32.lib")


using namespace std;
void arobas();




namespace MSerial { // my light weight implementation of serial. 38400 baud
    SOCKET clientSocket= INVALID_SOCKET;
    char t[1000]; int pos= 0;
    CRITICAL_SECTION cs;
    static DWORD WINAPI sockListen(LPVOID lpThreadParameter)
    {
        WSADATA wsaData;
        SOCKET serverSocket;
        struct sockaddr_in serverAddr, clientAddr;
        int addrSize = sizeof(clientAddr);

        WSAStartup(MAKEWORD(2, 2), &wsaData);
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
        serverAddr.sin_port = htons(8080);        // Port 8080
        bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
        listen(serverSocket, SOMAXCONN);
        while (1) 
        {
            clientSocket = accept(serverSocket, (SOCKADDR*)&clientAddr, &addrSize);
            if (clientSocket == INVALID_SOCKET) continue;
            printf("Client connected.\n");
            char buffer[1024];
            while (true) 
            {
                int bytesReceived = recv(clientSocket, buffer, sizeof(t)-pos, 0);
                if (bytesReceived <= 0) break; 
                buffer[bytesReceived]= 0;
                 printf("Received: %s\n", buffer);
                EnterCriticalSection(&(cs));
                memcpy(t+pos, buffer, bytesReceived), pos+= bytesReceived;
                LeaveCriticalSection(&(cs));
            }
            closesocket(clientSocket); clientSocket= INVALID_SOCKET;
        }
    }
    void sockSend(char const *s, int len) { if (clientSocket!=INVALID_SOCKET) send(clientSocket, s, len, 0); }

    void startSock() { CreateThread(NULL, 0, sockListen, nullptr, 0, NULL); }

    void print(char const *s) { sockSend(s,strlen(s)); 
     while (*s!=0) { if (*s=='\n') printf("\r\n"); else printf("%c", *s); *s++; } 
    }
    void print(char s) { sockSend(&s,1); 
     if (s=='\n') printf("\r\n"); else printf("%c", s); 
    }
    void flush(char s) { print(s); }
    void flush(char const *s) { print(s); }
    void print(int16_t v) { printf("%d", v); }
    void print(int8_t v) { printf("%d", v); }
    void print(uint16_t v) { printf("%d", v); }
    //void print(int32_t v) { printf("%d", v&0xffff); }
    void write(char const *s, int len) { sockSend(s,len); char t[1000]; memcpy(t, s, len); t[len]= 0; printf("%s", t); }
    void write(char s) { print(s); }
    int16_t read()
    {
        if (pos==0) return -1;
        EnterCriticalSection(&cs);
        char r= t[0];
        memcpy(t, t+1, pos); pos--;
        LeaveCriticalSection(&cs);
        return r;
    }
    static DWORD WINAPI readConsole(LPVOID lpThreadParameter)
    {
        while (true) { 
            char c= getc(stdin); 
            EnterCriticalSection(&(cs));
            t[pos]= c; pos+= 1;
            LeaveCriticalSection(&(cs));
        }
    }
    void begin() 
    { 
        InitializeCriticalSection(&cs); 
        startSock();
        CreateThread(nullptr, 0, readConsole, nullptr, 0, nullptr);

        //uint8_t a[13]= {0x3a, 0x53, 0x64, 0x2d, 0x31, 0x31, 0xdf, 0x30, 0x39, 0x3a, 0x33, 0x34, 0x23}; for (int i=0; i<13; i++) t[pos]= a[pos++];
        //uint8_t a[]= {0x23, 0x3a, 0x47, 0x44, 0x23}; for (int i=0; i<sizeof(a); i++) t[pos]= a[pos++];
        //char a[]= ":Sr2:31:00#:Sd-89:15:00#:MS#"; for (int i=0; i<sizeof(a)-1; i++) t[pos]= a[pos++];
    }
};

#define memcpy_P memcpy
#define PROGMEM
uint8_t PORTB=0, PORTC=0, PORTD=0, DDRB=0, DDRC=0, DDRD=0, PINC=0;
uint16_t keys= 0;
static uint16_t kbdValue() { return keys; }
static void inline portSetup() {} // set 3 kbd pins to pullup, one kbd pin to out and leave I2C alone...
#define PORTDSET(p) (PORTD|=p)
#define PORTDCLEAR(p) (PORTD&=~(p))
static void inline portBWritePin(int8_t pin, int8_t v) { if (v) PORTB|= 1<<pin; else PORTB&=~(1<<pin);}
static void inline portCWritePin(int8_t pin, int8_t v) { if (v) PORTC|= 1<<pin; else PORTC&=~(1<<pin); }
#define PC
uint64_t freq, start;
uint32_t micros()
{
    uint64_t now; QueryPerformanceCounter((LARGE_INTEGER*)&now); now= (now-start)*1000000/freq;
    return uint32_t(now);
}
uint32_t millis() { return micros()/1000; }
namespace Time {
    void begin() {}
    uint32_t unow() { return micros(); } // now in microseconds. But with 4 microsecond precision only...
    uint32_t mnow() { return millis(); }// now in milliseconds.
}
class CI2C { public:
    uint8_t *d= nullptr;
    int nb= 0;
    uint32_t msToDone= 0;
    bool next() 
    {
        if (d==nullptr) return false;          // nothing to do...
        if (msToDone>millis()) return true; // busy
        memset(d, 0, nb); d= nullptr; return false;
    }
    void begin() { }
    void send(uint8_t *i, int16_t nb) 
    { 
        if (d!=nullptr) return;
        d= i; this->nb= nb;
        msToDone= millis()+50;
    }
} I2C;
#define pgm_read_dword(a) ((*(a))&0xffffffff)
#define pgm_read_word(a)  ((*(uint16_t*)(a))&0xffff)
#define pgm_read_byte(a)  ((*(uint32_t*)(a))&0xff)
#define debug(...) ATLTRACE2(__VA_ARGS__)
void udelay(uint16_t) {}
void noInterrupts(){} // Time sensitive
void PORTBCLEAR(int){}
void PORTBSET(int){}
void PORTBPULLUP(int){}
void PORTBOUT(int){}
int PORTBGET(){ return 0; }
void interrupts(){}
static int const maxPulsesPerSecond= 9200;
uint32_t portSET_INTERRUPT_MASK_FROM_ISR() { return 0; }
void portCLEAR_INTERRUPT_MASK_FROM_ISR(uint32_t ) { }
void DoNotOptimize(uint8_t const &) { }
static const int8_t raDirPin    = 2; 
static const int8_t decDirPin   = 3; 
static const int8_t focDirPin   = 4; 
static const int8_t raStepPin   = 5; 
static const int8_t decStepPin  = 6; 
static const int8_t focStepPin  = 7; 
static const int8_t motorSerialRa = 8;
static const int8_t motorSerialDec = 9;
static const int8_t motorSerialFocus = 10;
static float const M_PI= 3.14159265358979323846264338327f;
static const int8_t raUartAddr= 3; 
static const int8_t decUartAddr= 1;
static const int8_t focUartAddr= 2; 
void esp_restart() {}
void xTaskCreate(void (*cb)(void*), char const*, int, void *p, int, void*)
{
    CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)cb, p, 0, nullptr);
}
#define reboot()
int gpspos= 0;
int gpsGetData(char *b, int size) 
{ 
    static char const gr[]=
        "$GNGGA,102758.000,4544.96195,N,00450.25286,E,1,08,1.2,147.6,M,50.2,M,,*4A\n"
        "$GNZDA,111557.000,04,11,2025,00,00*4F\n";
    int l= strlen(gr);
    if (size+gpspos>l) size= l-gpspos;
    memcpy(b, gr+gpspos, l); gpspos+= size; if (gpspos>=l) gpspos= 0;
    return l;

}
void gpsDone() { ExitThread(0); }
bool gpsBegin() { return true; }

#include "../eqControl_Ino/eqControl_Ino.ino"

class CMyWin : public CFBWindow { public:
    CMyWin(): CFBWindow(L"CB focusser", 800, 600) { }

    void resize(int width, int height)  { }

    void keyboard(int key, uint32_t mod, bool isPressed)
    {
    }
    int mouseX, mouseY;
    void mouse_move(int x, int y) { mouseX= x, mouseY= y; }
    void mouse_button(int button, uint32_t mod, bool isPressed)
    {
        uint16_t const ktable[9]={
            keyMenu, keySpd,keyEsc,
            keyGo,keyUp,keySync,
            keyLeft,keyDown,keyRight
        };
        if (button!=1) return;
        int b= -1;
        if (isPressed) { for (int i=0; i<nbBtn; i++)  if (buttonPos(i).contains(mouseX, mouseY)) { b= i; break; } }
        if (b==-1) keys= 0; else keys= ktable[b];
    }

    TRect buttonPos(int i) { return TRect((i%3)*38+4, (i/3)*38+54*2, 30, 30); }
    uint32_t buttonCol(int i) { uint32_t const c[3]= {ClRed, ClGreen, ClBlue}; return c[i/3]; }
    char const * buttonTxt(int i) { char const * const t[9]= {"Menu", "Spd", "Esc", "Go", "Dec+", "Sync", "RA-", "Dec-", "RA+"}; return t[i]; }
    static const int nbBtn= 9;
    static char *toHStr(int32_t v, char *s)
    {
        char *S= s;
        if (v<0) *s++= '-', v= -v;
        _itoa(v/3600, s, 10); while (*s!=0) s++; *s++='\'';
        _itoa((v/60)%60, s, 10); while (*s!=0) s++; *s++='\"';
        _itoa(v%60, s, 10); 
        return S;
    }

    void exec()
    {
        int lastFrame= frameCount;
        rect(0, 0, w, h, ClWhite);
        rect(0, 0, 128*2+8, 32*2+8, ClBlack);
        rect(2, 2, 128*2+4, 32*2+4, ClWhite);
        for (int i=0; i<nbBtn; i++) { TRect r= buttonPos(i); rect(r, buttonCol(i)); centertext(r.x+r.w/2, r.y+8, buttonTxt(i)); }
        char s1[20], s2[20], s3[20];
        while (!closed)
        {
            loop();
            if (lastFrame==frameCount) continue;
            lastFrame=frameCount;

            for (int y=0; y<32; y++)
                for (int x=0; x<128; x++)
                    rect(x*2+4, y*2+4, 2, 2, display.pixel(x,y)? ClBlack : ClWhite);
            text(310, 0*3*8, 500, print("RA  pos:%d %s %s %s", MRa.pos, toHStr(MRa.minPosReal, s1), toHStr(MRaposInReal(), s2), toHStr(MRa.maxPosReal, s3)), ClBlack, ClWhite, 2);
            text(310, 1*3*8, 500, print("RA  spd:%d dst:%d respd:%d", MRa.currentSpd, MRa.dst, MRa.requestedSpd), ClBlack, ClWhite, 2);
            text(310, 2*3*8, 500, print("RA ustp:%d", MRa.unstep), ClBlack, ClWhite, 3);
            text(310, 3*3*8, 500, print("RA rspd:%d", MRa.requestedSpd), ClBlack, ClWhite, 3);
            text(310, 4*3*8, 500, print("Dec pos:%d %s %s %s", MDec.pos, toHStr(MDec.minPosReal, s1), toHStr(MDec.posInReal(), s2), toHStr(MDec.maxPosReal, s3)), ClBlack, ClWhite, 2);
            text(310, 5*3*8, 500, print("Dec Invert:%s", MDec.invertDir?"True":"False"), ClBlack, ClWhite, 3);
            text(310, 6*3*8, 500, print("Decsdst:%d", MDec.dst), ClBlack, ClWhite, 3);
            text(310, 7*3*8, 500, print("Dec spd:%d", MDec.currentSpd), ClBlack, ClWhite, 3);
            text(310, 8*3*8, 500, print("spiralI:%d", spiralI), ClBlack, ClWhite, 3);
            text(310, 9*3*8, 500, print("SpiralDD:%d", spiralDD), ClBlack, ClWhite, 3);
            text(310,10*3*8, 500, print("SpiralDR:%d", spiralDR), ClBlack, ClWhite, 3);
            text(310,11*3*8, 500, print("keys:%x", keys), ClBlack, ClWhite, 3);
            text(310,12*3*8, 500, print("usteps:%d %d", MRa.countAllUncountedSteps/2, MRa.countAllUncountedSteps/2*1000/millis()), ClBlack, ClWhite, 3);
            text(310,13*3*8, 500, print("RA m:0 pos:%d M:%d", MRa.pos, MRa.maxPos), ClBlack, ClWhite,2);
            text(310,14*3*8, 500, print("RA mr:%d pos:%d Mr:%d", MRa.minPosReal, MRaposInReal(), MRa.maxPosReal), ClBlack, ClWhite,2);
            text(310,15*3*8, 500, print("PORTB:%d", PORTB), ClBlack, ClWhite,2);
        }
    }
};

int main()
{
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq); QueryPerformanceCounter((LARGE_INTEGER*)&start); 
    setup();

    // MRa.pos+= MRa.maxPos/4;
    // MRa.dst= MRa.pos;
    // int d= (MRa.maxPosReal-MRa.minPosReal)/4;
    //MRa.maxPosReal+= 43000*2; 
    //MRa.minPosReal+= 43000*2; 
    //enableFlip(false);

    CMyWin fb; fb.setFps(20); fb.run();
}

struct { char const *n; double ra, dec; } stars2[]={
    { "Acamar",2.971023,-40.304672},
    { "Achernar",1.628556,-57.236757},
    { "Acrux",12.443311,-63.099092},
    { "Adhara",6.977097,-28.972084},
    { "Albaldah",19.162731,-21.023615},
    { "Albireo",19.512023,27.959681},
    { "Alcor",13.420413,54.987958},
    { "Alcyone",3.79141,24.105137},
    { "Aldebaran",4.598677,16.509301},
    { "Alderamin",21.30963,62.585573},
    { "Algenib",0.220598,15.183596},
    { "Algieba",10.332873,19.841489},
    { "Algol",3.136148,40.955648},
    { "Algorab",12.497739,-16.515432},
    { "Alhena",6.628528,16.399252},
    { "Alioth",12.900472,55.959821},
    { "Alkaid",13.792354,49.313265},
    { "Almaak",2.064984,42.329725},
    { "Alnair",22.137209,-46.960975},
    { "Alnath",5.438198,28.60745},
    { "Alnilam",5.603559,-1.20192},
    { "Alnitak",5.679313,-1.942572},
    { "Alphard",9.45979,-8.658603},
    { "Alphekka",15.578128,26.714693},
    { "Alpheratz",0.139791,29.090432},
    { "Alshain",19.921887,6.406763},
    { "Altair",19.846388,8.868322},
    { "Aludra",7.401584,-29.303104},
    { "Ankaa",0.438056,-42.305981},
    { "Antares",16.490128,-26.432002},
    { "Arcturus",14.26103,19.18241},
    { "Arneb",5.545504,-17.822289},
    { "Atria",16.811077,-69.027715},
    { "Avior",8.375236,-59.509483},
    { "Babcock's star",22.735418,55.589226},
    { "Barnard's Star",17.963472,4.693388},
    { "Becrux",12.795359,-59.688764},
    { "Bellatrix",5.418851,6.349702},
    { "Betelgeuse",5.919529,7.407063},
    { "Canopus",6.399195,-52.69566},
    { "Capella",5.27815,45.997991},
    { "Caph",0.152887,59.14978},
    { "Castor",7.576634,31.888276},
    { "Cebalrai",17.724543,4.567303},
    { "Cih",0.945143,60.71674},
    { "Cor Caroli",12.933807,38.31838},
    { "Cursa",5.130829,-5.086446},
    { "Cygnus X-1",19.972688,35.201604},
    { "Deneb",20.690532,45.280338},
    { "Denebola",11.817663,14.57206},
    { "Diphda",0.72649,-17.986605},
    { "Dschubba",16.005557,-22.62171},
    { "Dubhe",11.062155,61.751033},
    { "Enif",21.736433,9.875011},
    { "Etamin",17.943437,51.488895},
    { "Fomalhaut",22.960838,-29.622236},
    { "Gacrux",12.519429,-57.113212},
    { "Gienah",20.770178,33.970256},
    { "Gienah Ghurab",12.263437,-17.541929},
    { "Gomeisa",7.452512,8.289315},
    { "Graffias",16.09062,-19.805453},
    { "Groombridge 1830",11.88282,37.718679},
    { "Hadar",14.063729,-60.373039},
    { "Hamal",2.119555,23.462423},
    { "Hassaleh",4.949894,33.16609},
    { "Hatsya",5.590551,-5.909901},
    { "Izar",14.749784,27.074222},
    { "Kapteyn's Star",5.194169,-45.018417},
    { "Kaus Australis",18.402868,-34.384616},
    { "Kaus Borealis",18.466179,-25.4217},
    { "Kaus Meridionalis",18.3499,-29.828103},
    { "Kochab",14.845105,74.155505},
    { "Kornephoros",16.503668,21.489613},
    { "Kraz",12.573121,-23.396759},
    { "Markab",23.079348,15.205264},
    { "Matar",22.716704,30.221245},
    { "Megrez",12.257086,57.032617},
    { "Menkalinan",5.992149,44.947433},
    { "Menkar",3.037992,4.089734},
    { "Menkent",14.111395,-36.369954},
    { "Merak",11.030677,56.382427},
    { "Miaplacidus",9.220041,-69.717208},
    { "Mintaka",5.533445,-0.299092},
    { "Mira",2.322442,-2.977643},
    { "Mirach",1.162194,35.620558},
    { "Mirphak",3.405378,49.86118},
    { "Mirzam",6.378329,-17.955918},
    { "Mizar",13.398747,54.925362},
    { "Mufrid",13.911411,18.397717},
    { "Naos",8.059737,-40.003148},
    { "Nash",18.096803,-30.424091},
    { "Nihal",5.470756,-20.759441},
    { "Nunki",18.92109,-26.296722},
    { "p Eridani",1.663003,-56.19462},
    { "Peacock",20.427459,-56.73509},
    { "Phad",11.897168,53.69476},
    { "Phakt",5.660817,-34.074108},
    { "Polaris",2.52975,89.264109},
    { "Pollux",7.755277,28.026199},
    { "Porrima",12.694345,-1.449375},
    { "Procyon",7.655033,5.224993},
    { "Ras El. Australis",9.764188,23.774255},
    { "Rasalgethi",17.244127,14.390333},
    { "Rasalhague",17.582241,12.560035},
    { "Rastaban",17.507213,52.301387},
    { "Red Rectangle",6.332838,-10.637414},
    { "Regulus",10.139532,11.967207},
    { "Rigel",5.242298,-8.20164},
    { "Rigil Kentaurus",14.660765,-60.833976},
    { "Ruchbah",1.430216,60.235283},
    { "Sadalmelik",22.096399,-0.319851},
    { "Sadalsuud",21.525982,-5.571172},
    { "Sadr",20.370473,40.256679},
    { "Saiph",5.795941,-9.669605},
    { "Sargas",17.62198,-42.997824},
    { "Scheat",23.062901,28.082789},
    { "Shaula",17.560145,-37.103821},
    { "Shedir",0.675116,56.537331},
    { "Sheliak",18.834665,33.362667},
    { "Sheratan",1.910668,20.808035},
    { "Sirius",6.752481,-16.716116},
    { "Spica",13.419883,-11.161322},
    { "Tarazed",19.770994,10.613261},
    { "Thuban",14.073165,64.37585},
    { "Tureis",9.284838,-59.275229},
    { "Unukalhai",15.737798,6.425627},
    { "Vega",18.61564,38.783692},
    { "Vindemiatrix",13.036278,10.95915},
    { "Wezen",7.139857,-26.3932},
    { "Zaurak",3.967157,-13.508515},
    { "Zosma",11.235138,20.523717},
    { "Zubenelgenubi",14.847977,-16.041778},
    { "Zubeneschemali",15.283449,-9.382917}};

void createStarsStruct()
{
    // 24 bits radec(12,12)
    // 4 bit name size in bytes (+1?)
    // name in 6 bit per char format. ' '=0, A=1...
    int size= 0, lastSize= 0;
    for (int i=0; i<sizeof(stars2)/sizeof(stars2[0]); i++)
    {
        size+= 3;
        int32_t ra= int32_t(stars2[i].ra*3600), dec= int32_t(stars2[i].dec*3600);
        uint32_t v= (ra*4096/(24*3600)) + uint32_t(((int64_t(dec+90*3600)*4096/(180*3600))<<12));
        printf("%d, %d, %d,  ", v&0xff, (v>>8)&0xff, (v>>16)&0xff);
        int s= int(strlen(stars2[i].n));
        if (s-3>=16 || s<3) { printf("Error with name size :%s\r\n", stars2[i].n); if (s>=19) s=18; }
        printf("0x%02x,",(lastSize<<4)+(s-3)); lastSize= s-3; size++;
        for (int j=0; j<s; j++) { if (stars2[i].n[j]=='\'') printf("'\\'',"); else printf("'%c',",stars2[i].n[j]); size++; }
        printf(" //%s\r\n", stars2[i].n);
    }
    printf("size %d (%lld stars)\r\n", size, sizeof(stars2)/sizeof(stars2[0]));
}

static char const img[]=
"        *        "\
"        *        "\
"  *  *  *  *  *  "\
"   *  *****  *   "\
"    *********    "\
"  * ********* *  "\
"   ***********   "\
"   ***********   "\
"*****************"\
"   ***********   "\
"   ***********   "\
"  * ********* *  "\
"    *********    "\
"   *  *****  *   "\
"  *  *  *  *  *  "\
"        *        "\
"        *        ";
// collumn 6*14
//"      "\
//"      "\
//"****  "\
//"****  "\
//"****  "\
//"****  "\
//"      "\
//"      "\
//"****  "\
//"****  "\
//"****  "\
//"****  "\
//"      "\
//"      ";
static char const img2[]=
"*   *"\
"*   *"\
" * * "\
" * * "\
"  *  "\
"  *  "\
" * * "\
" * * "\
"*   *"\
"*   *"\
;

void displayImageStruct()
{
    char const *p= img2; int w= 5, h=10;
    printf("uint8_t const [] PROGMEM = { %d, %d",w, h);
    uint8_t b= 0, bs= 0;
    for (int x=0; x<w; x++)
        for (int y=0; y<h; y++)
        {
            if (p[x+y*w]!=' ') b|= 1<<bs;
            if (++bs==8) { printf(", %d", b); b= 0; bs= 0; }
        }
    if (bs!=0) printf(", %d", b);
    printf("};\r\n");
}

void DipStarsFromStars()
{
    while (stars.id<stars.nb-1)
    {
        int32_t ra, dec; char t[30];
        stars.pos(ra, dec); stars.name(t); printf("%s ra:%d:%d dec:%d:%d\r\n", t, ra/3600, (ra/60)%60, dec/3600, (abs(dec)/60)%60);
        stars.next();
    }
    stars.s= stars.stars; stars.id= 0;
}

void verifyStars() // Verify a match between our data structures and the original... (within 10 minutes)
{
    while (stars.id<stars.nb-1)
    {
        int32_t sra, sdec; char t[30]; stars.pos(sra, sdec); stars.name(t);
        int32_t ra= int32_t(stars2[stars.id].ra*60), dec= int32_t(stars2[stars.id].dec*60);
        sra/=60, sdec/=60; 
        if (abs(sra-ra)>10 || abs(sdec-dec)>10)
            printf("%s should be: %d:%d %d:%d  is %d:%d %d:%d\r\n", t, ra/60, ra%60, dec/60, dec%60, sra/60, sra%60, sdec/60, sdec%60);
        stars.next();
    }
    stars.s= stars.stars; stars.id= 0;
}

struct NGC { 
    uint16_t id() const { return pgm_read_byte(&id0_7)+((uint8_t(pgm_read_byte(&v3_id8_12)&0xf8))<<5); }
    void pos(int32_t &ra, int32_t &dec) const
    {
        uint32_t v= pgm_read_byte(&v0)|(uint16_t(pgm_read_byte(&v1))<<8)|(uint32_t(pgm_read_byte(&v2))<<16)|(uint32_t(pgm_read_byte(&v3_id8_12)&0x7)<<24);
        ra= 8*(v%10800UL);
        dec= ((v/10800UL)-90*60)*60;
    }
    uint8_t id0_7, v0, v1, v2, v3_id8_12;
};
#define rdc(ra,dec) ((ra+4)/8 + ((dec+90*3600ULL+30)/60)*10800ULL )
#define NgC(id, ra, dec) { id&0xff, rdc(ra,dec)&0xff, (rdc(ra,dec)>>8)&0xff, (rdc(ra,dec)>>16)&0xff, ((rdc(ra,dec)>>24)&0x7) | ((id&0x1f00)>>5) }

NGC const NGCs[] PROGMEM = {    
#define r(h,m,s) (((h*60)+m)*60+s)
    NgC(  40, r(0,13,01), r(72,31,19)),
    NgC(45, 846, -83460),
    NgC(55, 894, -141060),
    NgC(103, 1518, 220860),
    NgC(104, 1446, -259500),
    NgC(121, 1608, -257520),
    NgC(129, 1794, 216840),
    NgC(133, 1872, 228120),
    NgC(134, 1824, -119700),
    NgC(146, 1986, 227880),
    NgC(147, 1992, 174600),
    NgC(157, 2088, -30240),
    NgC(185, 2340, 174000),
    NgC(188, 2640, 307200),
    NgC(189, 2376, 219840),
    NgC(205, 2424, 150060),
    NgC(210, 2436, -49920),
    NgC(221, 2562, 147120),
    NgC(224, 2562, 148560),
    NgC(225, 2604, 222420),
    NgC(246, 2820, -42780),
    NgC(247, 2826, -74760),
    NgC(253, 2856, -91020),
    NgC(278, 3126, 171180),
    NgC(281, 3168, 203820),
    NgC(288, 3168, -95700),
    NgC(300, 3294, -135660),
    NgC(330, 3372, -260940),
    NgC(346, 3546, -259860),
    NgC(362, 3792, -255060),
    NgC(381, 4098, 221700),
    NgC(404, 4164, 128580),
    NgC(419, 4098, -262380),
    NgC(436, 4536, 211740),
    NgC(457, 4746, 210000),
    NgC(488, 4908, 18900),
    NgC(524, 5088, 34320),
    NgC(559, 5370, 227880),
    NgC(578, 5430, -81600),
    NgC(581, 5592, 218520),
    NgC(584, 5478, -24720),
    NgC(596, 5574, -25320),
    NgC(598, 5634, 110340),
    NgC(613, 5658, -105900),
    NgC(628, 5802, 56820),
    NgC(637, 6174, 230400),
    NgC(650, 6144, 185671), //M76
    NgC(654, 6246, 222780),
    NgC(659, 6252, 218520),
    NgC(660, 6180, 49080),
    NgC(663, 6360, 220500),
    NgC(672, 6474, 98760),
    NgC(720, 6780, -49440),
    NgC(744, 7104, 199740),
    NgC(752, 7068, 135660),
    NgC(771, 7404, 260700),
    NgC(772, 7158, 68460),
    NgC(821, 7704, 39600),
    NgC(869, 8340, 205740),
    NgC(884, 8544, 205620),
    NgC(891, 8556, 152460),
    NgC(908, 8586, -76440),
    NgC(925, 8838, 120900),
    NgC(936, 8856, -4140),
    NgC(956, 9144, 160740),
    NgC(957, 9216, 207120),
    NgC(1023, 9624, 140640),
    NgC(1027, 9762, 221580),
    NgC(1039, 9720, 154020),
    NgC(1042, 9624, -30360),
    NgC(1052, 9666, -29700),
    NgC(1055, 9708, 1560),
    NgC(1068, 9762, -60),
    NgC(1084, 9960, -27300),
    NgC(1097, 9978, -109020),
    NgC(1201, 11046, -93840),
    NgC(1232, 11388, -74100),
    NgC(1245, 11682, 170100),
    NgC(1261, 11538, -198780),
    NgC(1275, r(3,19,48), r(14,30,42)),
    NgC(1291, 11838, -148080),
    NgC(1300, 11982, -69900),
    NgC(1313, 11898, -239400),
    NgC(1316, 12162, -133920),
    NgC(1326, 12234, -131280),
    NgC(1332, 12378, -76800),
    NgC(1342, 12696, 134400),
    NgC(1344, 12498, -111840),
    NgC(1350, 12666, -121080),
    NgC(1365, 12816, -130080),
    NgC(1398, 13134, -94800),
    NgC(1399, 13110, -127620),
    NgC(1404, 13134, -128100),
    NgC(1407, 13212, -66900),
    NgC(1432, 13644, 86820), //M45
    NgC(1433, 13320, -169980),
    NgC(1444, 13764, 189600),
    NgC(1496, 14664, 189420),
    NgC(1502, 14862, 224400),
    NgC(1512, 14634, -156060),
    NgC(1513, 15000, 178260),
    NgC(1514, 14952, 110820),
    NgC(1528, 15324, 184440),
    NgC(1533, 14994, -202020),
    NgC(1535, 15252, -45840),
    NgC(1543, 15168, -207840),
    NgC(1545, 15654, 180900),
    NgC(1549, 15342, -200160),
    NgC(1553, 15372, -200820),
    NgC(1559, 15456, -226020),
    NgC(1566, 15600, -197760),
    NgC(1574, 15720, -205080),
    NgC(1582, 16320, 157860),
    NgC(1605, 16500, 162900),
    NgC(1617, 16302, -196560),
    NgC(1624, 16824, 181620),
    NgC(1637, 16890, -10260),
    NgC(1647, 17160, 68640),
    NgC(1662, 17310, 39360),
    NgC(1664, 17466, 157320),
    NgC(1711, 17430, -252000),
    NgC(1724, 18210, 178200),
    NgC(1746, 18216, 85740),
    NgC(1755, 17700, -245460),
    NgC(1770, 17820, -246300),
    NgC(1774, 17868, -242100),
    NgC(1778, 18486, 133380),
    NgC(1786, 17946, -243900),
    NgC(1792, 18312, -136740),
    NgC(1805, 18132, -237960),
    NgC(1807, 18642, 59520),
    NgC(1808, 18462, -135060),
    NgC(1814, 18228, -242220),
    NgC(1816, 18228, -242220),
    NgC(1817, 18726, 60120),
    NgC(1818, 18252, -239040),
    NgC(1820, 18228, -242220),
    NgC(1829, 18282, -244980),
    NgC(1835, 18312, -249840),
    NgC(1850, 18510, -247560),
    NgC(1851, 18846, -144180),
    NgC(1854, 18546, -247860),
    NgC(1856, 18564, -248880),
    NgC(1857, 19212, 141660),
    NgC(1866, 18810, -235680),
    NgC(1893, 19362, 120240),
    NgC(1904, 19470, -88380),
    NgC(1907, 19680, 127140),
    NgC(1912, 19722, 129000),
    NgC(1922, 19722, 129078), //M38
    NgC(1947, 19608, -229560),
    NgC(1951, 19554, -239760),
    NgC(1952, 20070, 79260),
    NgC(1955, 19566, -242940),
    NgC(1960, 20166, 122880),
    NgC(1962, 19590, -247620),
    NgC(1964, 20004, -79020),
    NgC(1965, 19590, -247620),
    NgC(1968, 19632, -242760),
    NgC(1970, 19590, -247620),
    NgC(1974, 19674, -242640),
    NgC(1976, 20124, -19620),
    NgC(1978, 19716, -238440),
    NgC(1981, 20112, -15960),
    NgC(1982, 20136, -18960),
    NgC(1983, 19650, -248220),
    NgC(2004, 19836, -242220),
    NgC(2011, 19926, -243060),
    NgC(2014, 19932, -243600),
    NgC(2041, 20184, -241080),
    NgC(2068, 20802, 180),
    NgC(2070, 20316, -248700),
    NgC(2099, 21144, 117180),
    NgC(2100, 20520, -249240),
    NgC(2112, 21234, 1440),
    NgC(2126, 21780, 179640),
    NgC(2129, 21660, 83880),
    NgC(2136, 21168, -250200),
    NgC(2141, 21786, 37560),
    NgC(2146, 22722, 282060),
    NgC(2157, 21438, -249060),
    NgC(2158, 22050, 86760),
    NgC(2164, 21522, -246660),
    NgC(2168, 22134, 87600),
    NgC(2169, 22104, 50220),
    NgC(2175, 22188, 73140),
    NgC(2186, 22332, 19620),
    NgC(2194, 22428, 46080),
    NgC(2204, 22542, -67140),
    NgC(2207, 22584, -76920),
    NgC(2210, 22290, -248880),
    NgC(2215, 22860, -26220),
    NgC(2217, 22902, -98040),
    NgC(2232, 23196, -17100),
    NgC(2236, 23382, 24600),
    NgC(2237, r(6,33,45),r(4,59,54)),
    NgC(2243, 23388, -112620),
    NgC(2244, 23544, 17520),
    NgC(2250, 23568, -18120),
    NgC(2251, 23682, 30120),
    NgC(2252, 23700, 19380),
    NgC(2254, 23760, 27600),
    NgC(2261, r(6,39,1), r(8,45,0)),
    NgC(2264, 24066, 35580),
    NgC(2266, 24192, 97080),
    NgC(2269, 24234, 16440),
    NgC(2281, 24558, 147840),
    NgC(2286, 24456, -11400),
    NgC(2287, 24420, -74640),
    NgC(2298, 24540, -129600),
    NgC(2301, 24708, 1680),
    NgC(2302, 24714, -25440),
    NgC(2304, 24900, 64860),
    NgC(2311, 25068, -16500),
    NgC(2323, 25392, -30000),
    NgC(2324, 25452, 3780),
    NgC(2331, 25632, 98460),
    NgC(2335, 25596, -36300),
    NgC(2336, 26826, 288660),
    NgC(2343, 25698, -38340),
    NgC(2345, 25698, -47400),
    NgC(2353, 26076, -37080),
    NgC(2354, 26058, -92640),
    NgC(2355, 26214, 49620),
    NgC(2360, 26268, -56220),
    NgC(2362, 26328, -89820),
    NgC(2366, 26934, 249180),
    NgC(2367, 26406, -78960),
    NgC(2374, 26640, -47760),
    NgC(2383, 26688, -75360),
    NgC(2384, 26706, -75720),
    NgC(2392, 26952, 75300),
    NgC(2395, 26826, 48900),
    NgC(2396, 26886, -42240),
    NgC(2403, 27414, 236160),
    NgC(2414, 27198, -55620),
    NgC(2419, 27486, 139980),
    NgC(2420, 27510, 77640),
    NgC(2421, 27378, -74220),
    NgC(2422, 27396, -52200),
    NgC(2423, 27426, -49920),
    NgC(2432, 27654, -68700),
    NgC(2437, 27708, -53340),
    NgC(2438, 27708, -53040),
    NgC(2439, 27648, -113940),
    NgC(2447, 27876, -85920),
    NgC(2451, 27924, -136680),
    NgC(2453, 28068, -98040),
    NgC(2455, 28140, -76680),
    NgC(2467, 28356, -94980),
    NgC(2477, 28338, -138780),
    NgC(2479, 28506, -63780),
    NgC(2482, 28494, -87480),
    NgC(2483, 28554, -100560),
    NgC(2489, 28572, -108240),
    NgC(2506, 28812, -38820),
    NgC(2509, 28842, -68640),
    NgC(2516, 28698, -219120),
    NgC(2527, 29118, -101400),
    NgC(2533, 29220, -107640),
    NgC(2539, 29442, -46200),
    NgC(2546, 29544, -135480),
    NgC(2547, 29442, -177360),
    NgC(2548, 29628, -20880),
    NgC(2567, 29916, -110280),
    NgC(2571, 29934, -107040),
    NgC(2579, 30066, -130260),
    NgC(2580, 30096, -109140),
    NgC(2587, 30210, -106200),
    NgC(2613, 30804, -82680),
    NgC(2627, 31038, -107820),
    NgC(2632, 31206, 71940),
    NgC(2655, 32136, 281580),
    NgC(2658, 31404, -117540),
    NgC(2659, 31356, -161820),
    NgC(2660, 31332, -169740),
    NgC(2669, 31494, -190680),
    NgC(2670, 31530, -175620),
    NgC(2681, 32010, 184740),
    NgC(2682, 31824, 42540),
    NgC(2683, 31962, 120300),
    NgC(2768, 33096, 216120),
    NgC(2775, 33018, 25320),
    NgC(2784, 33138, -87000),
    NgC(2787, 33558, 249120),
    NgC(2808, 33120, -233520),
    NgC(2818, 33360, -131820),
    NgC(2841, 33720, 183480),
    NgC(2859, 33858, 124260),
    NgC(2867, 33684, -209940),
    NgC(2903, 34332, 77400),
    NgC(2905, 34332, 77460),
    NgC(2910, 34224, -190440),
    NgC(2925, 34422, -192360),
    NgC(2972, 34818, -181200),
    NgC(2974, 34956, -13320),
    NgC(2976, 35238, 244500),
    NgC(2985, 35424, 260220),
    NgC(2986, 35058, -76620),
    NgC(3031, 35736, 248640),
    NgC(3033, 35328, -203100),
    NgC(3034, 35748, 250860),
    NgC(3077, 36198, 247440),
    NgC(3079, 36120, 200460),
    NgC(3105, 36048, -197160),
    NgC(3109, 36186, -94140),
    NgC(3114, 36162, -216420),
    NgC(3115, 36312, -27780),
    NgC(3132, 36420, -145560),
    NgC(3147, 37014, 264240),
    NgC(3166, 36828, 12360),
    NgC(3169, 36852, 12480),
    NgC(3184, 37098, 149100),
    NgC(3193, 37104, 78840),
    NgC(3195, r(10,9,21), r(-80,51,31)),
    NgC(3198, 37194, 163980),
    NgC(3201, 37056, -167100),
    NgC(3227, 37410, 71520),
    NgC(3228, 37308, -186180),
    NgC(3242, 37488, -67080),
    NgC(3245, 37638, 102600),
    NgC(3247, 37554, -208560),
    NgC(3293, 38148, -209640),
    NgC(3310, 38322, 192600),
    NgC(3324, 38238, -211080),
    NgC(3330, 38316, -194940),
    NgC(3338, 38526, 49500),
    NgC(3344, 38610, 89700),
    NgC(3351, 38640, 42120),
    NgC(3359, 38796, 227580),
    NgC(3368, 38808, 42540),
    NgC(3372, r(10,45,8), r(-59,52,4)),
    NgC(3377, 38862, 50340),
    NgC(3379, 38868, 45300),
    NgC(3384, 38898, 45480),
    NgC(3412, 39054, 48300),
    NgC(3414, 39078, 100740),
    NgC(3486, 39624, 104280),
    NgC(3489, 39618, 50040),
    NgC(3496, 39588, -217200),
    NgC(3521, 39948, -120),
    NgC(3532, 39984, -211200),
    NgC(3556, 40290, 200400),
    NgC(3557, 40200, -135120),
    NgC(3572, 40224, -216840),
    NgC(3585, 40398, -96300),
    NgC(3587, 40487, 198068), //M97
    NgC(3590, 40374, -218820),
    NgC(3603, 40506, -220500),
    NgC(3607, 40614, 64980),
    NgC(3610, 40704, 211620),
    NgC(3621, 40698, -118140),
    NgC(3623, 40734, 47100),
    NgC(3626, 40806, 66060),
    NgC(3627, 40812, 46740),
    NgC(3628, 40818, 48960),
    NgC(3631, 40860, 191400),
    NgC(3640, 40866, 11640),
    NgC(3665, 41082, 139560),
    NgC(3680, 41142, -155700),
    NgC(3718, 41556, 191040),
    NgC(3726, 41598, 169320),
    NgC(3766, 41766, -221820),
    NgC(3810, 42060, 41280),
    NgC(3898, 42552, 201900),
    NgC(3918, 42618, -205860),
    NgC(3923, 42660, -103680),
    NgC(3938, 42768, 158820),
    NgC(3945, 42792, 218460),
    NgC(3953, 42828, 188400),
    NgC(3960, 42654, -200520),
    NgC(3962, 42882, -50280),
    NgC(3992, 43056, 192180),
    NgC(3998, 43074, 199620),
    NgC(4036, 43284, 222840),
    NgC(4038, 43314, -67920),
    NgC(4039, r(12,1,53), r(-18,52,10)),
    NgC(4051, 43392, 160320),
    NgC(4052, 43314, -227520),
    NgC(4088, 43536, 181980),
    NgC(4096, 43560, 170940),
    NgC(4103, 43602, -220500),
    NgC(4111, 43626, 155040),
    NgC(4125, 43686, 234660),
    NgC(4147, 43806, 66780),
    NgC(4151, 43830, 141840),
    NgC(4179, 43974, 4680),
    NgC(4192, 44028, 53640),
    NgC(4203, 44106, 119520),
    NgC(4214, 44136, 130800),
    NgC(4216, 44154, 47340),
    NgC(4230, 44238, -198480),
    NgC(4236, 44202, 250080),
    NgC(4244, 44250, 136140),
    NgC(4254, 44328, 51900),
    NgC(4258, 44340, 170280),
    NgC(4261, 44364, 20940),
    NgC(4267, 44388, 46080),
    NgC(4274, 44388, 106620),
    NgC(4278, 44406, 105420),
    NgC(4303, 44514, 16080),
    NgC(4314, 44556, 107580),
    NgC(4321, 44574, 56940),
    NgC(4337, 44634, -209280),
    NgC(4349, 44670, -222840),
    NgC(4361, 44670, -67680),
    NgC(4371, 44694, 42120),
    NgC(4372, 44748, -261600),
    NgC(4374, 44706, 46380),
    NgC(4382, 44724, 65460),
    NgC(4394, 44754, 65580),
    NgC(4395, 44748, 120780),
    NgC(4406, 44772, 46620),
    NgC(4414, 44784, 112380),
    NgC(4429, 44844, 40020),
    NgC(4435, 44862, 47100),
    NgC(4438, 44868, 46860),
    NgC(4439, 44904, -216360),
    NgC(4442, 44886, 35280),
    NgC(4449, 44892, 158760),
    NgC(4450, 44910, 61500),
    NgC(4457, 44940, 12840),
    NgC(4459, 44940, 50340),
    NgC(4463, 45000, -233280),
    NgC(4472, 44988, 28800),
    NgC(4473, 44988, 48360),
    NgC(4477, 45000, 49080),
    NgC(4486, 45048, 44640),
    NgC(4490, 45036, 149880),
    NgC(4494, 45084, 92820),
    NgC(4501, 45120, 51900),
    NgC(4517, 45168, 420),
    NgC(4526, 45240, 27720),
    NgC(4527, 45246, 9540),
    NgC(4535, 45258, 29520),
    NgC(4536, 45270, 7860),
    NgC(4546, 45330, -13680),
    NgC(4548, 45324, 52200),
    NgC(4552, 45342, 45180),
    NgC(4559, 45360, 100680),
    NgC(4565, 45378, 93540),
    NgC(4568, 45396, 40440),
    NgC(4569, 45408, 47400),
    NgC(4570, 45414, 26100),
    NgC(4579, 45462, 42540),
    NgC(4590, 45570, -96300),
    NgC(4594, 45600, -41820),
    NgC(4596, 45594, 36660),
    NgC(4609, 45738, -226680),
    NgC(4618, 45690, 148140),
    NgC(4621, 45720, 41940),
    NgC(4631, 45726, 117120),
    NgC(4636, 45768, 9660),
    NgC(4643, 45798, 7140),
    NgC(4649, 45822, 41580),
    NgC(4651, 45822, 59040),
    NgC(4654, 45840, 47280),
    NgC(4656, 45840, 115800),
    NgC(4666, 45906, -1680),
    NgC(4689, 46068, 49560),
    NgC(4696, 46128, -148740),
    NgC(4697, 46116, -20880),
    NgC(4698, 46104, 30540),
    NgC(4699, 46140, -31200),
    NgC(4725, 46224, 91800),
    NgC(4736, 46254, 148020),
    NgC(4753, 46344, -4320),
    NgC(4754, 46338, 40740),
    NgC(4755, 46416, -217200),
    NgC(4762, 46374, 40440),
    NgC(4815, 46680, -233820),
    NgC(4826, 46602, 78060),
    NgC(4833, 46776, -255180),
    NgC(4852, 46806, -214560),
    NgC(4856, 46758, -54120),
    NgC(4889, r(13,0,8), r(27,58,37)),
    NgC(4945, 47124, -178080),
    NgC(4958, 47148, -28860),
    NgC(4976, 47316, -178200),
    NgC(5005, 47454, 133380),
    NgC(5018, 47580, -70260),
    NgC(5024, 47574, 65400),
    NgC(5033, 47604, 131760),
    NgC(5053, 47784, 63720),
    NgC(5055, 47748, 151320),
    NgC(5102, 48120, -131880),
    NgC(5128, 48330, -154860),
    NgC(5138, 48438, -212460),
    NgC(5139, 48408, -170940),
    NgC(5168, 48672, -219360),
    NgC(5189, 48810, -237540),
    NgC(5194, 48594, 169920),
    NgC(5195, 48600, 170160),
    NgC(5236, 49020, -107520),
    NgC(5247, 49086, -64380),
    NgC(5248, 49050, 31980),
    NgC(5253, 49194, -113940),
    NgC(5272, 49332, 102180),
    NgC(5281, 49596, -226440),
    NgC(5286, 49584, -184920),
    NgC(5316, 50034, -222720),
    NgC(5322, 49758, 216720),
    NgC(5363, 50166, 18900),
    NgC(5364, 50172, 18060),
    NgC(5371, 50142, 145680),
    NgC(5457, 50592, 195660),
    NgC(5460, 50856, -173940),
    NgC(5466, 50730, 102720),
    NgC(5474, 50700, 193200),
    NgC(5566, 51618, 14160),
    NgC(5576, 51666, 11760),
    NgC(5585, 51588, 204240),
    NgC(5606, 52068, -214680),
    NgC(5617, 52188, -218580),
    NgC(5634, 52176, -21540),
    NgC(5662, 52512, -203580),
    NgC(5676, 52368, 178080),
    NgC(5694, 52776, -95520),
    NgC(5715, 53004, -207180),
    NgC(5746, 53094, 7020),
    NgC(5749, 53334, -196260),
    NgC(5813, 54072, 6120),
    NgC(5822, 54312, -195660),
    NgC(5823, 54342, -200160),
    NgC(5824, 54240, -119040),
    NgC(5838, 54324, 7560),
    NgC(5846, 54384, 5760),
    NgC(5866, 54390, 200760),
    NgC(5897, 55044, -75660),
    NgC(5904, 55116, 7500),
    NgC(5907, 54954, 202740),
    NgC(5921, 55314, 18240),
    NgC(5925, 55662, -196260),
    NgC(5927, 55680, -182400),
    NgC(5946, 56130, -182400),
    NgC(5986, 56766, -136020),
    NgC(5999, 57132, -203280),
    NgC(6005, 57348, -206760),
    NgC(6025, 57822, -217800),
    NgC(6031, 58056, -194640),
    NgC(6067, 58392, -195180),
    NgC(6087, 58734, -208440),
    NgC(6093, 58620, -82740),
    NgC(6101, 59148, -259920),
    NgC(6121, 59016, -95520),
    NgC(6124, 59136, -146400),
    NgC(6134, 59262, -176940),
    NgC(6139, 59262, -139860),
    NgC(6144, 59238, -93720),
    NgC(6152, 59562, -189420),
    NgC(6167, 59664, -178560),
    NgC(6169, 59646, -158580),
    NgC(6171, 59550, -46980),
    NgC(6178, 59742, -164280),
    NgC(6192, 60018, -156120),
    NgC(6193, 60078, -175560),
    NgC(6200, 60252, -170940),
    NgC(6204, 60390, -169260),
    NgC(6205, 60102, 131280),
    NgC(6208, 60570, -193740),
    NgC(6210, 60270, 85740),
    NgC(6216, 60564, -161040),
    NgC(6218, 60432, -7020),
    NgC(6222, 60642, -161100),
    NgC(6229, 60420, 171120),
    NgC(6231, 60840, -150480),
    NgC(6235, 60804, -79860),
    NgC(6242, 60936, -142200),
    NgC(6249, 61056, -161220),
    NgC(6250, 61080, -164880),
    NgC(6253, 61146, -189780),
    NgC(6254, 61026, -14760),
    NgC(6259, 61242, -160800),
    NgC(6266, 61272, -108420),
    NgC(6268, 61344, -143040),
    NgC(6273, 61356, -94560),
    NgC(6281, 61488, -136440),
    NgC(6284, 61470, -89160),
    NgC(6287, 61512, -81720),
    NgC(6293, 61812, -95700),
    NgC(6302, r(17,13,44), r(-37,6,16)),
    NgC(6304, 62070, -106080),
    NgC(6316, 62196, -101280),
    NgC(6322, 62310, -154620),
    NgC(6325, 62280, -85560),
    NgC(6333, 62352, -66660),
    NgC(6341, 62226, 155280),
    NgC(6342, 62472, -70500),
    NgC(6352, 62730, -174300),
    NgC(6355, 62640, -94860),
    NgC(6356, 62616, -64140),
    NgC(6362, 63114, -241380),
    NgC(6366, 62862, -18300),
    NgC(6374, 63138, -117360),
    NgC(6383, 63288, -117240),
    NgC(6384, 63144, 25440),
    NgC(6388, 63378, -161040),
    NgC(6396, 63486, -126000),
    NgC(6397, 63642, -193200),
    NgC(6400, 63648, -133020),
    NgC(6401, 63516, -86100),
    NgC(6402, 63456, -11700),
    NgC(6405, 63606, -115980),
    NgC(6416, 63864, -116460),
    NgC(6425, 64014, -113520),
    NgC(6440, 64134, -73320),
    NgC(6441, 64212, -133380),
    NgC(6451, 64242, -108780),
    NgC(6453, 64254, -124560),
    NgC(6469, 64374, -80460),
    NgC(6475, 64434, -125340),
    NgC(6494, 64608, -68460),
    NgC(6496, 64740, -159360),
    NgC(6503, 64164, 252540),
    NgC(6507, 64776, -62640),
    NgC(6514, 64938, -82920),
    NgC(6517, 64908, -32280),
    NgC(6520, 65004, -100440),
    NgC(6522, 65016, -108120),
    NgC(6523, 65028, -87780),
    NgC(6528, 65088, -108180),
    NgC(6530, 65088, -87600),
    NgC(6531, 65076, -81000),
    NgC(6535, 65028, -1080),
    NgC(6539, 65088, -27300),
    NgC(6541, 65280, -157320),
    NgC(6543, 64716, 239880),
    NgC(6544, 65238, -90000),
    NgC(6546, 65232, -84000),
    NgC(6553, 65358, -93240),
    NgC(6568, 65568, -77760),
    NgC(6569, 65616, -114600),
    NgC(6572, 65526, 24660),
    NgC(6583, 65748, -79680),
    NgC(6584, 65916, -187980),
    NgC(6595, 65820, -71580),
    NgC(6603, 65820, -66780), //M24
    NgC(6604, 65886, -44040),
    NgC(6605, 65826, -53880),
    NgC(6611, 65928, -49620),
    NgC(6613, 65994, -61680),
    NgC(6618, 66048, -58260),
    NgC(6624, 66222, -109320),
    NgC(6625, 66192, -43380),
    NgC(6626, 66270, -89520),
    NgC(6633, 66462, 23640),
    NgC(6637, 66684, -116460),
    NgC(6638, 66654, -91800),
    NgC(6642, 66714, -84540),
    NgC(6645, 66756, -60840),
    NgC(6647, 66690, -62460),
    NgC(6649, 66810, -37440),
    NgC(6652, 66948, -118740),
    NgC(6656, 66984, -86040),
    NgC(6664, 67002, -29580),
    NgC(6681, 67392, -116280),
    NgC(6683, 67332, -22620),
    NgC(6684, 67740, -234660),
    NgC(6694, 67512, -33840),
    NgC(6704, 67854, -18720),
    NgC(6705, 67866, -22560),
    NgC(6709, 67890, 37260),
    NgC(6712, 67986, -31320),
    NgC(6715, 68106, -109740),
    NgC(6716, 68076, -71580),
    NgC(6720, 68016, 118920),
    NgC(6723, 68376, -131880),
    NgC(6729, r(19,1,54), r(-36,57,12)),
    NgC(6738, 68484, 41760),
    NgC(6744, 68988, -229860),
    NgC(6752, 69054, -215940),
    NgC(6755, 68868, 15240),
    NgC(6760, 69072, 3720),
    NgC(6779, 69396, 108660),
    NgC(6790, 69792, 5460),
    NgC(6791, 69642, 136260),
    NgC(6802, 70236, 72960),
    NgC(6809, 70800, -111480),
    NgC(6811, 70692, 167640),
    NgC(6818, 71040, -50940),
    NgC(6819, 70878, 144660),
    NgC(6822, 71094, -53280),
    NgC(6823, 70986, 83880),
    NgC(6826, 71088, 181860),
    NgC(6830, 71460, 83040),
    NgC(6834, 71532, 105900),
    NgC(6838, 71628, 67620),
    NgC(6853, 71976, 81780),
    NgC(6864, 72366, -78900),
    NgC(6866, 72222, 158400),
    NgC(6871, 72354, 128820),
    NgC(6882, 72702, 95580),
    NgC(6883, 72678, 129060),
    NgC(6885, 72720, 95340),
    NgC(6888, r(20,12,7), r(38,21,20)),
    NgC(6910, 73386, 146820),
    NgC(6913, 73434, 138720),
    NgC(6934, 74052, 26640),
    NgC(6939, 73884, 218280),
    NgC(6940, 74076, 101880),
    NgC(6946, 74088, 216540),
    NgC(6960, r(20,45,38), r(30,42,30)),
    NgC(6981, 75210, -45120),
    NgC(6992, r(20,45,38),r(30,42,30)),
    NgC(6994, 75540, -45480),
    NgC(6997, 75390, 160680),
    NgC(7000, r(20,59,17), r(44,31,44)),
    NgC(7006, 75690, 58260),
    NgC(7009, 75852, -40920),
    NgC(7023, 75630, 245400),
    NgC(7027, 76026, 152040),
    NgC(7031, 76038, 183000),
    NgC(7039, 76272, 164340),
    NgC(7049, 76740, -174840),
    NgC(7062, 76992, 166980),
    NgC(7063, 77064, 131400),
    NgC(7067, 77052, 172860),
    NgC(7078, 77400, 43800),
    NgC(7082, 77364, 169500),
    NgC(7086, 77430, 185700),
    NgC(7089, 77610, -2940),
    NgC(7092, 77532, 174360),
    NgC(7099, 78024, -83460),
    NgC(7128, 78240, 193380),
    NgC(7142, 78354, 236880),
    NgC(7144, 78762, -173700),
    NgC(7160, 78822, 225360),
    NgC(7209, 79512, 167400),
    NgC(7213, 79758, -169800),
    NgC(7217, 79674, 112920),
    NgC(7226, 79830, 199500),
    NgC(7235, 79956, 206220),
    NgC(7243, 80118, 179580),
    NgC(7245, 80118, 195600),
    NgC(7261, 80424, 209100),
    NgC(7293, r(22,29,38), r(-20,50,13)),
    NgC(7296, 80892, 188220),
    NgC(7314, 81348, -93780),
    NgC(7331, 81426, 123900),
    NgC(7380, 82020, 209160),
    NgC(7410, 82500, -142800),
    NgC(7457, 82860, 108540),
    NgC(7479, r(23,4,56), r(12,19,22)),
    NgC(7507, 83526, -102720),
    NgC(7510, 83490, 218040),
    NgC(7552, 83772, -153300),
    NgC(7582, 83904, -152520),
    NgC(7606, 83946, -30540),
    NgC(7635, r(23,20,48), r(61,12,6)),
    NgC(7640, 84126, 147060),
    NgC(7654, 84252, 221700),
    NgC(7662, 84354, 153180),
    NgC(7686, 84612, 176880),
    NgC(7727, 85194, -44280),
    NgC(7762, 85788, 244920),
    NgC(7788, 86202, 221040),
    NgC(7789, 86220, 204240),
    NgC(7790, 86304, 220380),
    NgC(7793, 86268, -117300),
    NgC(7814, 198, 58140),

    NgC(7815, 66696, -69300), //M25
    NgC(7816, 44532, 209099), //M40

    NgC(7817, r(3,46,48), r(68,5,46) ),     // C5  IC 342
    NgC(7818, r(22,57,17), r(62,28,33) ),   // C9  Sh2-155
    NgC(7819, r(21,53,28), r(47, 16, 1)),   // C19  IC 5146}, 
    NgC(7820, r(5,16,05), r(34, 27, 49)),   // C31  IC 405},  
    NgC(7821, r(4,26,54), r(15, 52, 0)),    // C41  Hyades},  
    NgC(7822, r(1,04,47), r(2, 7, 4)),      // C51  IC 1613}, 
    NgC(7823, r(8,40,36), -r(53, 2,  0)),   // C85  IC 2391}, 
    NgC(7824, r(12,50,0), -r(62, 30, 0)),   // C99      
    NgC(7825, r(11,36,36), -r(63, 2, 0)),   // C100   IC 2944},
    NgC(7826, r(10,42,57), -r(64, 23, 39)), // C102   IC 2602},
#undef r
};
#undef rdc
#undef NgC
static int const nbNgc2= sizeof(NGCs)/sizeof(NGCs[0])-12;

#if 1 //def HASPlanets
//////////////////////////////////////////////
// Planetary stuff
namespace grandR {
static float timeFromDate(int y, int m, int d, int h, int M, int s)
{
    int32_t D= 367*y - (7*(y+((m+9)/12)))/4 + (275*m)/9 + d - 730530L;
    float H= (h+M/60.0f+s/3600.0f)/24.0f;
    return D+H;
}

//  N = longitude of the ascending node
//  i = inclination to the ecliptic (plane of the Earth's orbit)
//  w = argument of perihelion
//  a = semi-major axis, or mean distance from Sun
//  e = eccentricity (0=circle, 0-1=ellipse, 1=parabola)
//  M = mean anomaly (0 at perihelion; increases uniformly with time)
struct TOrbitalElements { float N1, N2, i1, i2, w1, w2, a1, a2, e1, e2, M1, M2; };
class TOrbitalElementsAt { public:
    float N, i, w, a, e, M; 
    float fmodf360(float a)
    {
        a= fmodf(a, 360.0f); if (a<0.0f) a+= 360.0f;
        return a;
    }
    TOrbitalElementsAt(TOrbitalElements const &o, float d)
    { 
        N= fmodf360(o.N1+o.N2*d); i=fmodf360(o.i1+o.i2*d); w= o.w1+o.w2*d;
        a= o.a1+o.a2*d; e= o.e1+o.e2*d; M= fmodf360(o.M1+o.M2*d);
    }
};
static TOrbitalElements const oe[9]={
    // N                        i                      w                         a                       e                        M
    { 0.0f,       0.0f,         0.0f,     0.0f,        282.9404f,  4.70935E-5f,  1.0f,       0.0f,       0.016709f,  -1.151E-9f,  356.0470f,  0.9856002585f}, // sun
    { 48.3313f,   3.24587E-5f,  7.0047f,  5.00E-8f,    29.1241f,   1.01444E-5f,  0.387098f,  0.0f,       0.205635f,  5.59E-10f,   168.6562f,  4.0923344368f}, // mercury
    { 76.6799f,   2.46590E-5f,  3.3946f,  2.75E-8f,    54.8910f,   1.38374E-5f,  0.723330f,  0.0f,       0.006773f,  1.302E-9f,   48.0052f,   1.6021302244f}, // Venus
    { 49.5574f,   2.11081E-5f,  1.8497f,  -1.78E-8f,   286.5016f,  2.92961E-5f,  1.523688f,  0.0f,       0.093405f,  2.516E-9f,   18.6021f,   0.5240207766f}, // Mars
    { 100.4542f,  2.76854E-5f,  1.3030f,  -1.557E-7f,  273.8777f,  1.64505E-5f,  5.20256f,   0.0f,       0.048498f,  4.469E-9f,   19.8950f,   0.0830853001f}, // Jupiter
    { 113.6634f,  2.38980E-5f,  2.4886f,  -1.081E-7f,  339.3939f,  2.97661E-5f,  9.55475f,   0.0f,       0.055546f,  -9.499E-9f,  316.9670f,  0.0334442282f}, // Saturne
    { 74.0005f,   1.3978E-5f,   0.7733f,  1.9E-8f,     96.6612f,   3.0565E-5f,   19.18171f,  -1.55E-8f,  0.047318f,  7.45E-9f,    142.5905f,  0.011725806f}, // Uranus
    { 131.7806f,  3.0173E-5f,   1.7700f,  -2.55E-7f,   272.8461f,  -6.027E-6f,   30.05826f,  3.313E-8f,  0.008606f,  2.15E-9f,    260.2471f,  0.005995147f} // Nepture
};
static float const MPI= 3.14159265358979323846264338327f;
static float const degToRad = MPI/180.0f;
static float inline dtr(float a) { return float(a)*degToRad; }

// Get sun position compared with earth. unit = AU
static void sunPos(float d, float &sx, float &sy, float &sz, float &ex, float &ey)
{
    float ecl= 23.4393f - 3.563E-7f * d; // ecl angle
    TOrbitalElementsAt sun(oe[0], d);
    float E= sun.M+sun.e*(180.0f/MPI)*sinf(sun.M*degToRad)*(1.0f+sun.e*cosf(sun.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    // compute the Sun's distance r and its true anomaly v from:
    float xv = cosf(E*degToRad)-sun.e;
    float yv = sqrt(1.0f-sun.e*sun.e) * sinf(E*degToRad);
    float v = atan2f( yv, xv );
    float rs = sqrt( xv*xv + yv*yv );
    //compute the Sun's true longitude
    float lonsun = v + sun.w*degToRad;
    // Convert lonsun,r to ecliptic rectangular geocentric coordinates xs,ys:
    ex= sx = rs * cosf(-lonsun);
    ey = -rs * sinf(-lonsun);
    // (since the Sun always is in the ecliptic plane, zs is of course zero). xs,ys is the Sun's position in a coordinate system in the plane of the ecliptic. To convert this to equatorial, rectangular, geocentric coordinates, compute:
    sy= ey * cosf(ecl*degToRad);
    sz= ey * sinf(ecl*degToRad);
}
// get an object position in solar system
static void objPos(float d, int ob, float &x, float &y, float &z)
{
    TOrbitalElementsAt o(oe[ob], d);
    float E= o.M+o.e*(180.0f/MPI)*sinf(o.M*degToRad)*(1.0f+o.e*cosf(o.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    float E1= E;
    for (int i=0; i<5; i++) E= E - ( E - o.e*(180.0f/MPI) * sinf(E*degToRad) - o.M) / ( 1 - o.e * cosf(E*degToRad) );
    float xv = o.a * ( cosf(E*degToRad) - o.e );
    float yv = o.a * ( sqrt(1.0f - o.e*o.e) * sinf(E*degToRad) );
    float v = atan2f( yv, xv );
    float r = sqrt( xv*xv + yv*yv );
    x = r * ( cosf(o.N*degToRad) * cosf(v+o.w*degToRad) - sinf(o.N*degToRad) * sinf(v+o.w*degToRad) * cosf(o.i*degToRad) );
    y = r * ( sinf(o.N*degToRad) * cosf(v+o.w*degToRad) + cosf(o.N*degToRad) * sinf(v+o.w*degToRad) * cosf(o.i*degToRad) );
    z = r * ( sinf(v+o.w*degToRad) * sinf(o.i*degToRad) );
}

static void objPos(float d, int ob, float &x, float &y, float &z, float sex, float sey)
{
    // Sun centered, ecliptic centered coordinates...
    // Need to rotate around earth center by ecliptic!
    // so, add earth coordinate, rotate by ecliptic and stay there...
    float tx, ty, tz;
    objPos(d, ob, tx, ty, tz);
    x= tx+sex, ty= ty+sey;
    float ecl= 23.4393f - 3.563E-7f * d; // ecl angle
    y= ty * cosf(ecl*degToRad) - tz*sinf(ecl*degToRad);
    z= ty * sinf(ecl*degToRad) + tz*cosf(ecl*degToRad);
}

int8_t year=23, month=1, day=1;
static char planetNames[8][8]= { "Soleil", "Mercure", "Venus", "Mars", "Jupiter", "Saturne", "Uranus", "Neptune" };

void planetPos(int p, int32_t &ra, int32_t &dec) // return planet position. 0 is sun, 1, 2 are mercury, venus, 3-7 are Mars to Neptune
{
    float d=timeFromDate(year+2000, month, day, 0, 0, 0); float sx, sy, sz, sex, sey; 
    sunPos(d, sx, sy, sz, sex, sey);
    float ox, oy, oz; objPos(d, p, ox, oy, oz, sex, sey);
    float raf= atan2f(oy, ox)/degToRad/15.0f, decf= atan2f(oz, sqrtf(ox*ox+oy*oy))/degToRad;
    ra= int32_t(raf*3600.0f); while (ra<0) ra+= 24*3600L;
    dec= int32_t(decf*3600.0f);
}
};

#if 1
namespace GrandN {
class Cartesian { // All is in AU, Uranus is 19 AU away from earth, so having 5 bits for IP should be enough!
public:
    int32_t v;
    inline Cartesian(): v(0) { }
    inline Cartesian(int32_t v): v(v) { }
    inline Cartesian(float v): v(int32_t(v*(1<<24))) { }
    inline Cartesian operator+(Cartesian v2) const { return Cartesian(v+v2.v); }
    inline Cartesian operator-(Cartesian v2) const { return Cartesian(v-v2.v); }
    inline Cartesian operator-() const { return Cartesian(-v); }
    inline Cartesian operator*(Cartesian v2) const { return Cartesian(int32_t((int64_t(v)*v2.v)>>24)); }
    inline Cartesian operator/(Cartesian v2) const { return Cartesian(int32_t(((int64_t(v)<<24)/v2.v))); }
    inline bool operator>(Cartesian v2) const { return v>v2.v; }
    inline bool operator<(Cartesian v2) const { return v>v2.v; }
#ifdef PC
    char t[20];
    char *str() { sprintf(t, "%f", float(v/16777216.0)); return t; }
#endif
};
Cartesian sqrt(Cartesian a1, Cartesian a2, Cartesian b1, Cartesian b2) // return sqrt a1*a2+b1*b2
{
    int64_t v= int64_t(a1.v)*a2.v + int64_t(b1.v)*b2.v;
    if (v<0) return Cartesian();
    uint32_t t= uint32_t(v>>48); uint8_t bits= 0; while (t!=0) { bits++; t>>=1; }
    int64_t x= v>>(bits/2+24);
    for (int8_t i=10; --i>=0; ) x= (x+v/x)/2;
    return Cartesian(int32_t(x));
}

// sinTable[i]=65536*sin(i in degree * 128/180)
static uint16_t const sinTable[65]={0, 1608, 3215, 4821, 6423, 8022, 9616, 11204, 12785, 14359, 15923, 17479, 19024, 20557, 22078, 23586, 25079, 26557, 28020, 29465, 30893, 32302, 33692, 35061, 36409, 37736, 39039, 40319, 41575, 42806, 44011, 45189, 46340, 47464, 48558, 49624, 50660, 51665, 52639, 53581, 54491, 55368, 56212, 57022, 57797, 58538, 59243, 59913, 60547, 61144, 61705, 62228, 62714, 63162, 63571, 63943, 64276, 64571, 64826, 65043, 65220, 65358, 65457, 65516, 65535};

class Angle { public:
    uint16_t v;
    inline Angle(): v(0) {}
    inline Angle(uint16_t v): v(v) { }
    inline Angle(float v): v(uint16_t(v*256.0f/360.0f*256.0f)) { }
    static inline Angle fromRad(Cartesian v) { return Angle(uint16_t((v.v*int64_t(10430))>>24)); } // v is rad<<24 and I want [0..256<<8[ so (0x8000/pi=10430)>>24
    inline Angle operator+(Angle v2) const { return Angle(uint16_t(v+v2.v)); }
    inline Angle operator-(Angle v2) const { return Angle(uint16_t(v-v2.v)); }
    inline Angle operator-() const { return Angle(uint16_t(-v)); }
    //inline Angle operator*(int32_t v2) const { return Angle(uint16_t(v*v2)); }
    inline Angle operator/(Cartesian v2) const { return Angle(uint16_t((int64_t(v)<<24)/v2.v)); }

    static Cartesian sin(uint16_t a) // a: [0, 65536[ correspond to [0,360deg[. Returns sin(a)*(1<<24)
    {
        a>>= 8;
        if (a<0x40) return sinTable[a]<<8;
        if (a<0x80) return sinTable[128-a]<<8;
        if (a<0xC0) return -int32_t(sinTable[a-128])<<8;
        return -int32_t(sinTable[uint8_t(0-a)])<<8;
    }
    // sin (a+b) = sin(a)*cos(b) + cos(a)*sin(b)
    // if b is small cos(b)=1 and  sin(b)=b
    // sin (a+b) = sin(a) + b*cos(a)
    static Cartesian sinPrecision(uint16_t a) // a: [0, 65536[ correspond to [0,360deg[. Returns sin(a)*(1<<24)
    {
        int32_t s, c;
        uint8_t a2= a>>8;
        if (a2<0x40) s= sinTable[a2], c= sinTable[64-a2];
        else if (a2<0x80) s= sinTable[128-a2], c= -int32_t(sinTable[a2-64]);
        else if (a2<0xC0) s= -int32_t(sinTable[a2-128]), c= -int32_t(sinTable[192-a2]);
        else s= -int32_t(sinTable[uint8_t(0-a2)]), c= sinTable[a2-192];
        int32_t t= (a&0xff)*6; // a&0xff = fp(a)*256 in [0..256[ so to get to fp(a)*256^2 in [0..2pi[, multiply by 6
        t= (t*c)>>8; // so, t has a factor of 1<<16 and c a factor of <<16. to get to our factor <<24, we need to shift by >>8
        return (s<<8)+t;
    }

    inline Cartesian sin() const { return sinPrecision(v); }
    inline Cartesian sin2() const { return sin(v); }
    Cartesian cos() const { return sinPrecision(v+0x4000); }
#ifdef PC
    char t[20];
    char *str() { sprintf(t, "%.2f", float(v*5.493164e-3)); return t; }
    float fval() { return float(v*5.493164e-3); }
#endif
};

Angle atan2(Cartesian y, Cartesian x)
{
    if (x.v==0) // x=0, can be +/-90 depending on y
    {
        if (y.v==0) return Angle(); // error!!!
        if (y.v>0) return Angle(uint16_t(0x4000));
        return Angle(uint16_t(0xC000));
    }
    // We will calculate atan(x in [0..0.5]).
    // But it this only correspond to an eight of the circle... so we will need to correct the final answer by potentially negating it and adding a vlaue...
    // the sign of x and y as well as the order of their magnitude indicates the octan that we are in
    // sx sy x<y pos      finalOp
    //  +  +  f  0-45     r
    //  +  +  t  45-90    90-r
    //  +  -  f  135-180  180-r
    //  +  -  t  90-135   90+r
    //  -  +  f  270-315  270+r
    //  -  +  t  315-360  360-r
    //  -  -  f  180-225  180+r
    //  -  -  t  225-270  270-r
    uint8_t const changes[8]= { 0x00, 0x41, 0x81, 0x40, 0x01, 0xC0, 0x80, 0xC1 };
    uint8_t m= ((y.v>>29)&0b100)+((x.v>>30)&0b10)+(abs(x.v)>abs(y.v)?0:1);
    uint8_t addAtEnd= changes[m];
    // now calculate arctan(y/x)
    // Since, atan(a) = 90-atan(1/a)
    // make sure that v=y/x is in [0..1] and if not, mark the need for neg and update addAtEnd..
    Cartesian v; if (abs(x.v)<abs(y.v)) v= x/y; else v= y/x; if (v.v<0) v=-v;
    // approximate arctan(x) by a 4th order polynomial centered at x=0.5 (taylor series)...
    // 0.463647609001+0.8*(x-0.5)-0.32*(x-0.5)^2-4.26666666667e-2*(x-0.5)^3+0.1536*(x-0.5)^4
    // But the coefitians here are for a result in radians.. and we want a result in our angle unit, so multiply by 0x8000/pi=10430
    // so the coefs becomes: 4836, 8344, -3338, -445, 1602
    int64_t X= v.v-(1<<23); // X=v-0.5
    int64_t r= (int64_t(4836)<<24)+8344*X; int64_t X2= (X*X)>>24;
    r+= -3338*X2; X2= (X2*X)>>24;
    r+= -445*X2; X2= (X2*X)>>24;
    r+= 1602*X2;
    uint16_t r2= uint16_t(r>>24);
    if ((addAtEnd&1)!=0) r2= -r2;
    r2+= uint16_t(addAtEnd&0xf0)<<8;
    return Angle(r2);
}

//  N = longitude of the ascending node
//  i = inclination to the ecliptic (plane of the Earth's orbit)
//  w = argument of perihelion
//  a = semi-major axis, or mean distance from Sun
//  e = eccentricity (0=circle, 0-1=ellipse, 1=parabola)
//  M = mean anomaly (0 at perihelion; increases uniformly with time)
struct TOrbitalElements { 
    Angle N; int32_t N2; Angle i, w; int32_t w2; Cartesian a, e; Angle M; int32_t M2;  // Here, the ?2 are perturbations to add to the main value by adding day*?2 to ?
    void updateAt(int32_t d) { N.v= N.v+((N2*d)>>16); w.v= w.v+((w2*d)>>16); M.v= M.v+((M2*d)>>16); } // Update Mean anomaly for the day
};
#define Per(a) int32_t(a*11930464.7f)
static TOrbitalElements const oe[8]={
    // N                             i         w                              a           e           M
    { 0.0f,       Per(0.0f),         0.0f,     282.9404f,  Per(4.70935E-5f),  1.0f,       0.016709f,  356.0470f,  Per(0.9856002585f)}, // sun
    { 48.3313f,   Per(3.24587E-5f),  7.0047f,  29.1241f,   Per(1.01444E-5f),  0.387098f,  0.205635f,  168.6562f,  Per(4.0923344368f)}, // mercury
    { 76.6799f,   Per(2.46590E-5f),  3.3946f,  54.8910f,   Per(1.38374E-5f),  0.723330f,  0.006773f,  48.0052f,   Per(1.6021302244f)}, // Venus
    { 49.5574f,   Per(2.11081E-5f),  1.8497f,  286.5016f,  Per(2.92961E-5f),  1.523688f,  0.093405f,  18.6021f,   Per(0.5240207766f)}, // Mars
    { 100.4542f,  Per(2.76854E-5f),  1.3030f,  273.8777f,  Per(1.64505E-5f),  5.20256f,   0.048498f,  19.8950f,   Per(0.0830853001f)}, // Jupiter
    { 113.6634f,  Per(2.38980E-5f),  2.4886f,  339.3939f,  Per(2.97661E-5f),  9.55475f,   0.055546f,  316.9670f,  Per(0.0334442282f)}, // Saturne
    { 74.0005f,   Per(1.3978E-5f),   0.7733f,  96.6612f,   Per(3.0565E-5f),   19.18171f,  0.047318f,  142.5905f,  Per(0.011725806f)}, // Uranus
    { 131.7806f,  Per(3.0173E-5f),   1.7700f,  272.8461f,  Per(-6.027E-6f),   30.05826f,  0.008606f,  260.2471f,  Per(0.005995147f)} // Nepture
};
#undef Per

// Get sun position compared with earth. unit = AU
static void sunPos(int32_t d, Cartesian &ex, Cartesian &ey)
{
    TOrbitalElements sun= oe[0]; sun.updateAt(d);
    // e*sin(M)*(1+e*cos(M)) is a scalar which is an angle in radians!
    Angle E= sun.M+Angle::fromRad(sun.e*sun.M.sin()*(Cartesian(1<<24)+sun.e*sun.M.cos())); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    // compute the Sun's distance r and its true anomaly v from:
    Cartesian xv = E.cos()-sun.e;
    Cartesian yv = sqrt(Cartesian(1<<24),Cartesian(1<<24), -sun.e, sun.e) * E.sin();
    Angle v = atan2(yv, xv);
    Cartesian rs = sqrt(xv, xv, yv,yv);
    //compute the Sun's true longitude
    Angle lonsun = v + sun.w;
    // Convert lonsun,r to ecliptic rectangular geocentric coordinates xs,ys:
    ex= rs * (-lonsun).cos();
    ey = -rs * (-lonsun).sin();
    // (since the Sun always is in the ecliptic plane, zs is of course zero). xs,ys is the Sun's position in a coordinate system in the plane of the ecliptic. To convert this to equatorial, rectangular, geocentric coordinates, compute:
    // Angle ecl(23.4393f); // ecl angle
    // sx= ex;
    // sy= ey * ecl.cos();
    // sz= ey * ecl.sin();
}
// get an object position in solar system
static void objPos(int32_t d, int8_t ob, Cartesian &x, Cartesian &y, Cartesian &z)
{
    TOrbitalElements o= oe[ob]; o.updateAt(d);
    Angle E= o.M+Angle::fromRad(o.e*o.M.sin()*(Cartesian(1<<24)+o.e*o.M.cos())); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    for (int i=0; i<5; i++) E= E - ( E - Angle::fromRad(o.e*E.sin())-o.M) / (Cartesian(1<<24) - o.e * E.cos() );
    Cartesian xv= o.a * (E.cos()-o.e);
    Cartesian yv= o.a * (sqrt(Cartesian(1<<24),Cartesian(1<<24), -o.e, o.e) * E.sin());
    Angle v= atan2(yv, xv);
    Cartesian r= sqrt(xv,xv, yv,yv);
    x = r * ( o.N.cos() * (v+o.w).cos() - o.N.sin() * (v+o.w).sin() * o.i.cos() );
    y = r * ( o.N.sin() * (v+o.w).cos() + o.N.cos() * (v+o.w).sin() * o.i.cos() );
    z = r * ( (v+o.w).sin() * o.i.sin() );
}

static void objPosFromEarth(int32_t d, int8_t ob, Cartesian &x, Cartesian &y, Cartesian &z, Cartesian sex, Cartesian sey)
{
    // Sun centered, ecliptic centered coordinates...
    // Need to rotate around earth center by ecliptic!
    // so, add earth coordinate, rotate by ecliptic and stay there...
    objPos(d, ob, x, y, z);
    x= x+sex, y= y+sey;
    Angle ecl(23.4393f); // ecl angle (precalc sin/cos!!!)
    Cartesian ty= y * ecl.cos() - z*ecl.sin();
    z= y * ecl.sin() + z*ecl.cos();
    y= ty;
}

int8_t year=23, month=1, day=1;
inline int32_t timeFromDate() { return 367*year - (7*((year+2000L)+((month+9)/12)))/4 + (275*month)/9 + day - (730530L-367*2000L); }
static char planetNames[8][8]= { "Soleil", "Mercure", "Venus", "Mars", "Jupiter", "Saturne", "Uranus", "Neptune" };

void planetPos(int p, int32_t &ra, int32_t &dec) // return planet position. 0 is sun, 1, 2 are mercury, venus, 3-7 are Mars to Neptune
{
    int32_t d= 367*year - (7*((year+2000L)+((month+9)/12)))/4 + (275*month)/9 + day - (730530L-367*2000L);
    Cartesian sex, sey; sunPos(d, sex, sey);
    Cartesian ox, oy, oz; objPosFromEarth(d, p, ox, oy, oz, sex, sey);
    Angle raf= atan2(oy, ox), decf= atan2(oz, sqrt(ox,ox,oy,oy));
    ra= (int32_t(raf.v)*5400)>>12;
    dec= (int32_t(decf.v)*5062)>>8; if (dec>180*3600L) dec= 360*3600L-dec;
}
};
#endif



float atan3(int y, int x) { float v= atan2f(float(y), float(x))*180.0f/3.14159f; if (v<0) v+= 360.0; return v; }

void planetTest()
{
        for (int i=0; i<8; i++)
        {
            //Angle N; int32_t N2; Angle i, w; int32_t w2; Cartesian a, e; Angle M; int32_t M2
            GrandN::TOrbitalElements o= GrandN::oe[i];
            printf("  { %d, %d, %d, %d, %d, %d, %d, %d, %d },\r\n", o.N.v, o.N2, o.i.v, o.w.v, o.w2, o.a.v, o.e.v, o.M.v, o.M2);
        }

    if (false) {
        grandR::year= 10, grandR::month= 4, grandR::day= 19;
        int32_t ra, dec;
        grandR::planetPos(3, ra, dec);
        ATLTRACE2("Venus %d:%d %d:%d\r\n", ra/3600, (ra/60)%60, dec/3600, (dec/60)%60);
    }
    if (false) {
        GrandN::year= 10, GrandN::month= 4, GrandN::day= 19;
        int32_t ra, dec;
        GrandN::planetPos(3, ra, dec);
        ATLTRACE2("Venus %d:%d %d:%d\r\n", ra/3600, (ra/60)%60, dec/3600, (dec/60)%60);
    }

    if (false) // test trig
    {
        {
            int x, y;
            x= 2, y= 1; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
            x= 1, y= 2; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
            x=-1, y= 2; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
            x=-2, y= 1; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
            x=-2, y=-1; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
            x=-1, y=-2; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
            x= 1, y=-2; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
            x= 2, y=-1; ATLTRACE2("y:%d x:%d atanf:%.2f, atan2i:%s\r\n", y, x, atan3(y,x), GrandN::atan2(y<<24, x<<24).str());
        }
        for (int x=-5; x<10; x+= 2)
            for (int y=-5; y<10; y+= 2)
            {
                float t1= atan2f(float(y), float(x))*180.0f/3.14159f; if (t1<0) t1+= 360.0;
                GrandN::Angle a2= GrandN::atan2(y<<24, x<<24);
                float t2= a2.fval();
                float d= t1-t2;
                if (fabs(d)>1)
                    ATLTRACE2("atanf(%d, %d):%.2f, atan2i(%d,%d):%s\r\n", y, x, t1, y, x, a2.str());
            }
        for (int i=0; i<7; i++)
        {
            ATLTRACE2("i:%d cosf(i):%f cosh(i):%s\r\n", i, cosf(float(i)), GrandN::Angle::fromRad(i<<24).cos().str());
        }
    }

    int32_t ra, dec;
    grandR::year= -10, grandR::month= 4, grandR::day= 19;
    grandR::planetPos(1, ra, dec);
    printf("mercury at 1990/4/19 %d:%d %d:%d\r\n\r\n", ra/3600, (ra/60)%60, dec/3600, (dec/60)%60);

    for (grandR::month=1; grandR::month<=12; grandR::month++)
    {
        grandR::year= 23, grandR::day= 1;
        int32_t ra1, dec1; grandR::planetPos(4, ra1, dec1);
        printf("jupiterf at %d/%d/%d %d:%d %d:%d\r\n", grandR::year, grandR::month, grandR::day, ra1/3600, (ra1/60)%60, dec1/3600, (dec1/60)%60);

        //GrandN::month= grandR::month, GrandN::year= 90, GrandN::day= 1;
        //int32_t ra3, dec3; GrandN::planetPos(5, ra3, dec3);
        //printf("Saturnei at %d/%d/%d %d:%d %d:%d\r\n", GrandN::year, GrandN::month, GrandN::day, ra3/3600, (ra3/60)%60, dec3/3600, (dec3/60)%60);

        Planets::month= grandR::month, Planets::year= 23, Planets::day= 1;
        int32_t ra2, dec2; Planets::planetPos(4, ra2, dec2);
        printf("jupiteri at %d/%d/%d %d:%d %d:%d\r\n", Planets::year, Planets::month, Planets::day, ra2/3600, (ra2/60)%60, dec2/3600, (dec2/60)%60);
    }
    Planets::month= 1;
}
#endif

struct { int c, ngc; } const cad[] {
{1, 188},
{2, 40},
{3, 4236},
{4, 7023},
{5, 7817}, 
{6, 6543},
{7, 2403},
{8, 559},
{9, 7818}, 
{10, 663},
{11, 7635},
{12, 6946},
{13, 457},
{14, 869},
{15, 6826},
{16, 7243},
{17, 147},
{18, 185},
{19, 7819},
{20, 7000},
{21, 4449},
{22, 7662},
{23, 891},
{24, 1275},
{25, 2419},
{26, 4244},
{27, 6888},
{28, 752},
{29, 5005},
{30, 7331},
{31, 7820},
{32, 4631},
{33, 6992},
{34, 6960},
{35, 4889},
{36, 4559},
{37, 6885},
{38, 4565},
{39, 2392},
{40, 3626},
{41, 7821},
{42, 7006},
{43, 7814},
{44, 7479},
{45, 5248},
{46, 2261},
{47, 6934},
{48, 2775},
{49, 2237},
{50, 2244},
{51, 7822},
{52, 4697},
{53, 3115},
{54, 2506},
{55, 7009},
{56, 246},
{57, 6822},
{58, 2360},
{59, 3242},
{60, 4038},
{61, 4039},
{62, 247},
{63, 7293},
{64, 2362},
{65, 253},
{66, 5694},
{67, 1097},
{68, 6729},
{69, 6302},
{70, 300},
{71, 2477},
{72, 55},
{73, 1851},
{74, 3132},
{75, 6124},
{76, 6231},
{77, 5128},
{78, 6541},
{79, 3201},
{80, 5139},
{81, 6352},
{82, 6193},
{83, 4945},
{84, 5286},
{85, 7823}, 
{86, 6397},
{87, 1261},
{88, 5823},
{89, 6087},
{90, 2867},
{91, 3532},
{92, 3372},
{93, 6752},
{94, 4755},
{95, 6025},
{96, 2516},
{97, 3766},
{98, 4609},
{99, 7824},
{100, 7825},
{101, 6744},
{102, 7826},
{103, 2070},
{104, 362},
{105, 4833},
{106, 104},
{107, 6101},
{108, 4372},
{109, 3195}};
//void makeCadList()
//{
//    for (int i=0; i<sizeof(cad)/8; i++)
//    {
//        for (int j=0; j<sizeof(NGCs)/sizeof(NGCs[0]); j++)
//            if (cad[i].ngc==NGCs[j].id())
//            {
//                printf("%d, ", j);
//                goto next;
//            }
//        printf("\r\nError for %d ngc %d\r\n", i, cad[i].ngc);
//        //printf("  NgC(%d, r(), r()),\r\n", cad[i].ngc);
//    next:;
//    }
//}

static uint16_t const MessierToNgc[] = {1952, 7089, 5272, 6121, 5904, 6405, 6475, 6523, 6333, 6254, 6705, 6218, 6205, 6402, 7078, 6611, 6618, 6613, 6273, 6514, 6531, 6656, 6494, 6603, 7815, 6694, 6853, 6626, 6913, 7099, 224, 221, 598, 1039, 2168, 1960, 2099, 1912, 7092, 7816, 2287, 1976, 1982, 2632, 1432, 2437, 2422, 2548, 4472, 2323, 5194, 7654, 5024, 6715, 6809, 6779, 6720, 4579, 4621, 4649, 4303, 6266, 5055, 4826, 3623, 3627, 2682, 4590, 6637, 6681, 6838, 6981, 6994, 628, 6864, 650, 1068, 2068, 1904, 6093, 3031, 3034, 5236, 4374, 4382, 4406, 4486, 4501, 4552, 4569, 4548, 6341, 2447, 4736, 3351, 3368, 3587, 4192, 4254, 4321, 5457, 5457, 581, 4594, 3379, 4258, 6171, 3556, 3992, 205};


void makeMessierCadListTen()
{
    uint16_t buf= 0; int bufsize=0; 
    printf("static uint8_t const MessierToNgc10[] PROGMEM = {");
    for (int i=0; i<sizeof(MessierToNgc)/sizeof(MessierToNgc[0]); i++)
    {
        for (int j=0; j<sizeof(NGCs)/sizeof(NGCs[0]); j++)
            if (MessierToNgc[i]==NGCs[j].id())
            {
                buf= buf|(j<<bufsize); bufsize+= 10;
                goto next1;
            }
        printf("\r\nError for %d Messier %d\r\n", i, MessierToNgc[i]);
        //printf("  NgC(%d, r(), r()),\r\n", cad[i].ngc);
    next1:;
        while (bufsize>=8) printf("%d, ", buf&255), buf>>=8, bufsize-= 8;
    }
    while (bufsize>0) printf("%d, ", buf&255), buf>>=8, bufsize-= 8;
    printf("};\r\nuint16_t const nbMessier= %d;\r\n", int(sizeof(MessierToNgc)/sizeof(MessierToNgc[0])));

    buf= 0; bufsize=0; 
    printf("static uint8_t const CadwellToNgc10[] PROGMEM = {");
    for (int i=0; i<sizeof(cad)/8; i++)
    {
        for (int j=0; j<sizeof(NGCs)/sizeof(NGCs[0]); j++)
            if (cad[i].ngc==NGCs[j].id())
            {
                buf= buf|(j<<bufsize); bufsize+= 10;
                goto next;
            }
        printf("\r\nError for %d ngc %d\r\n", i, cad[i].ngc);
        //printf("  NgC(%d, r(), r()),\r\n", cad[i].ngc);
    next:;
        while (bufsize>=8) printf("%d, ", buf&255), buf>>=8, bufsize-= 8;
    }
    while (bufsize>0) printf("%d, ", buf&255), buf>>=8, bufsize-= 8;
    printf("};\r\nuint16_t const nbCadwell= %d;\r\n", int(sizeof(cad)/8));
}

//void displayNgc()
//{
//    for (int i=0; i<nbNgc+2; i++)
//        printf("  {%d, { %d, %d, %d}},\r\n", NGCs[i].ngcid, NGCs[i].v[0], NGCs[i].v[1], NGCs[i].v[2]);
//}

void makeNgcDiff() 
{
    int lastid=0; int maxDiff= 0;
    for (int i= 0; i<sizeof(NGCs)/sizeof(NGCs[0]); i++)
    {
        int id= NGCs[i].id();
        if ((id-lastid>255) || (id-lastid)<0)
            printf("ERROR at %d\r\n", i);
        if (id-lastid>maxDiff) maxDiff= id-lastid;
        int32_t ra, dec; NGCs[i].pos(ra, dec);
        uint32_t v= (ra*4096/(24*3600)) + uint32_t(((int64_t(dec+90*3600)*4096/(180*3600))<<12));
        printf("  { %d, {%d, %d, %d}},\r\n", id-lastid, v&0xff, (v>>8)&0xff, (v>>16)&0xff);
        lastid= id;
    }
    printf("uint16_t const NbNGcs= %d;\r\n", int(sizeof(NGCs)/sizeof(NGCs[0])-12));
    printf("// max diff= %d;\r\n", maxDiff);
}








namespace Planets2 {
// I tried doing a version where the Cartesian and Angle classes where replaced by native ints, but it was even bigger!
//////////////////////////////////////////////
// Planetary stuff
class Cartesian { // All is in AU, Uranus is 19 AU away from earth, so having 5 bits for IP should be enough!
public:
    int32_t v;
    Cartesian() { }
    Cartesian(int32_t v): v(v) { }
    Cartesian operator+(Cartesian v2) const { return Cartesian(v+v2.v); }
    Cartesian operator-(Cartesian v2) const { return Cartesian(v-v2.v); }
    Cartesian operator-() const { return Cartesian(-v); }
    Cartesian operator*(Cartesian v2) const { return Cartesian((int64_t(v)*v2.v)>>24); }
    Cartesian operator/(Cartesian v2) const { return Cartesian((int64_t(v)<<24)/v2.v); }
    bool operator>(Cartesian v2) const { return v>v2.v; }
    bool operator<(Cartesian v2) const { return v>v2.v; }
};

    Cartesian sqrt(Cartesian a1, Cartesian a2, Cartesian b1, Cartesian b2) // return sqrt a1*a2+b1*b2
    {
        int64_t v= int64_t(a1.v)*a2.v + int64_t(b1.v)*b2.v;
        if (v<0) return Cartesian(0);
        uint32_t t= uint32_t(v>>48); uint8_t bits= 0; while (t!=0) { bits++; t>>=1; }
        int64_t x= v>>((bits>>1)+24);
        for (int8_t i=10; --i>=0; ) x= (x+v/x)>>1;
        return Cartesian(int32_t(x));
    }

    // sinTable[i]=65536*sin(i in degree * 128/180)
    static uint16_t const sinTable[65]={0, 1608, 3215, 4821, 6423, 8022, 9616, 11204, 12785, 14359, 15923, 17479, 19024, 20557, 22078, 23586, 25079, 26557, 28020, 29465, 30893, 32302, 33692, 35061, 36409, 37736, 39039, 40319, 41575, 42806, 44011, 45189, 46340, 47464, 48558, 49624, 50660, 51665, 52639, 53581, 54491, 55368, 56212, 57022, 57797, 58538, 59243, 59913, 60547, 61144, 61705, 62228, 62714, 63162, 63571, 63943, 64276, 64571, 64826, 65043, 65220, 65358, 65457, 65516, 65535};

class Angle { public:
    uint16_t v;
    Angle() {}
    Angle(uint16_t v): v(v) { }
    static Angle fromRad(Cartesian v) { return Angle((v.v*10430LL)>>24); } // v is rad<<24 and I want [0..256<<8[ so (0x8000/pi=10430)>>24
    Angle operator+(Angle v2) const { return Angle(v+v2.v); }
    Angle operator-(Angle v2) const { return Angle(v-v2.v); }
    Angle operator-() const { return Angle(-v); }
    Angle operator*(int32_t v2) const { return Angle(v*v2); }
    Angle operator/(Cartesian v2) const { return Angle(uint16_t((int64_t(v)<<24)/v2.v)); }

    // sin (a+b) = sin(a)*cos(b) + cos(a)*sin(b)
    // if b is small cos(b)=1 and  sin(b)=b
    // sin (a+b) = sin(a) + b*cos(a)
    static Cartesian sinPrecision(uint16_t a) // a: [0, 65536[ correspond to [0,360deg[. Returns sin(a)*(1<<24)
    {
        int32_t s, c;
        uint8_t a2= a>>8;
        if (a2<0x40) s= sinTable[a2], c= sinTable[64-a2];
        else if (a2<0x80) s= sinTable[128-a2], c= -int32_t(sinTable[a2-64]);
        else if (a2<0xC0) s= -int32_t(sinTable[a2-128]), c= -int32_t(sinTable[192-a2]);
        else s= -int32_t(sinTable[uint8_t(0-a2)]), c= sinTable[a2-192];
        int32_t t= (a&0xff)*6; // a&0xff = fp(a)*256 in [0..256[ so to get to fp(a)*256^2 in [0..2pi[, multiply by 6
        t= (t*c)>>8; // so, t has a factor of 1<<16 and c a factor of <<16. to get to our factor <<24, we need to shift by >>8
        return (s<<8)+t;
    }
    Cartesian sin() const { return sinPrecision(v); }
    Cartesian cos() const { return sinPrecision(v+0x4000); }
};

Angle atan2(Cartesian y, Cartesian x)
{ 
    if (x.v==0) // x=0, can be +/-90 depending on y
    {
        if (y.v==0) return Angle(0); // error!!!
        if (y.v>0) return Angle(uint16_t(0x4000));
        return Angle(uint16_t(0xC000));
    }
    // We will calculate atan(x in [0..0.5]).
    // But it this only correspond to an eight of the circle... so we will need to correct the final answer by potentially negating it and adding a vlaue...
    // the sign of x and y as well as the order of their magnitude indicates the octan that we are in
    // sx sy x<y pos      finalOp
    //  +  +  f  0-45     r
    //  +  +  t  45-90    90-r
    //  +  -  f  135-180  180-r
    //  +  -  t  90-135   90+r
    //  -  +  f  270-315  270+r
    //  -  +  t  315-360  360-r
    //  -  -  f  180-225  180+r
    //  -  -  t  225-270  270-r
    uint8_t const changes[8]= { 0x00, 0x41, 0x81, 0x40, 0x01, 0xC0, 0x80, 0xC1 };
    uint8_t addAtEnd= changes[((y.v>>29)&0b100)+((x.v>>30)&0b10)+(abs(x.v)>abs(y.v)?0:1)];
    // now calculate arctan(y/x)
    // Since, atan(a) = 90-atan(1/a)
    // make sure that v=y/x is in [0..1] and if not, mark the need for neg and update addAtEnd..
    Cartesian v; if (abs(x.v)<abs(y.v)) v= x/y; else v= y/x; if (v.v<0) v=-v;
    // approximate arctan(x) by a 4th order polynomial centered at x=0.5 (taylor series)...
    // 0.463647609001+0.8*(x-0.5)-0.32*(x-0.5)^2-4.26666666667e-2*(x-0.5)^3+0.1536*(x-0.5)^4
    // But the coefitians here are for a result in radians.. and we want a result in our angle unit, so multiply by 0x8000/pi=10430
    // so the coefs becomes: 4836, 8344, -3338, -445, 1602
    int64_t X= v.v-(1LL<<23); // X=v-0.5
    int64_t r= (int64_t(4836)<<24)+8344*X; int64_t X2= (X*X)>>24;
    r+= -3338*X2; X2= (X2*X)>>24;
    r+= -445*X2; X2= (X2*X)>>24;
    r+= 1602*X2;
    uint16_t r2= uint16_t(r>>24);
    if ((addAtEnd&1)!=0) r2= -r2;
    r2+= uint16_t(addAtEnd&0xf0)<<8;
    return Angle(r2);
}

//  N = longitude of the ascending node
//  i = inclination to the ecliptic (plane of the Earth's orbit)
//  w = argument of perihelion
//  a = semi-major axis, or mean distance from Sun
//  e = eccentricity (0=circle, 0-1=ellipse, 1=parabola)
//  M = mean anomaly (0 at perihelion; increases uniformly with time)
//#define Per(a) int32_t(a*11930464.7f)
//#define a(v) { uint16_t(v*256.0f/360.0f*256.0f) }
//#define c(v) { int32_t(v*(1<<24)) }
struct { uint16_t N; uint16_t N2; uint16_t i, w; int16_t w2; int32_t a, e; uint16_t M; int32_t M2; } const oe[8]={
    //N        N2   i     w        w2   a            e          M        M2
    { 0UL,     0,   0,    51507UL, 561, 16777216LL,  280330LL,  64816UL, 11758669LL },          //    { a(0.0f),       Per(0.0f),         a(0.0f),     a(282.9404f),  Per(4.70935E-5f),  c(1.0f),       c(0.016709f),  a(356.0470f),  Per(0.9856002585f)}, // sun
    { 8798UL,  387, 1275, 5301UL,  121, 6494427LL,   3449982LL, 30702UL, 48823452LL },   //    { a(48.3313f),   Per(3.24587E-5f),  a(7.0047f),  a(29.1241f),   Per(1.01444E-5f),  c(0.387098f),  c(0.205635f),  a(168.6562f),  Per(4.0923344368f)}, // mercury
    { 13959UL, 294, 617,  9992UL,  165, 12135464LL,  113632LL,  8739UL,  19114158LL },    //    { a(76.6799f),   Per(2.46590E-5f),  a(3.3946f),  a(54.8910f),   Per(1.38374E-5f),  c(0.723330f),  c(0.006773f),  a(48.0052f),   Per(1.6021302244f)}, // Venus
    { 9021UL,  251, 336,  52156UL, 349, 25563242LL,  1567075LL, 3386UL,  6251811LL },    //    { a(49.5574f),   Per(2.11081E-5f),  a(1.8497f),  a(286.5016f),  Per(2.92961E-5f),  c(1.523688f),  c(0.093405f),  a(18.6021f),   Per(0.5240207766f)}, // Mars
    { 18287UL, 330, 237,  49857UL, 196, 87284472LL,  813661LL,  3621UL,  991246LL },     //    { a(100.4542f),  Per(2.76854E-5f),  a(1.3030f),  a(273.8777f),  Per(1.64505E-5f),  c(5.20256f),   c(0.048498f),  a(19.8950f),   Per(0.0830853001f)}, // Jupiter
    { 20691UL, 285, 453,  61784UL, 355, 160302112LL, 931907LL,  57702UL, 399005LL },   //    { a(113.6634f),  Per(2.38980E-5f),  a(2.4886f),  a(339.3939f),  Per(2.97661E-5f),  c(9.55475f),   c(0.055546f),  a(316.9670f),  Per(0.0334442282f)}, // Saturne
    { 13471UL, 166, 140,  17596UL, 364, 321815680LL, 793864LL,  25957UL, 139894LL },   //    { a(74.0005f),   Per(1.3978E-5f),   a(0.7733f),  a(96.6612f),   Per(3.0565E-5f),   c(19.18171f),  c(0.047318f),  a(142.5905f),  Per(0.011725806f)}, // Uranus
    { 23989UL, 359, 322,  49670UL, -71, 504293920LL, 144384LL,  47376UL, 71524LL },    //    { a(131.7806f),  Per(3.0173E-5f),   a(1.7700f),  a(272.8461f),  Per(-6.027E-6f),   c(30.05826f),  c(0.008606f),  a(260.2471f),  Per(0.005995147f)} // Nepture
};
struct TOrbitalElements { 
    Angle N, i, w; Cartesian a, e; Angle M; // Here, the ?2 are perturbations to add to the main value by adding day*?2 to ?
    void updateAt(uint8_t obj, int32_t d) // Update Mean anomaly for the day
    { 
        N.v= oe[obj].N+((oe[obj].N2*d)>>16); w.v= oe[obj].w+((oe[obj].w2*d)>>16); M.v= oe[obj].M+((oe[obj].M2*d)>>16);
        i.v= oe[obj].i; a.v= oe[obj].a; e.v= oe[obj].e; 
    } 
};

static void objPos(int32_t d, int8_t ob, Cartesian &x, Cartesian &y, Cartesian &z)
{
    // get an object position in Sun centered, ecliptic centered coordinates...
    TOrbitalElements o; o.updateAt(ob, d);
    Angle E= o.M+Angle::fromRad(o.e*o.M.sin()*(Cartesian(1LL<<24)+o.e*o.M.cos())); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    if (ob!=0) 
        for (int i=0; i<5; i++) 
        {
            int64_t t= (int64_t(E.v-o.M.v)<<24) - (((int64_t(o.e.v))*E.sin().v)>>24)*10430LL;
            E= E - Angle(t / (Cartesian(1L<<24) - o.e * E.cos() ).v); // not used for sun...
        }
    Cartesian xv= o.a * (E.cos()-o.e);
    Cartesian yv= o.a * (sqrt(Cartesian(1LL<<24),Cartesian(1LL<<24), -o.e, o.e) * E.sin());
    //printf("vx, vy: %d %d\r\n", xv.v, yv.v);
    Angle v= atan2(yv, xv);
    Cartesian r= sqrt(xv,xv, yv,yv);
    Angle lon = v + o.w; //compute the longitude. i and N are actually 0 for sun... but we are size optimizing here...
    x = r * ( o.N.cos() * lon.cos() - o.N.sin() * lon.sin() * o.i.cos() );
    y = r * ( o.N.sin() * lon.cos() + o.N.cos() * lon.sin() * o.i.cos() );
    z = r * ( lon.sin() * o.i.sin() ); 
    //printf("x, y, z: %d %d %d\r\n", x.v, y.v, z.v);
}

int8_t year=23, month=1, day=1;

static char planetNames[8][8]= { "Soleil", "Mercure", "Venus", "Mars", "Jupiter", "Saturne", "Uranus", "Neptune" };

void planetPos(int p, int32_t &ra, int32_t &dec) // return planet position. 0 is sun, 1, 2 are mercury, venus, 3-7 are Mars to Neptune
{
    int32_t d= 367*year - (7*((year+2000L)+((month+9)/12)))/4 + (275*month)/9 + day - (730530L-367*2000L);
    Cartesian sex, sey, ox, oy, oz, t;
    objPos(d, 0, sex, sey, t);  // earth position (sun is -earth)
    objPos(d, p, ox, oy, oz);   // object position
    ox= ox+sex, oy= oy+sey;    // object position compared with earth... sunz=0...
    // Need to rotate around earth center by ecliptic!
    Cartesian sinecl(6673596), cosecl(15392794); // sin and cos of ecliptic
    t= oy*cosecl - oz*sinecl; oz= oy*sinecl + oz*cosecl; oy= t;    // ecliptic rotation...
    Angle raf= atan2(oy, ox), decf= atan2(oz, sqrt(ox,ox,oy,oy));  // cartesian to spherical transform...
    ra= (int32_t(raf.v)*5400)>>12; dec= (int32_t(decf.v)*5062)>>8; // to my fixed point system
    if (dec>180*3600L) dec= dec-360*3600L; // modulo...
}
};

void planetTest2(int p)
{
    int er1= 0, er2= 0;
    for (Planets::month=0; Planets::month<=12; Planets::month++)
    {
        int32_t ra2, dec2, ra, dec, er; 

        grandR::year= 23, grandR::day= 1; grandR::month= Planets::month; grandR::planetPos(p, ra, dec);
        printf("  %sr at %d/%d/%d %d:%d %d:%d\r\n", Planets::planetNames[p], Planets::year, Planets::month, Planets::day, ra/3600, (ra/60)%60, dec/3600, (dec/60)%60);

        Planets2::year= 23, Planets2::day= 1; Planets2::month= Planets::month; Planets2::planetPos(p, ra2, dec2);
        er= abs(ra-ra2)+abs(dec-dec2); er1+= er;
        printf("  %s2 at %d/%d/%d %d:%d %d:%d er:%d\r\n", Planets::planetNames[p], Planets::year, Planets::month, Planets::day, ra2/3600, (ra2/60)%60, dec2/3600, (dec2/60)%60, er);

        Planets::year= 23, Planets::day= 1; Planets::planetPos(p, ra2, dec2);
        er= abs(ra-ra2)+abs(dec-dec2); er2+= er;
        printf("  %s  at %d/%d/%d %d:%d %d:%d er:%d\r\n", Planets::planetNames[p], Planets::year, Planets::month, Planets::day, ra2/3600, (ra2/60)%60, dec2/3600, (dec2/60)%60, er);
    }
    printf("er ino:%d other:%d\r\n", er2, er1);
}

float const MPI= 3.14159265358979323846264338327f;
void trigTest()
{
    double maxs= -1;
    double av= 0;
    for (int i=0; i<65536; i++)
    {
        double s = double(Planets::sin(i)) / (1<<24);
        double s2= sin((i*2*MPI)/65536);
        s= abs(s2-s);
        av+= s;
        if (s>maxs) maxs= s;
    }
    printf("max sin  error: %f average:%f\r\n", maxs, av/65536);

    maxs= -1; av= 0;
    for (int i=0; i<65536; i++)
    {
        double s = double(Planets2::Angle::sinPrecision(i).v) / (1<<24);
        double s2= sin((i*2*MPI)/65536);
        s= abs(s2-s); av+= s; if (s>maxs) maxs= s;
    }
    printf("max sin2 error: %f average:%f\r\n", maxs, av/65536);

    maxs= -10; av= 0; int nb=0;
    for (int i=0; i<1<<24; i+= 1)
    {
        double a1= atan2(float(i)/(1<<24), 1.0f); if (a1<0) a1+= 2*MPI;
        double a2= Planets::atan2(i, 1<<24); a2*=2*MPI/65536.0f;
        double s= abs(a2-a1);
        if (s>6.0)
        {
            printf("error > 6 %d\r\n", i);
            Planets::atan2(i, 1<<24);
        }
        av+= s; if (s>maxs) maxs= s; nb++;
    }
    printf("max atan2 error: %f average:%f\r\n", maxs, av/nb);

}

static void makeCSharp() // SCharp catalogs...
{
    printf("       public static readonly CNgc[] Ngc= new CNgc[] { \r\n");
    for (int i=0; i<nbNgc2; i++)
    { int ra, dec; NGCs[i].pos(ra, dec);
      printf("           new CNgc(%d, %d, %d),\r\n", NGCs[i].id(), ra, dec);
    }
    printf("       };\r\n       public static readonly CNgc[] Messier= new CNgc[] { \r\n");
    for (int i=0; i<nbMessier; i++)
    { int ra, dec; getRaDecPos(NgcDiff[read10bits(MessierToNgc10, i)].radec, ra, dec);
      printf("           new CNgc(%d, %d, %d),\r\n", i+1, ra, dec);
    }
    printf("       };\r\n       public static readonly CNgc[] Cadwell= new CNgc[] { \r\n");
    for (int i=0; i<nbCadwell; i++)
    { 
        int ra, dec; getRaDecPos(NgcDiff[read10bits(CadwellToNgc10, i)].radec, ra, dec);
        printf("           new CNgc(%d, %d, %d),\r\n", i+1, ra, dec);
    }
    printf("       };\r\n       public static readonly CStar[] Stars= new CStar[] { \r\n");
    for (int i=0; i<sizeof(stars2)/sizeof(stars2[0]); i++)
    {
        int ra=stars2[i].ra*3600, dec=stars2[i].dec*3600;
        printf("           new CStar(\"%s\", %d, %d),\r\n", stars2[i].n, ra, dec);
    }
    printf("       };\r\n");
}

class TP { public:
    TP() { // comment/uncomment what you want to display some stuff at startup
        //createStarsStruct();
        //displayImageStruct();
        //verifyStars();
        //DipStarsFromStars();
        //planetTest();
        //displayNgc();
        //makeCadList();
        //makeMessierCadListTen();
        //makeNgcDiff();
        //for (int i=1; i<8; i++) planetTest2(i);
        //trigTest();
        //makeCSharp();
    }
} p;



uint8_t onecrc(uint8_t crc, uint8_t currentByte)
{
    for (uint8_t j=0; j<8; j++) 
    {
        if ((crc >> 7) ^ (currentByte&0x01)) // update CRC based result of XOR operation
            crc = (crc << 1) ^ 0x07;
        else crc = (crc << 1);
        currentByte>>= 1;
    }
    return crc;
}

uint8_t swuart_calcCRC(uint8_t crc, uint8_t const * s, uint8_t l)
{
    // CRC located in last byte of message
    while (l--!=0) // Execute for all bytes of a message
    {
        uint8_t currentByte = *s++; // Retrieve a byte to be sent from Array
        for (uint8_t j=0; j<8; j++) 
        {
            if ((crc >> 7) ^ (currentByte&0x01)) // update CRC based result of XOR operation
                crc = (crc << 1) ^ 0x07;
            else crc = (crc << 1);
            currentByte>>= 1;
        }
    }
    return crc;
}
uint8_t rev(uint8_t a)
{
    uint8_t r=0;
    for (int i=0; i<8; i++) if ((a&(0x80>>i))!=0) r|=1<<i;
    return r;
}

int8_t crc8_table[256]= {};
static void CalculateTable_CRC8(int8_t generator)
{
    /* iterate over all byte values 0 - 255 */
    for (int dividend = 0; dividend < 256; dividend++)
    {
        int8_t currByte = rev(dividend);
        /* calculate the CRC-8 value for current byte */
        for (byte bit = 0; bit < 8; bit++)
        {
            if ((currByte & 0x80) != 0)
            {
                currByte <<= 1;
                currByte ^= generator;
            }
            else
            {
                currByte <<= 1;
            }
        }
        /* store CRC value in lookup table */
        crc8_table[dividend] = currByte;
    }
} uint8_t crc8(uint8_t crc, uint8_t const *buffer, uint8_t len)
{
    while (len--) crc= crc8_table[crc^*buffer++];
    return crc;
}
class Ta {
public:
Ta()
{ return;
    // struct { uint8_t c[512],b[512]; int l; } l[256]; memset(l, 0, sizeof(l));
    // uint8_t C[256*256];
    // for (int c=0; c<256; c++)
    //     for (int b=0; b<256; b++)
    //     {
    //         uint8_t crc= onecrc(c, b);
    //         C[c*256+b]= crc;
    //         l[crc].c[l[crc].l]= c; l[crc].b[l[crc].l]= b; l[crc].l++;
    //     }
    // for (int i=0; i<256; i++)
    // {
    //     if (l[i].l!=256) printf("crc %d, %d elements", i, l[i].l);
    //     else
    //       for (int j=0; j<256; j++) printf("%02X:%02X:%02X ", l[i].c[j], l[i].b[j], l[i].c[j]^rev(l[i].b[j]));
    //     printf("\r\n");
    // }
    uint8_t r[1000]; for (int i=0; i<1000; i++) r[i]= rand();
    for (int g=0; g<256; g++)
    {
        CalculateTable_CRC8(g);
        for (int i=0; i<256; i++)
        {
            uint8_t crc1= swuart_calcCRC(0, r, i);
            uint8_t crc2= crc8(0, r, i);
            if (crc1!=crc2)
            {
                printf(" crc diffeer at %d %x %x\r\n", i, crc1, crc2); goto nope;
            } else printf("%x=%x\r", crc1, crc2);
        }
        printf("Works\r\n");
        for (int i=0; i<256; i++) printf("%d, ", crc8_table[i]);
        nope:;
    }
    printf("\r\n\r\n");
} 
} aaaa;