//******************************************************************************************
//  File: IS_DCMotor_ShadeControl.h
//  Authors: Tim O'Callaghan based on Is_DoorControl.h
//
//  Summary:  IS_DCMotor_ShadeControl is a class which implements the SmartThings "Shade" device capability for a DC Motor.  It features
//			  motor control and the use of a timer and optional switch for both open and close,although at least 1 switch should be used.
//            When a switch is used the timer should be set so that in normal operation the switch will trip before the timer.
//			  The inputs will be checked in update.  There is 2 outputs to control direction and a PWM 'analog' output to control speed for the L298N motor controller
//            If you update the open or closed timeouts it will be stored in EEPROM so it won't be forgotten
//			  It clones much from the st::Executor Class
//
//			  Create an instance of this class in your sketch's global variable section
//			  For Example:  st::IS_DCMotor_ShadeControl sensor3(F("windowShade1"), PIN_OPEN_SWITCH, 60,0,70,LOW, true, PIN_MOTOR_OPEN,PIN_MOTOR_CLOSED,PIN_MOTORENABLE, 500,open, false);
//
//			  st::IS_DCMotor_ShadeControl() constructor requires the following arguments
//				- String &name - REQUIRED - the name of the object - must match the Groovy ST_Anything DeviceType tile name
//				- byte pinOpenSW - REQUIRED - the Arduino Pin to be used as a digital input for open switch   (if not used set to 0, then timer will stop for opening)
//				- long openTimeLimit - REQUIRED - the number of seconds to run shade to open if no switch encountered
//				- byte pinClosedSW - REQUIRED - the Arduino Pin to be used as a digital input for closed switch  (if not used set to 0, then time will stop down)
//				- long closedTimeLimit - REQUIRED - the number of seconds to run shade to close if no switch encountered
//				- bool interruptActiveState - REQUIRED - LOW or HIGH - determines which value indicates the interrupt is true for both inputs
//				- bool internalPullup - REQUIRED - true == INTERNAL_PULLUP for both inputs
//				- byte pinOutputOpen - REQUIRED - the Arduino Pin to be used as a digital output for open
//				- byte pinOutputClose - REQUIRED - the Arduino Pin to be used as a digital output for close
//				- byte pinMotorEnablePWM - REQUIRED - the Arduino Pin to be used as a digital PWM output -simulate analog
//              - long PWMSpeedValue to output on PWM enable output
//				- state desiredStartState - REQUIRED - (open or closed) -pick the one that has a real switch
//				- bool invertOutputLogic - REQUIRED - determines whether the Arduino Digital Outputs should use inverted logic
//
//
//  Change History:
//
//    Date        Who            What
//    ----        ---            ----
//    2020-12-31  Tim OC         Original Creation
//
//
//******************************************************************************************

#ifndef ST_IS_DCMOTOR_SHADECONTROL_H
#define ST_IS_DCMOTOR_SHADECONTROL_H

enum state {dummy=0,open=1,opening=2,closed=3,closing=4,unknown=5};
enum command {Dummy=0,Open=1,Close=2, Stop=3};

#include "Sensor.h"

namespace st
{
	class IS_DCMotor_ShadeControl:public Sensor
	{
		private:
			//inherits everything necessary from parent InterruptSensor Class for the Contact Sensor

			//following are for the digital inputs
			byte m_nPinSWOpened;                                        //set switch to 0 if no switch
			byte m_nPinSWClosed;                                        //set switch to 0 if no switch
			unsigned int m_lOpenTimeLimitUser;  //from caller
			unsigned int m_lOpenTimeLimit;      //could be diff than caller if changed on groovy pages
			unsigned int m_lCloseTimeLimitUser; //from caller
			unsigned int m_lCloseTimeLimit;     //could be diff than caller if changed on groovy pages
			unsigned long m_lTimeOperationDone;
			bool m_bInternalPullup;
            bool m_bInterruptActiveState; 

    		//following are for the digital outputs
     		byte m_npinMotorOutputOpen;
			byte m_npinMotorOutputClose;
            byte m_npinMotorOutputEnablePWM;
			unsigned long m_lMotorPWMSpeed;
			bool m_bInvertLogic;	//determines whether the Arduino Digital Output should use inverted logic

			//current state open,opening,closed,closing,unknown
			state m_eCurrentState;	
			
			bool m_bTimerPending;		//true if waiting on timer to expire
            state m_eDesiredStartingState;
			void controlMotor(command);	//function to Open, Close,Stop
		    int readPin(byte pin);  //read digital input if pin is valid
			void CancelTimer();
			void ReadTimerValues(unsigned int &open,unsigned int &close);
            void WriteTimerValues(unsigned int open,unsigned int close);

		public:
			//constructor - momentary output - called in your sketch's global variable declaration section
        	IS_DCMotor_ShadeControl(const __FlashStringHelper *name, byte pinOpenSW,unsigned long openTimeLimit,byte pinClosedSW,long closedTimeLimit,  bool interruptActiveState, bool internalPullup, byte pinMotorOutputForward,byte pinMotorOutputReverse, byte pinMotorEnablePWM, unsigned long PWMSpeedValue, state desiredStartingState, bool invertOutputLogic);
			
			//destructor
			virtual ~IS_DCMotor_ShadeControl();
			
			//initialization function
			virtual void init();

			//update function 
			void update();

			//SmartThings Shield data handler (receives command to turn "Open, Close or Stop the motor  (digital output)
			virtual void beSmart(const String &str);

			//called periodically by Everything class to ensure ST Cloud is kept consistent with the state of the contact sensor
			virtual void refresh();
/* 
			//handles what to do when interrupt is triggered 
			virtual void runInterrupt();

			//handles what to do when interrupt is ended 
			virtual void runInterruptEnded(); */

			//gets
			//virtual byte getPin() const { return m_nOutputPin; }

	};
}


#endif