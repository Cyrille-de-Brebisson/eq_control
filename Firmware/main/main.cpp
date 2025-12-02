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

class CMyTelescope : public CTelescope { public: CMyTelescope(int id): CTelescope(id, "CdB 500 drivers", "1.0", "CdB500", "CdB500") { }
protected:
    bool canpulseguide() override { return true; } // Indicates whether the telescope can be pulse guided
    TAlpacaErr pulseguide(int dir, int length) override  // Moves the scope in the given $Direction for the given $Duration (ms). . 0: north, 1: south, 2: east, 3: west
    { 
        stopMovingOnKeyRelease= false;
        if (dir==0 && guideratedeclination>0) { MDecOn(); decGuiding= true; float r= guideratedeclination*3600.0f*CSavedData::savedData.dec.maxPos/360.0f; MDec.guide(int32_t(length*r/1000.0f), uint32_t(r)); return ALPACA_OK; }
        if (dir==1 && guideratedeclination>0) { MDecOn(); decGuiding= true; float r= guideratedeclination*3600.0f*CSavedData::savedData.dec.maxPos/360.0f; MDec.guide(-int32_t(length*r/1000.0f), uint32_t(r)); return ALPACA_OK; }
        if (dir==2 && guideraterightascension>0) { float r= guideraterightascension*3600.0f*CSavedData::savedData.dec.maxPos/360.0f; MRa.guide(int32_t(length*r/1000.0f), uint32_t(r)); return ALPACA_OK; }
        if (dir==3 && guideraterightascension>0) { float r= guideraterightascension*3600.0f*CSavedData::savedData.dec.maxPos/360.0f; MRa.guide(-int32_t(length*r/1000.0f), uint32_t(r)); return ALPACA_OK; }
        return ALPACA_ERR_INVALID_VALUE;
    } 
    bool ispulseguiding() override  { return decGuiding || MRa._guide!=0; }; // Indicates whether the telescope is currently executing a PulseGuide command

    bool get_tracking() override { return MRa.NextUncountedSteps!=0; } // Indicates whether the telescope is tracking.
    TAlpacaErr set_tracking(bool v) override 
    { 
        if (!v) CSavedData::savedData.initUncountedStep2(0); 
        else CSavedData::savedData.initUncountedStep2(sideralSpeeds[trackingrate]);
        return ALPACA_OK; } // Enables or disables telescope $Tracking.
         // 0: sideral, 1: lunar, 2: solar, 3: king (15.0369 arc"/s)
    TAlpacaErr set_trackingrate(int v) override { if (v>=0 && v<=3) { trackingrate= v; set_tracking(get_tracking()); } return ALPACA_OK; } // Sets the mount's $TrackingRate.
    int get_trackingrate() override { if (MRa.deltaBetweenUncountedSteps==0) return trackingrate; return MRa.sideralMove-1; } // Gets the mount's $TrackingRate.

    bool slewing() override { return MDec.isMoving()||MRa.isMoving(); }
    TAlpacaErr abortslew() override { savedGotoForFlip.flipFlags= 0; MDec.stop(); MRa.stop(); return ALPACA_OK; }; // Immediatley stops a slew in progress.
    TAlpacaErr slewtocoordinatesasync(float ra, float dec) override { goTo(int32_t(ra*3600.0f), int32_t(dec*3600.0f)); return ALPACA_OK; } // Asynchronously slew to the given equatorial $RightAscension $Declination coordinates.
    TAlpacaErr synctocoordinates(float ra, float dec) override { sync(int32_t(ra*3600.0f), int32_t(dec*3600.0f)); return ALPACA_OK; } // Syncs to the given $RightAscension $Declination coordinates.

    TAlpacaErr axisrates(int axis, char *b) override { strcpy(b, "[{\"Maximum\": 0,\"Minimum\": 4}]"); return ALPACA_OK; } // Returns the rates at which the telescope may be moved about the specified $Axis  returns [{"Maximum": 0,"Minimum": 0}] in b (b will be 30 chr long)
    bool canmoveaxis(int axis) override { return true; } // Indicates whether the telescope can move the requested $Axis.
    TAlpacaErr moveaxis(int axis, float rate) override   // Moves a telescope $Axis at the given $Rate.
    { 
        if (axis==0) MRa.goUpRealNoAbs(int(rate*3600.0f));
        else if (axis==1) MDec.goUpRealNoAbs(int(rate*(3600.0f/15.0f)));
        return ALPACA_OK; 
    }

    float declination() override { return MDec.posInReal()/3600.0f; }; // Returns the mount's declination.
    float rightascension() override { return MRa.posInReal()/3600.0f; }; // Returns the mount's right ascension coordinate.

    int get_sideofpier() override { return scopeWest()?1:0; } // Returns the mount's pointing state. 0:east, 1: west, -1: unknown
    int destinationsideofpier(float ra, float dec) override { return (scopeWest() ^ sameSideOfMeridian(int32_t(ra*3600.0f))) ? 0:1; } // Predicts the pointing state after a German equatorial mount slews to given $RightAscension $Declination coordinates. 0: east, 1: west: -1: unknown

    float get_aperturearea() override { return CSavedData::savedData.Area_cm2/10000.0f; } // Returns the telescope's aperture.
    TAlpacaErr set_aperturearea(float v) override { CSavedData::savedData.Area_cm2= v*10000.0f; return ALPACA_OK; } // Returns the telescope's aperture.
    float get_aperturediameter() override { return CSavedData::savedData.Diameter_mm/1000.0f; } // Returns the telescope's effective aperture.
    TAlpacaErr set_aperturediameter(float v) override { CSavedData::savedData.Diameter_mm= v*1000.0f; return ALPACA_OK; } // Returns the telescope's effective aperture.
    float get_focallength() override { return CSavedData::savedData.FocalLength/1000.0f; } // Returns the telescope's focal length in meters.
    TAlpacaErr set_focallength(float v) override { CSavedData::savedData.FocalLength= v*1000.0f; return ALPACA_OK;  } // Returns the telescope's focal length in meters.
    float get_siteelevation() override { return CSavedData::savedData.Altitude; } // Returns the observing $SiteElevation above mean sea level.
    TAlpacaErr set_siteelevation(float v) override { CSavedData::savedData.Altitude= uint16_t(v); return ALPACA_OK; } // Sets the observing site's elevation above mean sea level.
    float get_sitelatitude() override { return CSavedData::savedData.Latitude/3600.0f; } // Returns the observing $SiteLatitude .
    TAlpacaErr set_sitelatitude(float v) override { CSavedData::savedData.Latitude= v*3600.0f; return ALPACA_OK; } // Sets the observing site's latitude.
    float get_sitelongitude() override { return CSavedData::savedData.Longitude/3600.0f; } // Returns the observing site's longitude.
    TAlpacaErr set_sitelongitude(float v) override { CSavedData::savedData.Longitude= v*3600.0f; return ALPACA_OK; } // Sets the observing $SiteLongitude .


    void subSetup(CAlpaca *Alpaca, int sock, bool get, char *data, CMyStr &s) override  // This allows you to add stuff in the HTML or handle inputs...
    {
        CTelescope::subSetup(Alpaca, sock, get, data, s);
        if (data!=nullptr)
        {   
            int v; bool reinit= false;
            v= getIntData(data,"RaMax");    if (v!=-1) CSavedData::savedData.ra.maxPos= v, reinit= true;
            v= getIntData(data,"RaMaxSpd"); if (v!=-1) CSavedData::savedData.ra.maxSpd= v, reinit= true;
            v= getIntData(data,"RaAcc");    if (v!=-1) CSavedData::savedData.ra.msToSpd= v, reinit= true;
            v= getIntData(data,"RaBack");   if (v!=-1) CSavedData::savedData.raBacklash= v, reinit= true;
            v= getIntData(data,"RaSettle"); if (v!=-1) CSavedData::savedData._raSettle= v, reinit= true;
            v= getIntData(data,"RaAmplitude"); if (v!=-1) CSavedData::savedData.raAmplitude= v, reinit= true;
            v= getIntData(data,"RaGuide");  if (v!=-1) {
                v= (int)((v * (int64_t)CSavedData::savedData.ra.maxPos / (360*3600))); if (v>255) v= 255;
                CSavedData::savedData.guideRateRA= v, reinit= true;
            }
            v= getIntData(data,"RaInvert");  if (v!=-1) { CSavedData::savedData.invertAxes= (CSavedData::savedData.invertAxes&~2) | ((v!=0)?2:0); reinit= true; }
            
            v= getIntData(data,"DecMax");    if (v!=-1) CSavedData::savedData.dec.maxPos= v, reinit= true;
            v= getIntData(data,"DecMaxSpd"); if (v!=-1) CSavedData::savedData.dec.maxSpd= v, reinit= true;
            v= getIntData(data,"DecAcc");    if (v!=-1) CSavedData::savedData.dec.msToSpd= v, reinit= true;
            v= getIntData(data,"DecBack");   if (v!=-1) CSavedData::savedData.decBacklash= v, reinit= true;
            v= getIntData(data,"DecGuide");  if (v!=-1) {
                v= (int)((v * (int64_t)CSavedData::savedData.dec.maxPos / (360*3600))); if (v>255) v= 255;
                CSavedData::savedData.guideRateDec= v, reinit= true;
            }
            v= getIntData(data,"DecInvert");  if (v!=-1) { CSavedData::savedData.invertAxes= (CSavedData::savedData.invertAxes&~1) | ((v!=0)?1:0); reinit= true; }
            if (reinit) { CSavedData::savedData.save(); }
        }
        s.printf("<h1>Motors</h1>"
            "<form action=\"/setup/v1/%s/%d/setup\">"
            "<h2>RA Stepper</h2>"
            "<table align=\"center\">"
            "  <tr><td align=\"right\"><label for=\"RaMax\">Steps for 360deg:</label></td>"
            "      <td><input type=\"text\" id=\"RaMax\" name=\"RaMax\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"RaMaxSpd\">Speed Steps/s:</label></td>"
            "      <td><input type=\"text\" id=\"RaMaxSpd\" name=\"RaMaxSpd\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"RaAcc\">Time to full speed in ms:</label></td>"
            "      <td><input type=\"text\" id=\"RaAcc\" name=\"RaAcc\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"RaBack\">Backlash in steps:</label></td>"
            "      <td><input type=\"text\" id=\"RaBack\" name=\"RaBack\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"RaSettle\">Settle in arcsec:</label></td>"
            "      <td><input type=\"text\" id=\"RaSettle\" name=\"RaSettle\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"RaAmplitude\">Amplitude in degree:</label></td>"
            "      <td><input type=\"text\" id=\"RaAmplitude\" name=\"RaAmplitude\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"RaGuide\">guide rate in arcs/s:</label></td>"
            "      <td><input type=\"text\" id=\"RaGuide\" name=\"RaGuide\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"RaInvert\">ra is inverted (0 or 1):</label></td>"
            "      <td><input type=\"text\" id=\"RaInvert\" name=\"RaInvert\" value=\"%d\"></td>"
            "</table>"
            "<h2>Declinaison Stepper</h2>"
            "<table align=\"center\">"
            "  <tr><td align=\"right\"><label for=\"DecMax\">Steps for 360deg:</label></td>"
            "      <td><input type=\"text\" id=\"DecMax\" name=\"DecMax\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"DecMaxSpd\">Speed Steps/s:</label></td>"
            "      <td><input type=\"text\" id=\"DecMaxSpd\" name=\"DecMaxSpd\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"DecAcc\">Time to full speed in ms:</label></td>"
            "      <td><input type=\"text\" id=\"DecAcc\" name=\"DecAcc\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"DecBack\">Backlash in steps:</label></td>"
            "      <td><input type=\"text\" id=\"DecBack\" name=\"DecBack\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"DecGuide\">guide rate in arcs/s:</label></td>"
            "      <td><input type=\"text\" id=\"DecGuide\" name=\"DecGuide\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"DecInvert\">dec is inverted (0 or 1):</label></td>"
            "      <td><input type=\"text\" id=\"DecInvert\" name=\"DecInvert\" value=\"%d\"></td>"
            "</table>"
            "<input type=\"submit\" value=\"Update\">"
            "</form>",

            get_type(), id,
            CSavedData::savedData.ra.maxPos, CSavedData::savedData.ra.maxSpd, CSavedData::savedData.ra.msToSpd, CSavedData::savedData.raBacklash, CSavedData::savedData._raSettle, CSavedData::savedData.raAmplitude, CSavedData::savedData.guideRateRA*360*3600/CSavedData::savedData.ra.maxPos, (CSavedData::savedData.invertAxes&2)!=0?1:0,
            CSavedData::savedData.dec.maxPos, CSavedData::savedData.dec.maxSpd, CSavedData::savedData.dec.msToSpd, CSavedData::savedData.decBacklash, CSavedData::savedData.guideRateDec*360*3600/CSavedData::savedData.dec.maxPos, (CSavedData::savedData.invertAxes&1)!=0?1:0);
        s+= "</p>";
    }

};

class CMyFocuser : public CFocuser
{ public:
    CMyFocuser(int id): CFocuser(id, "CdB Focuser Driver", "1", "CdB Alpaca Focuser", "Focuser for eqMount") { }
    bool get_absolute() override { return true; }
    bool get_ismoving() override { return MFocus.isMoving(); }
    int32_t get_maxincrement() override { return MFocus.maxPos; }
    int32_t get_maxstep() override { return MFocus.maxPos; }
    int32_t get_position() override { return MFocus.pos; }
    int32_t get_stepsize() override { return CSavedData::savedData.FocStepdum/10; }
    TAlpacaErr put_halt() override { MFocus.stop(); return ALPACA_OK; };
    TAlpacaErr put_move(int32_t position) override { stopMovingOnKeyRelease= false; MFocusOn(); MFocus.goToSteps(position, MFocus.spdMax); return ALPACA_OK; };
    void subSetup(CAlpaca *Alpaca, int sock, bool get, char *data, CMyStr &s) override // This allows you to add stuff in the HTML or handle inputs...
    {
        static char savedPosString[1024]; Alpaca->load("savedPos", "", savedPosString, sizeof(savedPosString)); // (name\0x8pos\0x7)*
        if (data!=nullptr)
        {   
            char const *d= getStrData(data, "savePos");
            if (d!=nullptr)
            {
                char name[21]; int i; for (i=0; i<19; i++) if (*d<' ' || *d>'z') break; else name[i]= *d++; name[i++]= 8; name[i]= 0; // extract name
                size_t l= strlen(savedPosString); if (l>0 && savedPosString[l-1]!=7) { savedPosString[l-1]= 7; l++; } // make sure ends with 7
                char *d= strstr(savedPosString, name); // find existing name in the list...
                if (d!=nullptr) // if there...
                {
                    char *s= d; while (*s!=0 && *s!=7) s++; if (*s==7) s++; // go to end of pos
                    strcpy(d, s); // and erase by copying whatever is after over start...
                    l= strlen(savedPosString);
                }
                sprintf(savedPosString+l, "%s%d\007", name, int(MFocus.pos/32)); // add current post at end of string
                Alpaca->save("savedPos", savedPosString); // and save
            }

            d= getStrData(data, "erasePos");
            if (d!=nullptr)
            {
                char name[21]; int i; for (i=0; i<19; i++) if (*d<' ' || *d>'z') break; else name[i]= *d++; name[i++]= 8; name[i]= 0; // extract name
                size_t l= strlen(savedPosString); if (l>0 && savedPosString[l-1]!=7) { savedPosString[l-1]= 7; l++; } // make sure ends with 7
                char *d= strstr(savedPosString, name); // find existing name in the list...
                if (d!=nullptr) // if there...
                {
                    char *s= d; while (*s!=0 && *s!=7) s++; if (*s==7) s++; // go to end of pos
                    strcpy(d, s); // and erase by copying whatever is after over start...
                    Alpaca->save("savedPos", savedPosString); // and save
                }
            }

            bool reinit= false;
            int v= getIntData(data,"FocMax");    if (v!=-1) CSavedData::savedData.focMaxStp= v, reinit= true;
            v= getIntData(data,"FocMaxSpd"); if (v!=-1) CSavedData::savedData.focMaxSpd= v, reinit= true;
            v= getIntData(data,"FocAcc");    if (v!=-1) CSavedData::savedData.focAcc= v, reinit= true;
            v= getIntData(data,"FocBack");   if (v!=-1) CSavedData::savedData.focBacklash= v, reinit= true;
            v= getIntData(data,"FocInvert");  if (v!=-1) { CSavedData::savedData.invertAxes= (CSavedData::savedData.invertAxes&~4) | ((v!=0)?4:0); reinit= true; }
            v= getIntData(data,"FocStep");  if (v!=-1) { CSavedData::savedData.FocStepdum= v; reinit= true; }
            if (reinit) { CSavedData::savedData.save(); }
        }
        CFocuser::subSetup(Alpaca, sock, get, data, s);
        s.printf("<form action=\"/setup/v1/%s/%d/setup\">"
            "  <label for=\"savePos\">Position Name:</label>"
            "  <input type=\"text\" id=\"savePos\" name=\"savePos\" value=\"name\">"
            "  <input type=\"submit\" value=\"Save\">"
            "</form>",get_type(), id);
        char *S= savedPosString; bool hasone= false;
        while (*S!=0)
        {
            char name[20]; int i; for (i=0; i<19; i++) if (*S<=8) break; else name[i]= *S++; name[i]= 0; if (*S==8) S++;
            char pos[20]; int j; for (j=0; j<19; j++) if (*S<=7) break; else pos[j]= *S++; pos[j]= 0; if (*S==7) S++;
            if (i==0 || j==0) break;
            if (!hasone) { s.printf("<h2>Saved Positions</h2><br><table align=\"center\">"); hasone= true; }
            s.printf("<tr><th><form action=\"/setup/v1/%s/%d/setup\">"
                "  <label for=\"position\">%s:</label>"
                "  <input type=\"text\" id=\"position\" name=\"position\" value=\"%s\">"
                "  <input type=\"submit\" value=\"GoTo\">"
                "</form></th>"
                "<th><form action=\"/setup/v1/%s/%d/setup\">"
                "  <input type=\"hidden\" id=\"erasePos\" name=\"erasePos\" value=\"%s\">"
                "  <input type=\"submit\" value=\"Erase\">"
                "</form></th></tr>"
                , get_type(), id, name, pos, get_type(), id, name);
        }
        if (hasone) s.printf("</table>");
        s.printf("<h1>Motor</h1>"
            "<form action=\"/setup/v1/%s/%d/setup\">"
            "<table align=\"center\">"
            "  <tr><td align=\"right\"><label for=\"FocMax\">max steps:</label></td>"
            "      <td><input type=\"text\" id=\"FocMax\" name=\"FocMax\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"FocMaxSpd\">Speed Steps/s:</label></td>"
            "      <td><input type=\"text\" id=\"FocMaxSpd\" name=\"FocMaxSpd\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"FocAcc\">Time to full speed in ms:</label></td>"
            "      <td><input type=\"text\" id=\"FocAcc\" name=\"FocAcc\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"FocBack\">Backlash in steps:</label></td>"
            "      <td><input type=\"text\" id=\"FocBack\" name=\"FocBack\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"FocInvert\">foc is inverted (0 or 1):</label></td>"
            "      <td><input type=\"text\" id=\"FocInvert\" name=\"FocInvert\" value=\"%d\"></td>"
            "  <tr><td align=\"right\"><label for=\"FocStep\">focuser step size in tenth of micron:</label></td>"
            "      <td><input type=\"text\" id=\"FocStep\" name=\"FocStep\" value=\"%d\"></td>"
            "</table>"
            "<input type=\"submit\" value=\"Update\">"
            "</form>",
            get_type(), id,
            CSavedData::savedData.focMaxStp, CSavedData::savedData.focMaxSpd, CSavedData::savedData.focAcc, CSavedData::savedData.focBacklash, (CSavedData::savedData.invertAxes&4)!=0?1:0, CSavedData::savedData.FocStepdum);
        
    }
};

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
    #ifdef HASGPS // This one will setup the uart system... which will not be used after...
        CADC::begin();
    #endif
    xTaskCreate(UITask, "UI", 3048, NULL, 2, NULL);

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

    startWifi(alpaca->wifi, alpaca->wifip, "eqControl", (CSavedData::savedData.guidingBits&0x40)!=0);
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
