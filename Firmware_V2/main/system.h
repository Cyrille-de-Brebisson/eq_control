#define _CRT_SECURE_NO_WARNINGS // allows windows compilation
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "hal.h"
#include "motor.h"
#include "scope.h"
#include "ui.h"

extern CMotor motor1, motor2, motorFocus;
extern CScope scope;
extern CI2C I2C;

void HWSet(int motor, double pos); // set motor n to pos
extern void initSystem();

#pragma pack(push, 1) // This gets sent to PC so we need to ensure consistent data strucutre...
class CSavedData { public:
    struct { uint32_t maxPos, maxSpd, msToSpd; } ra, dec;
    int32_t timeComp; // drift adjustment between internal clock and real time...
    int32_t Latitude, Longitude; 
    uint16_t Altitude, FocalLength, Diameter_mm, Area_cm2; 
    uint16_t FocStepdum, focMaxStp, focMaxSpd, focAcc; // FocStepdum is in thenth of microns..
    uint16_t decBacklash, raAmplitude; // raAmplitude is the amplitude of the RA movement in degree! allows to pass the meridian... decBacklash is in steps...
    uint8_t guideRateRA, guideRateDec; // used by driver only... in thenth of arc"/s (75 = 7.5"/s) (used to be in steps/s)
    uint8_t invertAxes;  // 1 bit per motor... For some reason dec, ra and then focus... dec is before RA... don't ask...
                         // bit 4: check on goto bellow horizon
    uint8_t guidingBits; // 0:ra pier invert, 1:ra invert, 2:ra stop, 3:dec pier invert, 4:dec invert, 5:dec stop (these are server side stuff)
                         // 6: AP mode (true if access point mode, esp32 only)
                         // 7: NoAutoFlip
    int16_t raBacklash; uint16_t focBacklash;
    uint8_t _raSettle; // not used anymore... replaced by negative ra backlash...
    uint8_t extra[11]; // for future...
    uint8_t crc= -1; // avoid 00 init...
    char wifi[32], wifiP[32]; // this comes from esp32 wifi and will probably be repurposed...
    uint32_t ipaddr= 0;       // WARNING!!! not 32 bit alligned!!!
    uint8_t calcCrc() { uint8_t sum= 0; uint8_t *d= (uint8_t*)this; while (d<&crc) sum+= *d++; return sum; }
    void init() // Placed here rather than in the .cpp to help keep alligned with changes in the structure...
    {   // default values for EQ3-2
        memset(this, 0, sizeof(*this));
        ra={100*200*256, 200*256*12, 500 }; 
        dec={100*200*256, 200*256*12, 500}; 
        timeComp= 0; Latitude=45*36000, Longitude=4*36000, Altitude=1040, FocalLength=250, Diameter_mm=125, Area_cm2= 90, FocStepdum= 31; 
        focMaxStp=65532; focMaxSpd=200*4; focAcc= 200;
        raAmplitude= 185; // plus 10mn on each side...
        guideRateRA= uint8_t(ra.maxPos*7/(360*3600)); // should not overflow unless maxPos is in hundreds of millions...
        guideRateDec= uint8_t(dec.maxPos*7/(360*3600));
    } 
    bool testCrc() { return crc==calcCrc(); }
    void load(); // load from eprom, verify checksum and if bad, re-init
    void save();
    void applySettings(); // init the 3 motors based on saved data..
};
extern CSavedData savedData;
#pragma pack(pop)
