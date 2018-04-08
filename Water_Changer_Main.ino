//Version 6: water changer by Oscar Chen
//Ver 6 changes: merged with menu system
//V5.1 update changes: 
// separated the code into multiple files for readability
// added ability to distinguish between water use during a water change session and evaporation
// water change target now excludes evaporation volumes (true water change volume should not include the top-off volumes in the past 24-hr session)

#include <PinChangeInterrupt.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

////////PIN ASSIGNMENT /////////

#define FloatSensor 4  //float sensor pin #
#define HighLevelSw 2  //Backup high level water switch #, use an external interrupt pin
#define Pump 3         //drain pump pin #
#define InletValve 5   //inlet valve pin #
#define InletValve2 6  //inlet valve 2 (backup) pin #

#define PresSw 7       //inlet water low pressure switch pin #
#define flowmeterPin A0 //inlet water flow meter pin # (interrupt pin), A0 for Uno, A8 for Mega

#define rotaryPin_A 12  //digital rotary encoder pin A
#define rotaryPin_B 13  //digital rotary encoder pin B
#define rotaryButtonPin 8 //digital rotary click button pin
#define inputTimeOut 10000   //time out for human input session, milliseconds

///////// END OF PIN ASSIGNMENT /////////

LiquidCrystal_I2C lcd(0x27,20,4);  //LCD address set to 0x20, 16 X 2 display
unsigned long LCDlastRefresh;     //last time LCD display was refreshed
int LCDmsgType = 1;                 //keep track between alternating display of flow rate and volume

unsigned char opMode = 4; //mode of operation: 0 - disabled; 1 - continuous; 2 - daily volume target; 3 - top-off only; 4 - timer mode

byte PumpPower = 250;          //power level of pump

unsigned long ESDpumpDuration = 360000;    //how long to run outlet pump for when ESD is triggered, milliseconds
//this should be long enough to draw water level below float sensor's position

bool ESDallowAutoReset = true;  //enable ESD auto reset
unsigned long autoResetDelay = 86400000;  //minimum delay before allowing auto reset, milliseconds
unsigned char maxAutoResets = 5;      //how many times auto reset is allowed before a user reset is required
unsigned char ESDresets = 0;           //keeps track of number of ESD auto reset
bool errorFlag = false;         //flag to enable system functioning, change it to false when fault
bool activeESD = false;       //flag to indicate if ESD sequence is in progress
bool ESDcomplete = false;     //flag to indicate if ESD sequence completed running
unsigned long ESDtime = millis();     //time of when ESD was triggered

unsigned char FloatSensorState = HIGH; //registered float sensor state
unsigned char lastFloatSensorState = HIGH;  //live update last float sensor state
unsigned long lastFloatSensorChangeTime;

unsigned char HighLevelState = LOW;
unsigned char lastHighLevelState = HighLevelState;
unsigned long lastHighLevelChangeTime;

unsigned char inletPres;
unsigned char lastInletPres = HIGH;
bool inletPresEnable = true;  //enable/disable the use of inlet pressure switch
unsigned long PressureChangeDelay = 300000; ////delay between toggling low pressure switch back to high, milliseconds
unsigned long lastPresChangeTime = millis() - PressureChangeDelay;

////////////Flow meter declarations /////////////////

float calibrationFactor = 30; // pulses per second per litre/minute of flow.
volatile byte pulseCount;
float flowRate;
float totalVolume;  //current dayily water consumption;
unsigned long oldTime;
unsigned long weeklyTotal;  //weekly water usage
float YDayVolume;     //yesterday's total water usage

float waterChangeVol; //volume of water in water change session
float dailyEvapVol; //evaporated volume per day

/////////////End of Flow meter declarations /////////////


//opMode 2 & 4 specific settings
int targetVolume = 15;      //desired daily water consumption volume in litres
int startTimeHour = 18;     //set what time during the day to start water change operation, hour
int startTimeMinute = 0;   //set what time during the day to start water change operation, minute
int drainMinutes = 7;       //duration of drain pump in Mode 4
bool waterChangeInProgress = false; //flag to indicate if water change is in progress
bool waterChangeLevelLow = false; //flag to indicate if water level is drawn down during water change (used to find out if water change is completed later)
bool timerModeDrainOn = false;  //flag to indicate if draining is active (opMode 4 Timer Mode)
bool maintainLevelDuringWC = true; //maintains water level during water change (small batch vs large batch water change)

int drainTimeHour;
int drainTimeMinute;

/////Digital Rotary Encoder Delcarations/////////
unsigned int maxMenuItems;     //number of menu items
unsigned char encoder_A = 0;  
unsigned char encoder_B = 0;
unsigned char encoder_A_prev = 0;
unsigned char encoder_C = 1;  //encoder button
unsigned char encoder_C_prev = 1;
unsigned long currentTime;
unsigned long loopTime;

/////Menu related declarations/////////

unsigned int setHour = 0;   //used for setting time: hour
unsigned int setMinute = 0; //used for setting time: minute
unsigned int menuPos = 0; //current menu position
unsigned int lastMenuPos = 0; //previous menu position
unsigned int parentMenuPos = 0; //parent menu position
bool humanInputActive = false;  //flag to indicate if input session is active
unsigned subMenuActive = 0;   //flag to indicate a sub selection menu session is active: 0 - main menu; 1 - number selection 1- 255; 2 - binary selection yes/no; 3 - time setting;
unsigned int subMenuPos = 0;  //sub menu rotary position
unsigned int subMenuClick = 0; //sub menu click counter
unsigned long lastInputTime = millis(); //keep track of time of last human input

typedef struct menu_type
 {
     
     menu_type()
     : code(0), text("")
     {
       //do nothing
     }
     
     unsigned int code; //code that indicate address (position) in the menu tree
     String text; //text to be displayed for the menu selection
     
 }  menu_type;

menu_type menu[100] = {}; //initilizing menu array, use a number >= than the number of menu options

/////END OF Menu related declarations////////////

//Alarm (scheduler) delcarations
AlarmId dailyAlarm;   //daily execution
AlarmId weeklyAlarm;  //weekly execution
AlarmId dailyAlarm2;   //mode 4 daily required alarm

///////////////////// END OF ALL DECLARATIONS //////////////////////


void loop() {
//main program
 
  Alarm.delay(0);
  flowmeterUpdate();
  updateSensors();
  time_t t = now();

  if (errorFlag == false) { //if ESD, bypass all operation modes
  
    switch (opMode) {
      case 0: //disabled mode, do nothing
        //Serial.println("Mode 0");
        PumpOff();
        InletClose();
        break;
      
      case 1: // continuous mode
        //Serial.println("Mode 1");
           
          if (checkTankLevel() == 255) { //if (emergency) high level switch is treiggered
              ESD();
          
          } else if (checkTankLevel() >= 250) { //if water level high
              InletClose();
              PumpOn();
              
          } else if(checkTankLevel() < 250) { //if water level low
              
              PumpOff();
              
              if (checkInletPressure() == true) { //fill only if pressure check passes
                InletOpen();
              } else {
                InletClose();
              }
          } else {
            InletClose();
            PumpOff();
          }
          
        break;
        
      case 2: //daily volume target mode
        
        if (checkTankLevel() == 255) { //if emergency high level is triggered
          ESD();
          
        } else if (checkTankLevel() >= 250) { //if water level high
          InletClose();
          
          if (checkWaterConsumption() == true && checkInletPressure() == true) { //if daily volume target has not been met yet
            PumpOn();
            
          }
          
        } else if (checkTankLevel() < 250 ) { //if water level low
            
           PumpOff();
           
           if (checkInletPressure() == true) { //only fill if pressure is good
            InletOpen();
           } else {
            InletClose();
           }
          
        } else {
          InletClose();
          PumpOff();
        }
          
        break;
        
      case 3: //top-off only mode;

        if (checkTankLevel() == 255) {  //if high level emergency
          ESD();
          
        } else if(checkTankLevel() < 250) { //if not topped off
            PumpOff();
            
            if (checkInletPressure() == true) {
              InletOpen();
            } else {
              InletClose();
            }
            
        } else {
            PumpOff();
            InletClose();
        }
        
        break;
        
      case 4: //daily timer mode;

        if (checkTankLevel() == 255) { //if emergency high level is triggered
          ESD();
          
        } else if (timerModeDrainOn == true) { //if it is scheduled draining time
          InletClose();
          PumpOn();
          
        } else if (timerModeDrainOn == false) {
           PumpOff();

           if (checkTankLevel() < 250 ) { //if water level low
              if (checkInletPressure() == true) { //only fill if pressure is good
                InletOpen();
              } else {
                InletClose();
              }
          
            } else {  //if water level high
              InletClose();
              PumpOff();
            }
        }
        
        break;
        
    }

  } else {  //if ESD has been triggered
    
    ESD(); //update ESD sequence.
    
  }

  rotaryEncoderUpdate();
  lcdBackLight();
  updateLCD();
}

void forceDrain() {
  //to be activated manually by user
  PumpOn();
  Alarm.timerOnce(5, PumpOff);
}

void forceFill() {
  //to be activated manually by user
  InletOpen();
  Alarm.timerOnce(5, InletClose);
}

void InletClose() {
  //Serial.println("inlet valve close");
  digitalWrite(InletValve, LOW);
  digitalWrite(InletValve2, LOW);
}

void InletOpen() {
  if (errorFlag == false) {
    //Serial.println("inlet valve open");
    digitalWrite(InletValve, HIGH);
    digitalWrite(InletValve2, HIGH);
  }
}

void PumpOn() {
    //Serial.println("pump on");
    analogWrite(Pump, PumpPower);
}

void PumpOff() {
  //Serial.println("pump off");
  analogWrite(Pump, 0);
}

void masterReset() {
// resets whole system, enable normal functioning
  errorFlag = false;
  ESDcomplete = false;
  activeESD = false;
  ESDresets = 0;
  LCDmsgType = 1;
}


