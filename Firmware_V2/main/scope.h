#include "libsgp4/CoordGeodetic.h"
#include "libsgp4/CoordTopocentric.h"
#include "libsgp4/Observer.h"
#include "libsgp4/SGP4.h"
#include "asteroids.h"

class CScope { public:
	// First we have basic non changing scope dataconfiguration here...
	double latitude=45.0, longitude=-4.0, altitude=1080.0;
	double minAltitude=1.0; // These are physical scope limits...
	void init(uint32_t SecSince2026, double latitude, double longitude, double altitude, double mountAmplitude);
	void setJulianSecSince2026(uint32_t sec);
	// Parking features
		double parkMotor1=0.0, parkMotor2=0.0;   // park position of motors
		bool parked=false, parking=false;       // the scope is parked (not moving anymore) or parking (going to the parking position)
	void park() { setSideralRate(0); guideRa(0, 0), guideDec(0, 0), moveRa(0), moveDec(0); raTrackRate=0.0, decTrackRate= 0.0; satelite= false; asteroid= false; slewing= true, parking= true; flipping= false; }
	void unpark() { satelite= false; asteroid= false; slewing= false; flipping= false; parked= parking= false; guideRa(0, 0), guideDec(0, 0), moveRa(0), moveDec(0); raTrackRate=0.0, decTrackRate= 0.0; setSideralRate(0); setTracking(true); }
	void setpark() { motor1angle= motor1angle, motor2angle= motor2angle; }
	// Here we have the main telescope "state"
	double motor1angle=0.0, motor2angle=0.0;
	double raActual=0.0, decActual=0.0, az=0.0, alt=0.0; // this is where the scope is calculated to be based on motor angles...
	double desiredRa=0.0, desiredDec=0.0;         // This is where we want the scope to be in 1s
	double incrementalRa=0.0, incrementalDec=0.0; // This is where we think it SHOULD be right now...
	double startLst=0.0; // LST for when nowus=0.
	double lst=0.0;      // Lst of the last "loop"...
	bool slewing= false;
	bool flipping= false;
	bool technicalFlip= false; // put in a volatile way by external forces to tell the scope to "assume" that it has changed direction...
	bool scopeEast= false;
	double minRaAngle=-105, maxRaAngle= 105; // min and max ra angles in deg for the RA motor. Will be initialized from east/westRaAmplitude
	bool getSlewing() { return slewing || flipping || raMoveRate!=0.0 || decMoveRate!=0.0; }

	double raMotorDegToRa(double motorDeg) { return fmod(motorDeg/15.0+lst+(scopeEast?6:-6)+24.0, 24.0); }
	double RaToRaMotorDeg(double ra, double lst) { 
		double motor= ra-lst-(scopeEast?6:-6); 
		while (motor>12.0) motor-=24.0; while (motor<=-12.0) motor+=24.0; 
		return motor*15.0; }

	// Guiding at speed until given time...
		double raGuideRate=0.0, decGuideRate= 0.0; // rates in unit/s
		uint64_t stopGuideRaAt=0, stopGuideDecAt=0;
	bool guideRa(double speed, int durationMs)
	{
		if (parked || parking || durationMs<0) return false;
		if (durationMs==0) speed= 0.0;
		raGuideRate=speed*15.0; stopGuideRaAt=nowus()+durationMs*1000;
		return true;
	}
	bool guideDec(double speed, int durationMs)
	{
		if (parked || parking || durationMs<0) return false;
		if (durationMs==0) speed= 0.0;
		decGuideRate= speed; stopGuideDecAt= nowus()+durationMs*1000;
		return true;
	}
	bool isPulseGuiding() { return raGuideRate!=0.0 || decGuideRate!= 0.0; }

	// joystick axes movements...
	double raMoveRate=0.0, decMoveRate=0.0; // rates in units/s
	bool moveRa(double speed) { if (parked || parking || slewing || flipping) return false; raMoveRate= speed/15.0; return true; } // input in deg/s
	bool moveDec(double speed) 
	{ 
		if (parked || parking || slewing || flipping) return false;
		if (speed<=0 || decActual<89.9) { decMoveRate= speed; return true; } // flip when at top and direction is down...
		technicalFlip= true; // tell the servicing loop to change the side of pier. Assumes possible!
		decMoveRate= -speed; // then go down...
		return true;
	}

	// Sideral tracking weired 2 step system...
		bool tracking= false; // true if the telescope is moving at sideral time...
		double raSideralRate=0.0; // Note that this is a differential to sideral. So 0 is sideral speed! in ra units/s
	bool setTracking(bool enabled);
		int sideralRateType= 0; // 0: sideral, 1: lunar, 2: solar, 3: king (15.0369 arc"/s)
	bool setSideralRate(int type, bool enable= false); // if enable, then force the start of tracking. Else leave as is

	// ascom differential sideral tracking on axes
	double raTrackRate=0.0, decTrackRate= 0.0; // in units/s
	int TODO; // verify units to check if there is not a factor to change or a direction...
	bool trackRa(double speed) { if (parked || parking) return false; raTrackRate= speed*15.0; return true; }
	bool trackDec(double speed) { if (parked || parking) return false; decTrackRate= speed; return true; }

	// This is about slew and sync...
	int destinationsideofpier(double ra, bool visual=false); // 0:east 1:west
	double altitudeOf(double ra, double dec); // return the altitude (in alt/az reference) of these coordinates
	bool flip();
	    bool _slew= false; double _slewRa, _slewDec; bool _slewVisual;
	bool slew(double ra, double dec, bool visual=false) { if (parked || parking) return false; _slewRa= ra; _slewDec= dec; _slewVisual= visual; _slew= true; return true; }
	    bool _abortslew= false; 
	void abortslew() { _abortslew= true; }
		bool _synctocoordinates= false; double _syncRa, _syncDec;
	bool synctocoordinates(double ra, double dec) { _syncRa= ra; _syncDec= dec; _synctocoordinates= true; return true; }
	bool set_sideofpier(bool scopeEast) // Cause a flip if possible... 
	{
		if (scopeEast==this->scopeEast) return false;
		return flip();
	}

	// for asteroid tracking
	bool asteroid= false;      // true if we are tracking an asteroid..
	CMPCOrbit asteroidData;    // asteroid computation system
	double startJulianDay=0.0; // the julian date for when nowus==0
	bool startAsteroid(char const *mpcOrbLine);

	// for satelite tracking
	bool satelite= false;            // true if tracking a satelite
	libsgp4::SGP4 sateliteData;      // satelite computation system
	libsgp4::Observer obs;           // observer postiion
	libsgp4::DateTime startDateTime; // dateTime for when nowus==0
	double sateliteRaCorrect= 0.0, sateliteDecCorrect= 0.0; // user correction to add to the 2 calculated values
	void startSatelite(char const * tle[3]);

	void targetToRealMotorPos(double ra, double dec, double& a1, double& a2);
	void realMotorPosToTarget(double a1, double a2, double& ra, double& dec);
	void syncToMotors(); // used mostly for debug and such...make ra/dec match motors
	uint64_t lastus; // last time HWLoopwas called. set it to current time at startup time!
	void HWLoop(uint64_t nowus);

#ifdef STATS
    void stats() 
	{
		if (stopStats) return;
		statincrementalDec.v.push_back(incrementalDec); statdecMoveRate.v.push_back(decMoveRate); statdesiredDec.v.push_back(desiredDec);
		statincrementalRa.v.push_back(incrementalRa*15.0); statRaMoveRate.v.push_back(raMoveRate); statdesiredRa.v.push_back(desiredRa*15.0);
	}
#endif
};
