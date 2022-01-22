//******************************************************************************************
// File: ST_Anything_DCMotor_Shades.ino
// Authors: Dan G Ogorchock & Daniel J Ogorchock (Father and Son)
//
// Summary: This Arduino Sketch, along with the ST_Anything library and the revised SmartThings
//          library, demonstrates the ability of one NodeMCU ESP8266 to
//          implement a multi input/output custom device for integration into SmartThings.
//
//          The ST_Anything library takes care of all of the work to schedule device updates
//          as well as all communications with the NodeMCU ESP8266’s WiFi.
//
//          IS_DCMotor_ShadeControl implements the following Hubitat Shade Capabilities with a DC motor, DC motor controller and mechanical switches
//
//
// Change History:
//+
//    Date        Who            What
//    ----        ---            ----
//    2019-10-31  Dan Ogorchock  Original Creation
//    2022-01-20  Tim O          Modified for DC Shade, Illuminance and Motion
//    2022-01-21  Tim O          use map on illuminance so 0 is dark and 100 is brightest
//******************************************************************************************
//******************************************************************************************
// SmartThings Library for ESP8266WiFi
//******************************************************************************************
#include <SmartThingsESP8266WiFi.h>

//******************************************************************************************
// O T A 
//******************************************************************************************
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
const char* host = "shade-webupdate";
//const char* ssid = STASSID;
//const char* password = STAPSK;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

//******************************************************************************************
// ST_Anything Library
//******************************************************************************************
#include <Constants.h> //Constants.h is designed to be modified by the end user to adjust behavior of the ST_Anything library
#include <Device.h> //Generic Device Class, inherited by Sensor and Executor classes
#include <Sensor.h> //Generic Sensor Class, typically provides data to ST Cloud (e.g. Temperature, Motion, etc…)
#include <Executor.h> //Generic Executor Class, typically receives data from ST Cloud (e.g. Switch)
#include <InterruptSensor.h> //Generic Interrupt "Sensor" Class, waits for change of state on digital input
#include <PollingSensor.h> //Generic Polling "Sensor" Class, polls Arduino pins periodically
#include <Everything.h> //Master Brain of ST_Anything library that ties everything together and performs ST Shield communications

#include <IS_DCMotor_ShadeControl.h> //Implements DC Motor shade controller
#include <PS_Illuminance.h> //Illuminance
#include <IS_Motion.h>  //Motion
//*************************************************************************************************
//NodeMCU v1.0 ESP8266-12e Pin Definitions (makes it much easier as these match the board markings)
//                    -Do NOT uncomment these statements as they are defined by the board manager
//*************************************************************************************************
#define LED_BUILTIN 16
#define BUILTIN_LED 16

#define D0 16  //no internal pullup resistor
#define D1  5
#define D2  4
#define D3  0  //must not be pulled low during power on/reset, toggles value during boot
#define D4  2  //must not be pulled low during power on/reset, toggles value during boot
#define D5 14
#define D6 12
#define D7 13
#define D8 15  //must not be pulled high during power on/reset

//******************************************************************************************
//Define which Arduino Pins will be used for each device
//******************************************************************************************
#define PIN_ILLUMINANCE   A0
#define PIN_OPEN_SWITCH   D1
#define PIN_CLOSE_SWITCH  0     // not using close switch  D2
#define PIN_MOTION        D4    //was D3
#define PIN_MOTOR_ENABLE  D5
#define PIN_MOTOR_OPEN    D7
#define PIN_MOTOR_CLOSE   D6
//******************************************************************************************
//ESP8266 WiFi Information
//******************************************************************************************
String str_ssid     = "xxxxxxxxxx";                           //  <---You must edit this line!
String str_password = "xxxxxxxxxx";                   //  <---You must edit this line!
IPAddress ip(192, 168, 1, 143);       //Device IP Address       //  <---You must edit this line!
IPAddress gateway(192, 168, 1, 1);    //Router gateway          //  <---You must edit this line!
IPAddress dnsserver(192, 168, 1, 1);  //DNS server              //  <---You must edit this line!
IPAddress subnet(255, 255, 255, 0);   //LAN subnet mask         //  <---You must edit this line!
const unsigned int serverPort = 8090; // port to run the http server on

// Hubitat Hub Information
IPAddress hubIp(192, 168, 1, 118);    // hubitat hub ip         //  <---You must edit this line!
const unsigned int hubPort = 39501;   // hubitat hub port

//******************************************************************************************
//st::Everything::callOnMsgSend() optional callback routine.  This is a sniffer to monitor 
//    data being sent to ST.  This allows a user to act on data changes locally within the 
//    Arduino sktech.
//******************************************************************************************
void callback(const String &msg)
{
}

//******************************************************************************************
//Arduino Setup() routine
//******************************************************************************************
void setup() 
{

  
//******************************************************************************************
// O T A  Stuff
//******************************************************************************************
  Serial.begin(115200);
  Serial.println();
  Serial.println("Setting up OTA..");
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(str_ssid, str_password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(str_ssid, str_password);
    Serial.println("WiFi failed, retrying.");
  }

  MDNS.begin(host);

  httpUpdater.setup(&httpServer);
  httpServer.begin();
  Serial.println("MDSN.addService..");
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
  Serial.println("Fixing MAP on window light sensor..");
  delay(3000);
  //******************************************************************************************
  //Declare each Device that is attached to the Arduino
  //  Notes: - For documentation on each device's constructor arguments below, please refer to  
  //           the corresponding header (.h) and program (.cpp) files.
  //         - The new Composite Device Handler is comprised of a Parent DH and various Child
  //           DH's.  The names used below MUST not be changed for the Automatic Creation of
  //           child devices to work properly.  Simply increment the number by +1 for each duplicate
  //           device (e.g. valve1, valve2, valve3, etc...)  You can rename the Child Devices
  //           to match your specific use case in the Hubitat IDE  
  //******************************************************************************************
 //       st::IS_DCMotor_ShadeControl() constructor requires the following arguments
//        - String &name - REQUIRED - the name of the object - must match the Groovy ST_Anything DeviceType tile name
//        - byte pinOpenSW - REQUIRED - the Arduino Pin to be used as a digital input for open switch   (if not used set to 0, then shade will stop after openTimeLimit and be open)
//        - long openTimeLimit - REQUIRED - the number of seconds to run shade to open if no switch encountered
//        - byte pinClosedSW - REQUIRED - the Arduino Pin to be used as a digital input for closed switch  (if not used set to 0, then shade will stop after closeTimeLimit and be closed)
//        - long closedTimeLimit - REQUIRED - the number of seconds to run shade to close if no switch encountered
//        - bool interruptActiveState - REQUIRED - LOW or HIGH - determines which value indicates the interrupt is true for both inputs
//        - bool internalPullup - REQUIRED - true == INTERNAL_PULLUP for both inputs
//        - byte pinOutputOpen - REQUIRED - the Arduino Pin to be used as a digital output for open
//        - byte pinOutputClose - REQUIRED - the Arduino Pin to be used as a digital output for close
//        - byte pinMotorEnablePWM - REQUIRED - the Arduino Pin to be used as a digital PWM output -simulate analog
//        - long PWMSpeedValue to output on PWM enable output
//        - state desiredStartState - REQUIRED - (open or closed) -pick the one that has a real switch
//        - bool invertOutputLogic - REQUIRED - determines whether the Arduino Digital Outputs should use inverted logic



  static st::IS_DCMotor_ShadeControl sensor1(F("windowDCShade1"), PIN_OPEN_SWITCH,60,PIN_CLOSE_SWITCH,48, LOW, true, PIN_MOTOR_OPEN,PIN_MOTOR_CLOSE, PIN_MOTOR_ENABLE, 1000, open, false);

//        st::PS_Illuminance() constructor requires the following arguments
//        - String &name - REQUIRED - the name of the object - must match the Groovy ST_Anything DeviceType tile name
//        - long interval - REQUIRED - the polling interval in seconds
//        - long offset - REQUIRED - the polling interval offset in seconds - used to prevent all polling sensors from executing at the same time
//        - byte pin - REQUIRED - the Arduino Pin to be used as a digital output
//        - int s_l - OPTIONAL - first argument of Arduino map(s_l,s_h,m_l,m_h) function to scale the output
//        - int s_h - OPTIONAL - second argument of Arduino map(s_l,s_h,m_l,m_h) function to scale the output
//        - int m_l - OPTIONAL - third argument of Arduino map(s_l,s_h,m_l,m_h) function to scale the output
//        - int m_h - OPTIONAL - fourth argument of Arduino map(s_l,s_h,m_l,m_h) function to scale the output
 
 static st::PS_Illuminance sensor2(F("illuminance1"), 900, 0, PIN_ILLUMINANCE, 0, 1023, 100,0);

//        st::IS_Motion() constructor requires the following arguments
//        - String &name - REQUIRED - the name of the object - must match the Groovy ST_Anything DeviceType tile name
//        - byte pin - REQUIRED - the Arduino Pin to be used as a digital output
//        - bool iState - OPTIONAL - LOW or HIGH - determines which value indicates the interrupt is true
//        - bool internalPullup - OPTIONAL - true == INTERNAL_PULLUP
//        - long numReqCounts - OPTIONAL - number of counts before changing state of input (prevent false alarms)
//        - long inactiveTimeout - OPTIONAL - number of milliseconds motion must be inactive before sending update 

static st::IS_Motion sensor3(F("motion1"), PIN_MOTION, HIGH, false, 1,500);

  //*****************************************************************************
  //  Configure debug print output from each main class 
  //  -Note: Set these to "false" if using Hardware Serial on pins 0 & 1
  //         to prevent communication conflicts with the ST Shield communications
  //*****************************************************************************
  st::Everything::debug=true;
  st::Executor::debug=true;
  st::Device::debug=true;
  st::Sensor::debug=true;
  st::PollingSensor::debug=true;
  st::InterruptSensor::debug=true;

  //*****************************************************************************
  //Initialize the "Everything" Class
  //*****************************************************************************

  //Initialize the optional local callback routine (safe to comment out if not desired)
  st::Everything::callOnMsgSend = callback;
  
  //Create the SmartThings ESP8266WiFi Communications Object
    //STATIC IP Assignment - Recommended
    st::Everything::SmartThing = new st::SmartThingsESP8266WiFi(str_ssid, str_password, ip, gateway, subnet, dnsserver, serverPort, hubIp, hubPort, st::receiveSmartString);
 
  //Run the Everything class' init() routine which establishes WiFi communications with Hubitat Hub
  st::Everything::init();
  
  //*****************************************************************************
  //Add each sensor to the "Everything" Class
  //*****************************************************************************
  st::Everything::addSensor(&sensor1);
  st::Everything::addSensor(&sensor2);
  st::Everything::addSensor(&sensor3);

  //*****************************************************************************
  //Initialize each of the devices which were added to the Everything Class
  //*****************************************************************************
  st::Everything::initDevices();
  
}

//******************************************************************************************
//Arduino Loop() routine
//******************************************************************************************
void loop()
{
  //*****************************************************************************
  //Execute the Everything run method which takes care of "Everything"
  //*****************************************************************************
  st::Everything::run();

  //*****************************************************************************
  //  O T A
  //*****************************************************************************
  httpServer.handleClient();
  MDNS.update();
}
