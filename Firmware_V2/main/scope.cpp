#include "system.h"
static double const PI= 3.14159265358979323846264338327;


// Convertit RA/Dec/LST/Lat (en h24/degrees) en Alt/Az (en radians)
void raDecToAltAz(double ra, double dec, double lst, double lat, double &alt, double &az) 
{
	double ra_rad = ra * (15.0 * PI / 180.0); // RA de heures à radians
	double dec_rad = dec * (PI / 180.0);
	double lst_rad = lst * (15.0 * PI / 180.0);
	double lat_rad = lat * (PI / 180.0);
	double ha = lst_rad - ra_rad;
	alt = asin(sin(dec_rad) * sin(lat_rad) + cos(dec_rad) * cos(lat_rad) * cos(ha));
	double y = sin(ha);
	double x = sin(lat_rad) * cos(ha) - cos(lat_rad) * tan(dec_rad);
	az = atan2(y, x)-PI;
	while (az<0) az+= 2.0*PI; // normalisation
	alt*= 180.0/PI; az*= 180.0/PI; 
}
void altAzToRaDec(double alt, double az, double lat_rad, double lst, double &ra, double &dec) 
{
	// 2. Conversion Alt/Az -> Dec / Hour Angle (HA)
	// double lat_rad = lat * (M_PI / 180.0);
	// Calcul de la Déclinaison (Dec)
	double sin_dec = sin(alt) * sin(lat_rad) + cos(alt) * cos(lat_rad) * cos(az);
	double tdec = asin(sin_dec);
	// Calcul de l'Angle Horaire (HA)
	double cos_ha = (sin(alt) - sin(lat_rad) * sin(tdec)) / (cos(lat_rad) * cos(tdec));
	// Sécurité pour les erreurs d'arrondi de doubleing point
	if (cos_ha > 1.0) cos_ha = 1.0; if (cos_ha < -1.0) cos_ha = -1.0;
	double ha = acos(cos_ha);
	// Si l'Azimuth est à l'Est du méridien, l'HA est négatif
	if (sin(az) > 0.0) ha = 2.0 * PI - ha;
	// 3. Conversion HA -> Right Ascension (RA)
	double ha_hours = ha * (180.0 / PI) / 15.0;
	double tra = lst - ha_hours;
	// Normalisation de la RA entre 0 et 24h
	while (tra < 0.0) tra += 24.0;
	while (tra >= 24.0) tra -= 24.0;
	// write output...
	ra= tra;
	dec = tdec * (180.0 / PI);
}


void CScope::init(uint32_t SecSince2026, double latitude, double longitude, double altitude, double mountAmplitude)
{
	this->latitude= latitude; this->longitude= longitude, this->altitude= altitude;
	minRaAngle= -mountAmplitude/2;
	maxRaAngle= mountAmplitude/2;
	setJulianSecSince2026(SecSince2026);
	scopeEast= false;
	syncToMotors();
	setSideralRate(0); setTracking(true); guideRa(0, 0), guideDec(0, 0), moveRa(0), moveDec(0);
	satelite= false; asteroid= false; slewing= false, parking= false; flipping= false;
	raTrackRate=0.0, decTrackRate= 0.0;
	lastus= nowus();
}
void CScope::setJulianSecSince2026(uint32_t sec)
{
	libsgp4::DateTime epoch= libsgp4::DateTime(2026, 1, 1); // our base...
	startDateTime= epoch.AddSeconds(sec).AddMicroseconds(-int64_t(nowus())); // add sec to get now, remove high precision timer to get a start time...
	startJulianDay= startDateTime.ToJulian(); // the julian date for when nowus==0
	startLst= fmod(startDateTime.ToGreenwichSiderealTime0_24()+longitude/15.0, 24.0);
	lst= fmod(startLst+nowus()/(86164090500.0/24), 24.0); while (lst>24.0) lst-= 24.0; // sideral hour calculation!
}
bool CScope::setTracking(bool enabled)
{
	if (parked || parking) return false;
	tracking= enabled;
	return setSideralRate(sideralRateType);
}
bool CScope::setSideralRate(int type, bool enable)
{ 
	if (type<0 || type>3) type= 0;
	if (parked || parking) return false;
	static double rates[] = { // This is a delta from sideral!!!!
		0.0, // sideral 
		24.0/2360591.0, // lunar
		24.0/365.25/(24.0*3600.0), // solar
		0.0369*15.0 // king, which is an average of daily that takes into account atmospheric perturbations. Useless with a pointing model
	};
	sideralRateType= type;
	if (enable) tracking= true;
	if (tracking) raSideralRate=rates[type];
	else raSideralRate=-24.0/86164.090500; // sideral stop rate...
	return true;
}

double CScope::altitudeOf(double ra, double dec)
{
	double az, alt; raDecToAltAz(ra, dec, lst, latitude, alt, az); return alt;
}
int CScope::destinationsideofpier(double ra, bool visual) // 0:east 1:west
{
	double raDeg= RaToRaMotorDeg(ra, lst);
	//trace("destinationsideofpier %f %f %f %s   ", ra, raDeg, lst, scopeEast?"east":"west");
	if (raDeg<minRaAngle || raDeg>maxRaAngle) return scopeEast?1:0; // we do not fit in current "span", so the destination is the inverse of the current one!
	if (visual) return scopeEast?0:1; // in visual mode, then we are ok as we are...
	// in photo mode, the question is: would we be better of "on the other side"?
	return ((raDeg>90.0-(-90.0-minRaAngle)) ^ scopeEast)?0:1; // min ra angle is always <=-6 and indicates how much extra room there is. so -6-a gives us that "room". We remove it from the horizontal to calculate at which point switch is possible...
}
bool CScope::flip()
{
	double ra= RaToRaMotorDeg(raActual, lst);
	if (ra>90.0-(-90.0-minRaAngle) && ra<-90.0+(maxRaAngle-90.0)) return false;
	flipping= true;
	return true;
}

bool CScope::startAsteroid(char const *mpcOrbLine)
{
	int TODO; // They probably should be some way to check on visibility before starting!
	return asteroid= asteroidData.fromMpcorb(mpcOrbLine);
}

void CScope::startSatelite(char const * tle[3])
{
	int TODO; // They probably should be some way to check on visibility before starting!
	tle[0]= "UK-DMC 2                ";
	tle[1]= "1 35683U 09041C   12289.23158813  .00000484  00000-0  89219-4 0  5863";
	tle[2]= "2 35683  98.0221 185.3682 0001499 100.5295 259.6088 14.69819587172294";
	obs= libsgp4::Observer(latitude, longitude, altitude/1000.0); // to be verified on altitude...
	sateliteData.SetTle(libsgp4::Tle(tle[0], tle[1], tle[2]));
	sateliteRaCorrect= sateliteDecCorrect= 0.0;
	satelite= true;
}

void CScope::targetToRealMotorPos(double ra, double dec, double& a1, double& a2)
{
	a1= ra; a2= dec;
} 
void CScope::realMotorPosToTarget(double a1, double a2, double& ra, double& dec)
{
	ra= a1; dec= a2;
} 
void CScope::syncToMotors() // used mostly at init and for debug and such...make ra/dec match motors
{
	double tra, tdec; realMotorPosToTarget(motor1angle= motor1.posInReal(), motor2angle= motor2.posInReal(), tra, tdec);
	desiredRa= incrementalRa= raActual= raMotorDegToRa(tra), desiredDec= incrementalDec= decActual= scopeEast ? tdec+90.0 : 90.0-tdec;
	raDecToAltAz(tra, tdec, 0.0, latitude, alt, az);
	//ATLTRACE2("Sync To motors @%f %f %f -> %f %f\r\n", lst, motor1angle, motor2angle, desiredRa, desiredDec);
}

void CScope::HWLoop(uint64_t nowus)
{
	// Write actual ra/dec vs want to be ra/dec...
	// get deltaT and use to calculate distances to move... Need precision!
	// first, figure where we are...
	uint32_t deltaus= uint32_t(nowus-lastus); lastus= nowus;
	lst= fmod(startLst+nowus/(86164090500.0/24), 24.0); while (lst>24.0) lst-= 24.0; // number of us in a sideral hour...
	double tra, tdec; realMotorPosToTarget(motor1angle= motor1.posInReal(), motor2angle= motor2.posInReal(), tra, tdec);

	if (_slew)
	{
		_slew= false;
		if (!parked && !parking)
		{
			desiredRa= incrementalRa= _slewRa, desiredDec= incrementalDec= _slewDec;
			parking= slewing= flipping= false;
			raMoveRate=0.0, decMoveRate=0.0; 
			raGuideRate=0.0, decGuideRate= 0.0; stopGuideRaAt=0, stopGuideDecAt=0;
			flipping= ((destinationsideofpier(desiredRa, _slewVisual)==0) != scopeEast);
			slewing= true;
		}
	}

	if (_abortslew)  // Async abort slew
	{ 
		parking= slewing= flipping= false;
		_abortslew= false;
		double t1= RaToRaMotorDeg(raActual, lst)+motor1.stopDistanceReal(); if (t1<minRaAngle) t1=minRaAngle; if (t1>maxRaAngle) t1=maxRaAngle; 
		desiredRa= incrementalRa= raMotorDegToRa(t1); 
		double t2= decActual+(scopeEast?1:-1)*motor2.stopDistanceReal(); if (t2<-90.0) t2=-90.0; if (t2>90.0) t2=90.0; desiredDec= incrementalDec= t2; 
		raMoveRate=0.0, decMoveRate=0.0; 
		raGuideRate=0.0, decGuideRate= 0.0; stopGuideRaAt=0, stopGuideDecAt=0;
	}

	if (_synctocoordinates)
	{
		_synctocoordinates= false;
		if (!parked && !parking)
		{
			double a1, a2; targetToRealMotorPos(RaToRaMotorDeg(_syncRa, lst), scopeEast ? _syncDec-90.0 : 0-(_syncDec-90.0), a1, a2);
			motor1.setPos(a1); motor2.setPos(a2);
			desiredRa= incrementalRa= _syncRa; 
			desiredDec= incrementalDec= _syncDec; 
			raMoveRate=0.0, decMoveRate=0.0; raGuideRate=0.0, decGuideRate= 0.0; stopGuideRaAt=0, stopGuideDecAt=0;
		}
	}

	if (technicalFlip) // We are asked to declare that the scope is now going the other way.. Normally this happends only when dec is at 90°
	{   // main use case is when going "UP" while at 90°.
		technicalFlip= false; slewing= false; flipping= false; asteroid= satelite= false;
		scopeEast= !scopeEast;
		desiredRa= incrementalRa= raMotorDegToRa(tra);
		raMoveRate=0.0, decMoveRate=0.0; 
		raGuideRate=0.0, decGuideRate= 0.0; stopGuideRaAt=0, stopGuideDecAt=0;
	}

	raActual= raMotorDegToRa(tra); decActual= scopeEast ? tdec+90.0 : 90.0-tdec;
	//ATLTRACE2("pos from motors @%f %f %f -> %f %f\r\n", lst, motor1angle, motor2angle, raActual, decActual);
	raDecToAltAz(raActual, decActual, lst, latitude, alt, az);
	if (parked) return;
	// now where do we want to go?
	if (flipping) // flipping case..
	{ 
		if (fabs(motor1angle)>4.0/3600.0 || fabs(motor2angle)>4.0/3600.0)
		{
			motor1.goTo(0.0); motor2.goTo(0.0); // to to north!
			return;
		}
		// at destination, stop then flip, change scope side and go to where we want to...
		slewing= true;
		flipping= false;
		scopeEast= !scopeEast;
		raMoveRate=0.0, decMoveRate=0.0; raGuideRate=0.0, decGuideRate= 0.0; stopGuideRaAt=0, stopGuideDecAt=0;
		incrementalRa= desiredRa; incrementalDec= desiredDec;
		//trace("Flipped\r\n\r\n");
	}
	if (parking) 
	{
		if (fabs(motor1angle-parkMotor1)>0.1 || fabs(motor2angle-parkMotor2)>0.1) // unless we are there..
			motor1.goTo(parkMotor1), motor2.goTo(parkMotor2); // to to parking position!
		else
			slewing= false, parking= false, parked= true;
		return;
	}
	int timeToDesired= 100; 
	if (slewing) // slewing, go to desination....
	{
		if (fabs(raActual-desiredRa)<1.0/3600.0 && fabs(decActual-desiredDec)<1.0/3600.0)  // done slewing?
		{ 
			incrementalRa= desiredRa, incrementalDec= desiredDec;
			slewing= false; 
			return; 
		}
	}
	else if (asteroid) // are we chasing an asteroid?
	{
		asteroidData.ephemeris(startJulianDay+(nowus+100000)/(24.0*3600.0*1000000.0), latitude, longitude, altitude, desiredRa, desiredDec);
		incrementalRa=desiredRa, incrementalDec= desiredDec;
	} else if (satelite) // Are we chasing a satelite?
	{
		libsgp4::Eci eci= sateliteData.FindPosition(startDateTime.AddMicroseconds(nowus+100000));
		libsgp4::CoordTopocentric topo= obs.GetLookAngle(eci); // from observer point of view
        altAzToRaDec(topo.elevation, topo.azimuth, latitude*(PI/180.0), lst+timeToDesired/3600000.0, desiredRa, desiredDec);
		desiredRa+= sateliteRaCorrect; desiredDec+= sateliteDecCorrect;
		incrementalRa=desiredRa, incrementalDec= desiredDec;
	} else {  // normal mode, we are just moving at rates...
		if (stopGuideRaAt<nowus) raGuideRate= 0; 
		incrementalRa+= (raGuideRate+raMoveRate+raSideralRate+raTrackRate)*deltaus/1000000;
		desiredRa= incrementalRa+(raGuideRate+raMoveRate+raSideralRate+raTrackRate)*timeToDesired/1000.0;
		if (stopGuideDecAt<nowus) decGuideRate= 0;
		incrementalDec+= (decGuideRate+decMoveRate+decTrackRate)*deltaus/1000000;
		desiredDec= incrementalDec+(decGuideRate+decMoveRate+decTrackRate)*timeToDesired/1000.0;
		if (desiredDec>90.0) desiredDec= incrementalDec= 90.0, decGuideRate= decMoveRate= decTrackRate= 0; 
		if (desiredDec<-90.0) desiredDec= incrementalDec= -90.0, decGuideRate= decMoveRate= decTrackRate= 0; 
	}
	if (desiredRa<0) desiredRa+= 24.0; if (desiredRa>24.0) desiredRa-= 24.0; 
	if (incrementalRa<0) incrementalRa+= 24.0; if (incrementalRa>24.0) incrementalRa-= 24.0; 
	if ((destinationsideofpier(desiredRa, true)==0) != scopeEast) { 
		//trace("Will flipt because desired ra:%f caused a flip\r\n", desiredRa);
		//destinationsideofpier(desiredRa, true);
		flip(); return; } // test for meridian flip
	// Altitude check before movement: if next altitude too small and smaller than current (i.e we are going down, not up), then stop movement
	// however, if I see the altitude growing, then I let it go!
	double naz, nalt; raDecToAltAz(desiredRa, desiredDec, lst+timeToDesired/3600000.0, latitude, nalt, naz);
	if (nalt<minAltitude && nalt<alt) // Are we bellow horizon and not going higher? stop..
	{ 
		incrementalRa=desiredRa=raActual, incrementalDec=desiredDec=decActual; 
		altAzToRaDec(minAltitude, naz, latitude*(PI/180.0), lst+timeToDesired/3600000.0, desiredRa, desiredDec);
	}
	// Figure out where the motors need to be in 1s...
	double a1, a2; targetToRealMotorPos(RaToRaMotorDeg(desiredRa, lst+timeToDesired/3600000.0), scopeEast ? desiredDec-90.0 : 0-(desiredDec-90.0), a1, a2);
	//trace("tra: %f(%f) tdec:%f(%f) %s\r\n", desiredRa, a1, desiredDec, a2, scopeEast?"east":"west");
	motor1.goAtTime(a1, timeToDesired); motor2.goAtTime(a2, timeToDesired);
}
