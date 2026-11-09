// Stepper Motor class
// Handles the motor step position and a step to user unit convertion
// Will send steps and contol a TMC2209 motor
// You control the motor using:
//   goTo to go to a pos at max speed
//   goAtTime to ask the motor to be at a given position in a given time interval
//   goAtSpeed to get the motor moving at a given speed
//   stop to stop as fast as possible.
// step must be called at maxStepsPerSecond Hertz to issue a HW step (typically in an IRQ)
// quantize must be called at quantizationPerSecond Hertz to plan how often steps need to be taken (typically in a thread)
//
class CMotor { public:
	static int const quantizationPerSecond= 100;
	static int32_t const maxStepsPerSecond= 10000;
	double inline posInReal() { return pos*posToReal+minReal; }
	uint32_t inline getPos() { return pos; } // get physical position. used by serial monitoring
	double inline maxSpdInReal() { return maxSpd*posToReal; }
	void goAtTime(double real, int ms) // ms can not be 0!
	{
		double dst= realToPosd(real);
		goTo(uint32_t(dst), uint32_t(fabs(dst-pos)*1000/ms));
	}
	void goTo(double real) { goTo(realToPos(real), maxSpd); }
	void goAtSpeed(double speed) { goTo(speed>=0?maxPos:0, uint32_t(fabs(speed)/posToReal)); } // speed is in real unit per second...
	void goToPos(uint32_t dst) { goTo(imin(dst, maxPos), maxSpd); } // used by park and the like

	void init(double minReal, double maxReal, double realPos, uint32_t maxPos, uint32_t maxSpd, uint32_t maxAcc, bool invertDir, uint32_t stepPin, uint8_t UARTAdr, uint8_t IHold)
	{ 
		this->maxPos= maxPos; this->maxSpd= maxSpd; this->maxAcc= maxAcc; this->invertDir= invertDir;
		posToReal= (maxReal-minReal)/maxPos;
		this->minReal= minReal;
		setPos(realPos);
		beginTMC(UARTAdr, IHold); 
		stepMask= 1<<stepPin;
		GPIO::output(stepMask); GPIO::clearMask(stepMask);
		stepSize= 1, quantizedMaxAcc= maxAcc/quantizationPerSecond;
	}
	// not thread safe, but only used at init time (and in windows simulation)
	void setPos(double real)
	{
		pos= dstPos= realToPos(real); 
		requestedSpeed= currentSpeed= nextStep= stepInterval= 0;
	}
	bool isMoving() { return dstPos!=pos; }
	double stopDistanceReal() // return the distance to stop in real units at max deceleration
	{
		uint32_t dist= uint64_t(currentSpeed)*currentSpeed/maxAcc/2; // d=1/2at^2 t=v/a ->  d=1/2av^2/a^2 -> d=1/2v^2/a
		return dist*posToReal*(currentSpeed<0?-1:1);
	}
	void stop() // stop when you can by deceleration at max speed...
	{ 
		// not thread safe, but if quantize while here, currentSpeed=0 which results
		// in dst=pos and will cause the motor to try to stop "here" rather than at dst and will cause an overshoot and return
		// so not a problem.
		// only used in focusser at this point in time
		// here we calculate where to "go" to stop as fast as possible...
		uint32_t dist= uint64_t(currentSpeed)*currentSpeed/maxAcc/2; // d=1/2at^2 t=v/a ->  d=1/2av^2/a^2 -> d=1/2v^2/a
		if (currentSpeed<0) if (pos<dist) dstPos= 0; else dstPos= pos-dist;
		else if (pos+dist>=maxPos) dstPos= maxPos; else dstPos= pos+dist;
	}
private:
	uint32_t pos, maxPos; double posToReal, minReal;
	uint32_t maxSpd; int32_t maxAcc; // in steps/s ans steps²/s
	uint32_t stepMask; bool invertDir;
	uint32_t inline realToPos(double real) { return uint32_t((real-minReal)/posToReal); }
	double inline realToPosd(double real) { return (real-minReal)/posToReal; }
	void goTo(uint32_t dst, uint32_t speed) // assumes pos is valid!
	{
		// stop quantization for the setup here? move steps calculations somewhere else?
		if (speed>maxSpd) speed= maxSpd;
		if (speed>200)
		{
			int a= 0;
		}
		uint32_t steps= 0; while (requestedSpeed>maxStepsPerSecond<<steps) steps++; // find microstep multiplicator
		dstPos= (dst+(1<<steps)/2) & (0xffffffff<<steps); // make sure that pos is a multiple of step size
		requestedSpeed= speed;
	}

	uint32_t dstPos;
	int32_t requestedSpeed, lastRequestedSpeed; // in steps/s sign is direction...
	int32_t currentSpeed;
	uint32_t nextStep, stepInterval; int32_t quantizedMaxAcc, stepSize;
	static int32_t inline abs(int32_t v) { if (v<0) return -v; return v; }
	static int32_t imax(int32_t a, int32_t b) { if (a>b) return a; return b; }
	static int32_t imin(int32_t a, int32_t b) { if (a<b) return a; return b; }
public:
	// This function can block on send microsteps and directions.
	// This can be up to 10ms due to OS tick time (regardless of the time it take to send, it waits 1 os tick)
	// Since quantization is at 10ms, this means that I can block one quantization from another motor... which is not a big issue.
	// It does mean that step issue for THIS motor can be blocked for 10ms. But once again not a problem as this would happen
	// only at start time, or when changing direction where a 20ms delay is not problematic.
	// Best solution would be an IRQ which releases steps, but needs to be carefull about which motor needs releasing!
	void IRAM_ATTR quantize()
	{
		int32_t spd= atomic_exchange(currentSpeed, 0); // This acts as a guard for the step interrupt function
		int32_t dist= dstPos-pos;
		if (dist==0 && spd==0) return; // at destination and slow enough... do nothing
		int32_t newSpeed; // will contain the updated speed that we need to go at...
		if (spd==0) newSpeed= (dist>0) ? imin(requestedSpeed, quantizedMaxAcc) : -imin(requestedSpeed, quantizedMaxAcc); // starting acceleration
		else if (dist!=0 && (dist^spd)>=0) // going in the right direction, accelerate unless we need to slow down to reach destination...
		{
			// how many steps will it take to get to speed=0 from current speed?
			// d=1/2at^2 t=v/a ->  d=1/2av^2/a^2 -> d=1/2v^2/a
			// we can then compare with dstPos-pos and see if we can stop in time at max deceleration!
			uint32_t decelerationDistance= uint64_t(spd)*spd/maxAcc/2; // can curSpeed be > 32 bits? to be verified (note, the square is always positive!)
			if (uint32_t(abs(dist))>=decelerationDistance) // no problem to stop from here, so we accelerate as much as possible
			{
				if (spd>0) newSpeed= spd+quantizedMaxAcc; else newSpeed= spd-quantizedMaxAcc; // accelerate, with clamping...
				if (newSpeed>requestedSpeed) newSpeed= requestedSpeed; else if (newSpeed<-requestedSpeed) newSpeed= -requestedSpeed;
			} else goto Decelerate;
		} else { Decelerate: // going the wrong way! need to inverse speed (slow down and go past 0 the other way around)
			// if speed <0, increase, and make sure speed is never 0
			if (spd<-quantizedMaxAcc) newSpeed= spd+quantizedMaxAcc; 
			else if (spd<0) newSpeed= imin(requestedSpeed, quantizedMaxAcc); // switch direction!
			// if speed >0, decreese, and make sure speed is never 0
			else if (spd>quantizedMaxAcc) newSpeed= spd-quantizedMaxAcc;
			else newSpeed= -imin(requestedSpeed, quantizedMaxAcc); // switch direction!
		}
		// if new speed is different recalculate stuff...
		if (newSpeed!=spd)
		{
			needToWait= false;
			if (abs(newSpeed)>abs(spd)) // if we are accelerating, then check microstepping based on desired speed
			{
				// use microstepping to reduce the number of steps per second to send...
				// at 10khz steps, for an eq5 with 22million steps/turn, it would take 2000s for a full turn without this trick!
				// here we can do a /256 for a max speed of 1 turn in 8s :-)
				uint32_t steps= 0; while (requestedSpeed>maxStepsPerSecond<<steps) steps++; // find microstep multiplicator
				microsteps3(steps); // there is a cache here so no send if not needed
				stepSize= 1<<steps;
				pos= (pos+stepSize/2) & (0xffffffff<<steps); // make sure that pos is a multiple of step size
				// What is dstPos is > maxPos due to rounding?
			}

			shaft3((newSpeed>=0) ^ invertDir);  // there is a cache here so no send if not needed
			sendPacket2Wait(); // wait if needed If waiting happends, this will take 10ms due to OS scheduling...

			if (newSpeed!=0) // calculate interval....
			{
				uint32_t interval= 1000000*stepSize/abs(newSpeed);
				if (spd==0) nextStep= uint32_t(nowus()); // we were stopped, do a step asap...
				else if (interval<stepInterval) nextStep= nextStep-stepInterval+interval; // need to shrink step size, reprogram as previous programation might have been in a VERY long time...
				stepInterval= interval;
			} // else, we stop, on next move spd will be 0 and cause a reprogram of nextStep to now...
		}
		currentSpeed= newSpeed; // "release" step
	}
	// depends on variables currentSpeed, nextStep, stepInterval, pos and dstPos
	// currentSpeed, nextStep, stepInterval are only changed in quantize where currentSpeed acts as a "guard" with an atomic exchange
	void IRAM_ATTR step(uint32_t us) // this is not interuptible by quantize and currentSpeed==0 serves as a "guard".
	{
		if (currentSpeed==0) return;         // motor stopped
		if (int32_t(nextStep-us)>0) return;  // no steps needed at this time
		nextStep+= stepInterval;             // schedule next step
		GPIO::toggleMask(stepMask);          // issue step
		if (currentSpeed>0)                  // calculate new pos depending on direction
		{
			pos+= stepSize;
			if (abs(pos-dstPos)<stepSize && currentSpeed<=maxAcc*4) currentSpeed= 0; // stop if arrived and not too brutal. else we continue and will return
			if (pos>=maxPos) currentSpeed= 0; // hard stop on limit
		}
		else
		{
			pos-= stepSize;
			if (abs(pos-dstPos)<stepSize && currentSpeed>=-maxAcc*4) currentSpeed= 0; // stop if arrived and not too brutal. else we continue and will return
			if (pos==0) currentSpeed= 0;     // hard stop on limit
		}
	}
#ifdef STATS // used on windos fopr debugging
	void stats(TStat &spos, TStat &sdst, TStat &smaxSpd, TStat &sreqSpd, TStat &sspd)
	{
		if (stopStats) return;
		spos.v.push_back(pos); sdst.v.push_back(dstPos); smaxSpd.v.push_back(maxSpd); sspd.v.push_back(currentSpeed); sreqSpd.v.push_back(requestedSpeed);
	}
#endif
private:
	// Stuff here is about sending serial packets to TMC2209
	// We have 3 controls: motor ON/OFF, motor direction, and number of microsteps/steps
    uint8_t GCONF[8], CHOPCONF[8];
    void beginTMC(uint8_t UARTAdr, uint8_t IHold)
    {
		#ifdef ESP_PLATFORM
		    // uart at 400kb/s. Send 1 message (8 bytes) = 0.2ms
			const uart_config_t uart_config = { .baud_rate= 400000, .data_bits= UART_DATA_8_BITS, .parity= UART_PARITY_DISABLE, .stop_bits= UART_STOP_BITS_1, .flow_ctrl= UART_HW_FLOWCTRL_DISABLE };
			if (uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0)==ESP_OK) // will fail on calls after call 1.... so will only install the driver once for all motors...
			{
				uart_param_config(UART_NUM_1, &uart_config);
				uart_set_pin(UART_NUM_1, stepperUartPin, -1, -1, -1);
			}
		#endif
		uint8_t const IGCONF[8]=    { 5, UARTAdr, 0|0x80, 0, 0, 1, 0b11000001 };       memcpy(GCONF, IGCONF, 8);       // GCONF.pdn_disable=1, GCONF.mstep_reg_select = 1 (use MRES for step count)
        uint8_t const ICHOPCONF[8]= { 5, UARTAdr, 0x6C|0x80, 0x30, 0x00, 0x00, 0x53 }; memcpy(CHOPCONF, ICHOPCONF, 8); // TOFF=3, microsteps=0(256), INTPOL=1, HSTRT= 5, TBL=0, double edge on
        sendPacket(GCONF, 8); sendPacket(CHOPCONF, 8);
        uint8_t IHOLD_IRUN[8]= { 5, UARTAdr, 0x10|0x80, 0, 1, 31, IHold }; sendPacket(IHOLD_IRUN, 8); // set power in hold mode to 1/4 power...
    }
	static uint8_t swuart_calcCRC(uint8_t const * s, uint8_t l)
	{
		uint8_t crc= 0; // CRC located in last byte of message
		do {
			uint8_t currentByte = *s++; // Retrieve a byte to be sent from Array
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 0)) & 1) * 0x07);
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 1)) & 1) * 0x07);
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 2)) & 1) * 0x07);
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 3)) & 1) * 0x07);
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 4)) & 1) * 0x07);
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 5)) & 1) * 0x07);
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 6)) & 1) * 0x07);
			crc = (crc << 1) ^ ((((crc >> 7) ^ (currentByte >> 7)) & 1) * 0x07);
		} while (--l!=0);
		return crc;
	}
	void sendPacket(uint8_t *s, uint8_t size)
    {
		#ifdef ESP_PLATFORM
			s[size-1]= swuart_calcCRC(s, size-1);
			uart_write_bytes(UART_NUM_1, s, size);
			uart_wait_tx_done(UART_NUM_1, 1); // flush... This ensures that no steps are sent with the wrong configuration...
		#endif
    }

	// stop/start stepper 0:stop stepper, 3:start stepper
    void TOFF(uint8_t val) { CHOPCONF[6]= (CHOPCONF[6]&~15)|val; sendPacket(CHOPCONF, 8); }
    // change number of microsteps per steps. 0=256microsteps, 1:128, ..., 7:2, 8:1
	void inline microsteps(int val) { CHOPCONF[3]= (CHOPCONF[3]&~15)|val; sendPacket(CHOPCONF, 8); }
	void inline microsteps2(int val) { if ((CHOPCONF[3]&15)!=val) microsteps(val); } // only send if needd
	// change shaft direction 0 or 1
	void inline shaft(bool dir) { GCONF[6]= (GCONF[6]&~8)|(dir?8:0); sendPacket(GCONF, 8); }
	void inline shaft2(bool dir) { if ((GCONF[6]&8)!=(dir?8:0)) shaft(dir); } // only send if needd

	// These are version that will allow to do a "wait" only once at the end. This is usefull as the "wait" typically will take 10ms as this is the OS quantization speed.
	bool needToWait= false;
	void inline microsteps3(int val) { if ((CHOPCONF[3]&15)==val) return; CHOPCONF[3]= (CHOPCONF[3]&~15)|val; sendPacket2(CHOPCONF); } // only send if needd
	void inline shaft3(bool dir) { if ((GCONF[6]&8)==(dir?8:0)) return; GCONF[6]= (GCONF[6]&~8)|(dir?8:0); sendPacket2(GCONF); } // only send if needd
	void sendPacket2(uint8_t *s)
	{
		s[7]= swuart_calcCRC(s, 7);
		needToWait= true;
#ifdef ESP_PLATFORM
		uart_write_bytes(UART_NUM_1, s, 8);
#else
		trace("MotorUart-> (%x:%d): %02x %02x %02x %02x\r\n", s[2], s[1], s[3], s[4], s[5], s[6]); // print out on PC
#endif
	}
	void sendPacket2Wait()
	{
#ifdef ESP_PLATFORM
		if (needToWait) uart_wait_tx_done(UART_NUM_1, 1); // flush... This ensures that no steps are sent with the wrong configuration...
#endif
	}
};
