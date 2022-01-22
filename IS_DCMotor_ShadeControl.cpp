//******************************************************************************************
//  File: IS_DCMotor_ShadeControl.cpp
//  Authors: Tim O'Callaghan
//
//  See .h for details
//
//  Change History:
//
//    Date        Who            What
//    ----        ---            ----
//    2020-12-31  Tim OCallaghan
//
//
//******************************************************************************************

#include "IS_DCMotor_ShadeControl.h"

#include "Constants.h"
#include "Everything.h"
#include <EEPROM.h>

//open always means move shade to let light in
//close always means move shade to block light


namespace st

{
//private
    //valid commands are Stop, Open and Close

	void IS_DCMotor_ShadeControl::controlMotor(command c) 
	{ 
        const char* stateStr[] = {"dummy","open", "opening", "closed","closing","unknown"};

        if (c == Open) {
	         Serial.println("DEBUG-IS_DCMotor_ShadeControl::controlmotor open");
		     digitalWrite(m_npinMotorOutputOpen, m_bInvertLogic ? LOW:HIGH); 
			 digitalWrite(m_npinMotorOutputClose,m_bInvertLogic ? HIGH:LOW);
             analogWrite(m_npinMotorOutputEnablePWM, m_lMotorPWMSpeed); 	

     		 //Save time operation limit
             m_lTimeOperationDone = millis() + (1000 * m_lOpenTimeLimit);		 

     		 //Increment number of active timers if no timers active
			 if (!m_bTimerPending) {
     		    st::Everything::bTimersPending++;
			     m_bTimerPending = true;
			 }
			 
			 m_eCurrentState = opening;	
			 
    		//Queue the door status update the ST Cloud 
	    	Everything::sendSmartStringNow(getName() + " " + stateStr[m_eCurrentState] );					


	   } else if (c == Close) {
   	         Serial.println("DEBUG-IS_DCMotor_ShadeControl::controlmotor closing");
		     digitalWrite(m_npinMotorOutputOpen,  m_bInvertLogic ? HIGH : LOW);
			 digitalWrite(m_npinMotorOutputClose,  m_bInvertLogic ? LOW : HIGH);
             analogWrite(m_npinMotorOutputEnablePWM, m_lMotorPWMSpeed); 	

			//Save time operation limit
			m_lTimeOperationDone = millis() + 	(1000 * m_lCloseTimeLimit);	

    		//Increment number of active timers
     		st::Everything::bTimersPending++;
			m_bTimerPending = true;		

			m_eCurrentState = closing;	

     		//Queue the door status update the ST Cloud 
	    	Everything::sendSmartStringNow(getName() + " " +  stateStr[m_eCurrentState] );					
			
       } else if (c == Stop) {
		   
		     //DO NOT SET THE STATE HERE - should be done in update
   	         Serial.println("DEBUG-IS_DCMotor_ShadeControl::controlmotor Stop");
		     digitalWrite(m_npinMotorOutputOpen,  m_bInvertLogic  ? HIGH : LOW);
			 digitalWrite(m_npinMotorOutputClose,  m_bInvertLogic ? HIGH : LOW);
             analogWrite(m_npinMotorOutputEnablePWM,0); 	

             CancelTimer();
	      } else {
			  
		     Serial.print(F("DEBUG-IS_DCMotor_ShadeControl::controlmotor -unsupported command="));
  		     Serial.println(c);
	   }	   

	}

//public

//constructor - called in your sketch's global variable declaration section
	IS_DCMotor_ShadeControl::IS_DCMotor_ShadeControl(const __FlashStringHelper *name, byte pinSWOpen,unsigned long openTimeLimit,byte pinSWClosed,long closedTimeLimit,  bool interruptActiveState, bool internalPullup, byte pinOutputOpen,byte pinOutputClose, byte pinMotorEnablePWM, unsigned long PWMSpeedValue, state desiredStartingState, bool invertOutputLogic):Sensor(name),
		m_nPinSWOpened(pinSWOpen),
		m_lOpenTimeLimitUser(openTimeLimit),
		m_nPinSWClosed(pinSWClosed),
		m_lCloseTimeLimitUser(closedTimeLimit),
		m_bInterruptActiveState(interruptActiveState),
		m_bInternalPullup(internalPullup),
		m_npinMotorOutputOpen(pinOutputOpen),
		m_npinMotorOutputClose(pinOutputClose),
        m_npinMotorOutputEnablePWM(pinMotorEnablePWM),
		m_lMotorPWMSpeed(PWMSpeedValue),
		m_eDesiredStartingState(desiredStartingState),    //note the desired starting state should have a switch on it- do not 
		m_bInvertLogic(invertOutputLogic),
		m_lTimeOperationDone(0),
		m_bTimerPending(false),
		m_eCurrentState(unknown)
		{

		//setup input pins if defined
        if (m_nPinSWClosed!=0) {
				pinMode(m_nPinSWClosed, (m_bInternalPullup)?INPUT_PULLUP:INPUT);
		}

        if (m_nPinSWOpened!=0) {
				pinMode(m_nPinSWOpened, (m_bInternalPullup)?INPUT_PULLUP:INPUT);
		}
			
		//define output pins
		pinMode(m_npinMotorOutputOpen,OUTPUT);
		pinMode(m_npinMotorOutputClose,OUTPUT);
        pinMode(m_npinMotorOutputEnablePWM, OUTPUT);
	}
		
//destructor
	IS_DCMotor_ShadeControl::~IS_DCMotor_ShadeControl()
	{
	}
	
//init
	void IS_DCMotor_ShadeControl::init()
    {
        const char* stateStr[] = {"dummy","open", "opening", "closed","closing","unknown"};
	
		//check starting state of switches
		if (readPin(m_nPinSWClosed) == HIGH) {
			m_eCurrentState = closed;
		} else if (readPin(m_nPinSWOpened) == HIGH) {
			m_eCurrentState = open;
		} else {
			m_eCurrentState = unknown;
		}	
		
		//get values from eeprom
		ReadTimerValues(m_lOpenTimeLimit,m_lCloseTimeLimit);
		
		//if too high user the users input
		if ((m_lOpenTimeLimit> 150) || (m_lCloseTimeLimit>150)) {
			//use input values
			m_lOpenTimeLimit = m_lOpenTimeLimitUser;
			m_lCloseTimeLimit = m_lCloseTimeLimitUser;
            WriteTimerValues(m_lOpenTimeLimit,m_lCloseTimeLimit);		
        }	
				
		Everything::sendSmartStringNow(getName() + " opentimeout:" + m_lOpenTimeLimit );
		Everything::sendSmartStringNow(getName() + " closetimeout:" + m_lCloseTimeLimit );
		
		if ((m_eDesiredStartingState == open) && (m_eCurrentState != open)) {
				controlMotor(Open);
		} else if ((m_eDesiredStartingState == closed) && (m_eCurrentState != closed)) {
		      	controlMotor(Close);
		} else {	
		    Everything::sendSmartStringNow(getName() + " " + stateStr[m_eCurrentState] );   
        }
		

	}

//update function 
	void IS_DCMotor_ShadeControl::update() {
        const char* stateStr[] = {"dummy","open", "opening", "closed","closing","unknown"};
        bool stopmotor = false;

		//open switch defined, opening and open switch hit
		if ((m_nPinSWOpened != 0) && (m_eCurrentState == opening) && readPin(m_nPinSWOpened)) {
			   Serial.println(F("DEBUG-IS_ShadeControl::update - hit opened switch"));
			   m_eCurrentState = open;
			   stopmotor=true;
		//opening and hit timeout	   
		} else if 	((m_bTimerPending) && (m_eCurrentState == opening) && (millis() > m_lTimeOperationDone)) {
			   Serial.println(F("DEBUG-IS_ShadeControl::update - hit opened timeout"));
			     //set to stopmotor and set state to open if no switch,otherwise the switch should have hit and it didnt so say unknown
    			 stopmotor=true;
				  m_eCurrentState = (m_nPinSWOpened ==0)?open:unknown;
		//closed switch defined, closing and closed switch hit
        } else if   ((m_nPinSWClosed != 0) &&  (m_eCurrentState == closing) && readPin(m_nPinSWClosed) )  {
			   Serial.println(F("DEBUG-IS_ShadeControl::update - hit closed switch"));
               //update state
			   m_eCurrentState = closed;
			   stopmotor=true;
		//closing and hit timeout	   
		} else if 	((m_bTimerPending) && (m_eCurrentState == closing) && (millis() > m_lTimeOperationDone)) {
			   Serial.println(F("DEBUG-IS_ShadeControl::update - hit closing timeout"));
			
			     //set stopmotor and set state to closed if no switch,otherwise the switch should have hit and it didnt so say unknown
     			  stopmotor=true;
				  m_eCurrentState = (m_nPinSWClosed ==0)?closed:unknown;
        }

		if (stopmotor) {     

			//stop motor
			controlMotor(Stop);

     		Everything::sendSmartStringNow(getName() + " " + stateStr[m_eCurrentState] );

			Serial.print(F("DEBUG-IS_ShadeControl::update - sending state to hub: "));
			Serial.println(stateStr[m_eCurrentState]);
        }
 }

 

//beSmart function
// windowShade open
// windowShade close
// windowShade setclosetimeout:nnnnn
// windowShade setopentimeout:nnnnn
	void IS_DCMotor_ShadeControl::beSmart(const String &str)
	{
		String s = str.substring(str.indexOf(' ') + 1);
		int colonloc = str.indexOf(':');

		String timeout = str.substring(str.indexOf(':') + 1);
	
		if (st::Sensor::debug) {
			Serial.print(F("IS_DCMotor_ShadeControl::beSmart s = "));
			Serial.println(s);
		}

		if (s == F("open")) {
		{
			Serial.println("DEBUG beSmart open path");
			//if not in open state and no timer pending OR there is a valid open switch AND we are not at the open
			 //if  ((m_eCurrentState != open) && (((!m_bTimerPending) || (m_nPinSWOpened>0)) && !readPin(m_nPinSWOpened))) {
	
            //if no timer and not already at open state and state is closed OR you have a valid open sw and open sw not active
			//if ((!m_bTimerPending) && (m_eCurrentState != open) && ((m_eCurrentState == closed) || ((m_nPinSWOpened>0) && !readPin(m_nPinSWOpened)))) {

            //if normal state of closed and no timer   OR  valid open switch and its not active
			if (((m_eCurrentState == closed) && (!m_bTimerPending)) || ((m_nPinSWOpened>0) && !readPin(m_nPinSWOpened))) {
	
			 			controlMotor(Open);		
			 } else {
				 if (st::Sensor::debug) {
	        	     Serial.println("conditions not met to call controlMotor with open ");
				 }		 
             }
					  
   			}
			
		}  else if (s == F("close")) {
						Serial.println("DEBUG beSmart close path");
						Serial.print("current state:");
	                    Serial.println(m_eCurrentState);
						Serial.print("timer pending:");
						Serial.println(m_bTimerPending);

            
           //if normal state of open and no timer   OR  valid closed switch and its not active
			if (((m_eCurrentState == open) && (!m_bTimerPending)) || ((m_nPinSWClosed>0) && !readPin(m_nPinSWClosed))) {
			
			//if no timer and not already at closed state and state is open OR you have a valid close sw and close sw not active
            //if ((!m_bTimerPending) && (m_eCurrentState != closed) && ((m_eCurrentState == open) || ((m_nPinSWClosed>0) && !readPin(m_nPinSWClosed)))) {
			  
            //only close when no timers or have a close switch AND not at the close switch
            //if (((!m_bTimerPending) || (m_nPinSWClosed>0)) && !readPin(m_nPinSWClosed)) {

            // commented out ..but before only allowed close when opem 
			//if ((m_eCurrentState == open) && (!m_bTimerPending)) {
				
     			controlMotor(Close);	

			   //Queue the door status update the ST Cloud 
			   Everything::sendSmartStringNow(getName() +  F(" closing") );				
			}

		} else if (s == F("stop")) {
						Serial.print(F("DEBUG beSmart stop path"));
						Serial.print("current state:");
	                    Serial.println(m_eCurrentState);
						Serial.print("timer pending:");
						Serial.println(m_bTimerPending);
             			controlMotor(Stop);	

        //check for the set open and close timers
		} else if (colonloc> -1) {
			if (str.indexOf("setclosetimeout")>-1) {
			     m_lCloseTimeLimit = timeout.toInt();
                 Serial.print("Setting close timeout and sending info back to hub");
        		 Everything::sendSmartStringNow(getName() + " closetimeout:" + timeout );

			} else if (str.indexOf("setopentimeout")>-1) {	 
				 m_lOpenTimeLimit = timeout.toInt();
				 Serial.print("Setting open timeout and sending info back to hub");
				 Everything::sendSmartStringNow(getName() + " opentimeout:" + timeout );
            }

            //write to EEPROM    
	        WriteTimerValues(m_lOpenTimeLimit,m_lCloseTimeLimit);		
		}	
		
	}


	//called periodically by Everything class to ensure ST Cloud is kept consistent with the state of the contact sensor
//refresh function	
	void IS_DCMotor_ShadeControl::refresh()
	{
        const char* stateStr[] = {"dummy","open", "opening", "closed","closing","unknown"};
    	Serial.println(F("DEBUG-IS_ShadeControl::refresh - sending state,and timeouts to hub"));

		Everything::sendSmartStringNow(getName() + " " + stateStr[m_eCurrentState] );   
	    Everything::sendSmartStringNow(getName() + " opentimeout:" + m_lOpenTimeLimit );
		Everything::sendSmartStringNow(getName() + " closetimeout:" + m_lCloseTimeLimit );
		Serial.println(m_eCurrentState);
	}

    void IS_DCMotor_ShadeControl::CancelTimer()
	{
		if (st::Everything::bTimersPending > 0) st::Everything::bTimersPending--;
		m_bTimerPending = false;
	}

//readPin function	
	int IS_DCMotor_ShadeControl::readPin(byte pin)
	{
        if (pin == 0) {
			return LOW;
		} else {	
			int value = digitalRead(pin);

			if ((value == HIGH) && m_bInterruptActiveState) {
 				return HIGH;
			} else if ((value == LOW) && !m_bInterruptActiveState) {
				return HIGH;
			} else {
				return LOW;
			}	
		}
    }	

//ReadTimerValues
   void IS_DCMotor_ShadeControl::ReadTimerValues(unsigned int &open,unsigned int &close) {
	   const int eepromAdress_start = 0;

	    #if defined(ESP8266)|| defined(ESP32)
           EEPROM.begin(512);  
        #endif
        EEPROM.get(eepromAdress_start, open); 
        EEPROM.get(eepromAdress_start + sizeof(int), close); 
        Serial.print("read eprom open:");
        Serial.println(open);
        Serial.print("read eprom close:");
        Serial.println(close);

   }

//WriteTimerValues
  void IS_DCMotor_ShadeControl::WriteTimerValues(unsigned int open,unsigned int close) {
	  const int eepromAdress_start = 0;
                 Serial.print("writing eeprom close:");
                 Serial.println(close);

	    #if defined(ESP8266)|| defined(ESP32)
           EEPROM.begin(512);  
        #endif
		EEPROM.put(eepromAdress_start, open);
        EEPROM.put(eepromAdress_start + sizeof(int), close); 
    	#if defined(ESP8266)|| defined(ESP32)
	    	EEPROM.commit();
		#endif

   }
	
}


