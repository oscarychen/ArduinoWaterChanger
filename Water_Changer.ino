//Version 5: water changer by Oscar Chen
//V5.1 update: timer mode adapted to as the sub-mode of daily target mode, where water level is drawn down during water change.

#include <PinChangeInterrupt.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define FloatSensor 4  //float sensor pin #
#define HighLevelSw 2  //Backup high level water switch #, use an external interrupt pin
#define Pump 3         //drain pump pin #
#define InletValve 5   //inlet valve pin #
#define InletValve2 6  //inlet valve 2 (backup) pin #

#define PresSw 7       //inlet water low pressure switch pin #
#define flowmeterPin A0 //inlet water flow meter pin # (interrupt pin), A0 for Uno, A8 for Mega

#define rotaryPin_A 12
#define rotaryPin_B 13
#define rotaryButtonPin 8

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


////////////Flow meter declarations
float calibrationFactor = 30; // pulses per second per litre/minute of flow.
volatile byte pulseCount;
float flowRate;
float totalVolume;  //current dayily water consumption;
unsigned long oldTime;
unsigned long weeklyTotal;  //weekly water usage
float YDayVolume;     //yesterday's total water usage

float waterChangeVol; //volume of water in water change session
float dailyEvapVol; //evaporated volume per day
/////////////End of Flow meter declarations

//opMode 2 & 4 settings
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


AlarmId dailyAlarm;   //daily execution
AlarmId weeklyAlarm;  //weekly execution
AlarmId dailyAlarm2;   //mode 4 daily required alarm

void setup() {

  Serial.begin(9600);
  while(!Serial);
  
  setTime(18,45,0,1,28,18); //set default time (hr, min, sec, day, month, year)

//set daily alarm to start draining water
  dailyAlarm = Alarm.alarmRepeat(startTimeHour, startTimeMinute, 0, DailyReset);  //sets time to start water change (opMode 2 Daily Volume Target mode)

//calculate what time to stop draining and start re-filling (opMode 4)
  if (startTimeMinute + drainMinutes >= 60) {
    drainTimeHour = startTimeHour + (startTimeMinute + drainMinutes) / 60;
    drainTimeMinute = (drainMinutes + startTimeMinute) % 60;
  } else {
    drainTimeHour = startTimeHour;
    drainTimeMinute = drainMinutes + startTimeMinute;
  }

 //set daily alarm to start re-filling
  dailyAlarm = Alarm.alarmRepeat(drainTimeHour, drainTimeMinute, 0, DailyReset2); //set time to start filling (opMode 4 Timer Mode only)

 //weekly action
  weeklyAlarm = Alarm.alarmRepeat(dowSaturday, 0, 00, 0, WeeklyReset);

  pinMode(FloatSensor, INPUT_PULLUP);
  pinMode(HighLevelSw, INPUT_PULLUP);
  pinMode(PresSw, INPUT_PULLUP);
  
  pinMode(InletValve, OUTPUT);
  pinMode(InletValve2, OUTPUT);
  pinMode(PumpPower, OUTPUT); 
  
  pinMode(rotaryPin_A, INPUT_PULLUP);
  pinMode(rotaryPin_B, INPUT_PULLUP);
  pinMode(rotaryButtonPin, INPUT_PULLUP);

  //flow meter set up
  flowmeterInit();
  
  //external interrupt attached to high level switch, triggers ESD routine.
  //attachInterrupt(digitalPinToInterrupt(HighLevelSw), ESD, FALLING);
  //attachPCINT(digitalPinToPCINT(HighLevelSw), ESD, FALLING); //use this line if pin has no external interrupt

  
  ////////LCD set up
  lcd.init(); // initialize the lcd 
  lcd.backlight();
  /////// end of LCD set up
  
}


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
  
  updateLCD();
}

void waterChangeProgressUpdate() {
  //update the water change completion flag
  //update global variable waterChangeInProgress
  //update global variable waterChangeLevelLow

int currentLevel = checkTankLevel();

  if (waterChangeInProgress == true) {
    
    if (waterChangeLevelLow == false && currentLevel < 250) {
      //water level dropped, update waterChangeLevelLow to true
      waterChangeLevelLow = true;
      
    } else if (waterChangeLevelLow == true && currentLevel >= 250) {
      
      //water level went from low to high, water level change completed, update flags
      waterChangeLevelLow = false;
      waterChangeInProgress = false;

    }
  }
  
}

void updateLCD() {

  time_t t = now();
  
    if (errorFlag == true) {  //if error, skip normal display sequence
      LCDmsgType = 0;
      
            lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("ESD triggered.");
            lcd.setCursor(0,1);
            lcd.print("Auto resets: ");
            lcd.print(ESDresets);

    } else if (millis() - LCDlastRefresh > 3000) {
      //check what type of message was last displayed, and alternate
      switch (LCDmsgType) {
        case 1: //displays todays volume of water added
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Volume Today(L)");
          lcd.setCursor(0,1);
          lcd.print(totalVolume);
          LCDmsgType ++;
          break;
          
        case 2: //displays instantaneous flow rate
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Flowrate(L/min)");
          lcd.setCursor(0,1);
          lcd.print(flowRate);
          LCDmsgType ++;
          break;
          
        case 3: //displays yesterdays volume
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print("Volume Yday(L)");
          lcd.setCursor(0,1);
          lcd.print(YDayVolume);
          LCDmsgType ++;
          break;
          
        case 4: //displays current time and operating mode
          lcd.clear();
          lcd.setCursor(0,0);
          //print hour
          lcd.print(hour(t));
          lcd.print(":");
          //format and print minute
          if (minute(t)<10) {
            lcd.print("0");
          }
          lcd.print(minute(t));
          //print seconds
          lcd.print(":");
          lcd.print(second(t));
          lcd.setCursor(0,1);
            switch (opMode) {
              case 0:
                lcd.print("Disabled");
                break;
              case 1:
                lcd.print("Continuous Mode");
                break;
              case 2:
                lcd.print("Daily Volume");
                break;
              case 3:
                lcd.print("Top-Off Only");
                break;
              case 4:
                lcd.print("Daily Timer");
                break;
            }
            LCDmsgType ++;
            break;

          case 5:
            lcd.clear();
          if (checkInletPressure() == false) {
            lcd.setCursor(0,0);
            lcd.print("Inlet Press. Low");
            lcd.setCursor(0,1);
            lcd.print("Waiting to fill");
          }
          
            LCDmsgType = 1 ;
            
            break;
          
        default:
           //do not update LCD
          break;
      }
      
      LCDlastRefresh = millis();
    }
    
}

void DailyReset() {
  //Target Volume mode: called once a day to reset daily target to zero
  //Daily Timer mode: called once a day to start draining
  
  flowmeterUpdate();
  weeklyTotal += totalVolume;
  YDayVolume = totalVolume;
  totalVolume = 0;    //reset the flowmeter total volume to zero
  waterChangeVol = 0; //reset the daily water change volume to zero
  dailyEvapVol = 0; //reset daily water evaporation volume to zero

  waterChangeInProgress = true;
  timerModeDrainOn = true;  //for opMode 4 only: start draining

  //Serial.println("Daily reset.");
}

void DailyReset2() {
  //Daily Timer mode: called once a day to start filling
  timerModeDrainOn = false; //for opMode 4 only: stop draining
}

void WeeklyReset() {
  //called once a week 
  
  weeklyTotal = 0;
}

void flowmeterInit() {
  pinMode(flowmeterPin, INPUT);
  digitalWrite(flowmeterPin, HIGH);

  pulseCount = 0;
  flowRate = 0.0;
  totalVolume  = 0;
  oldTime = 0;

  attachPCINT(digitalPinToPCINT(flowmeterPin), pulseCounter, FALLING);
}

void flowmeterUpdate() {
  float volume = 0; //volume since last check
  
  if((millis() - oldTime) > 1000)    // Only process counters once per second
  { 
    // Disable the interrupt while calculating flow rate
      detachPinChangeInterrupt(digitalPinToPinChangeInterrupt(flowmeterPin));
    
    //flow rate at in liters/minute
      flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;

      //Serial.print("flowRate: ");
      //Serial.println(flowRate);
      oldTime = millis();
    
    //flow volume during this update in litres
      volume = flowRate / 60;
      totalVolume += volume;

    //if water change is in progress, also add to water change volume
      if (waterChangeInProgress == true) {
        waterChangeVol += volume;
      }

      dailyEvapVol = totalVolume - waterChangeVol;

    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
    attachPCINT(digitalPinToPCINT(flowmeterPin), pulseCounter, FALLING);
  }
}

void pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
}

int checkTankLevel() {
  updateSensors();
  int WaterLevel;

  if (HighLevelState == HIGH) {
    WaterLevel = 255;
  } else if (FloatSensorState == HIGH) {
    WaterLevel = 250;
  } else {
    WaterLevel = 0;
  }

 
  return WaterLevel;
  
}

bool checkWaterConsumption() {
//checks current water consumption against daily target, 
//if below target return true, if over target return false
  bool returnValue;
  
  if (targetVolume > totalVolume) {
    returnValue = true;
    
  } else {
    returnValue = false;
    waterChangeInProgress = false;  //water change is completed
  }

  return returnValue;
}


void updateSensors() {
  
  unsigned char currentFloatSensorState = digitalRead(FloatSensor);
  unsigned char currentHighLevelState = !digitalRead(HighLevelSw);
  
  inletPres = digitalRead(PresSw);

//checks float sensor state, only register a change in the sensor state being low if it has been so for at least 3 second
  if (currentFloatSensorState == HIGH) {  //if float sensor is high, register it immediately
    FloatSensorState = HIGH;
    
  } else if (currentFloatSensorState == LOW && lastFloatSensorState == HIGH) { //float sensor reading changed to low, record time of change
    lastFloatSensorChangeTime = millis();
   
  } else if (currentFloatSensorState == LOW && lastFloatSensorState == LOW){  //float sensor reading low consecutively
    if (millis() - lastFloatSensorChangeTime > 3000) { //float reading stays low for at least 1 second
      FloatSensorState = LOW;  //updates the global variable for float sensor
      
    } 
  }
  lastFloatSensorState = currentFloatSensorState;
  

//checks high level sensor, only register a change if it has been so for at least 1 second
  if (lastHighLevelState != currentHighLevelState) {  //high level sensor reading changed
    lastHighLevelChangeTime = millis();  
      
  } else {  //high level sensor reading remains the same
    if (millis() - lastHighLevelChangeTime > 1000) {
      HighLevelState = currentHighLevelState; //updates the global variable for high level sensor
    }
  }
lastHighLevelState = currentHighLevelState;

 
  if (HighLevelState == HIGH) { //if high level switch register high level twice in a row
    errorFlag = true;
  }

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

bool checkInletPressure() {
//returns true if inlet pressure is good, false if pressure is low
  bool returnValue = false;

 // Serial.print("lastInletPres: ");
  //Serial.println(lastInletPres);
  //Serial.print("inletPres: ");
  //Serial.println(inletPres);
  
  if (inletPresEnable == false) { //if pressure switch is diabled
    returnValue = true;
    
  } else if (lastInletPres == HIGH && inletPres == LOW) { //if pressure falls
    
      lastPresChangeTime = millis();
      lastInletPres = inletPres;
      returnValue = false;
      //Serial.print("inlet pressure falls, return 0. Time lapsed: ");
      //Serial.println(millis() - lastPresChangeTime);
      
  } else if (lastInletPres == LOW && inletPres == HIGH ) { //if pressure rises
    
      lastPresChangeTime = millis();
      lastInletPres = inletPres;

      //check if enough time has elapsed since the last time pressure fell
      if ((millis() - lastPresChangeTime) > PressureChangeDelay) {
        returnValue = true; 
        //Serial.print("Inlet pressure rise, return 1. Time lapsed: ");
        //Serial.println(millis() - lastPresChangeTime);
        
      } else {
        returnValue = false;
        //Serial.print("Inlet pressure rise, but not enough time lapsed, return 0. Time lapsed: ");
        //Serial.println(millis() - lastPresChangeTime);
      }
  
  } else if (lastInletPres == HIGH && inletPres == HIGH ) { //pressure remains high

    if ( (millis() - lastPresChangeTime) > PressureChangeDelay) {
      returnValue = true;
      //Serial.print("Inlet pressure remaining high, return 1. Time lapsed: ");
      //Serial.println(millis() - lastPresChangeTime);
      
    } else {
    returnValue = false;
    //Serial.print("Not enough time passed, return 0. Time lapsed: ");
    //Serial.println(millis() - lastPresChangeTime);
    }
  }

  //Serial.print("checkInletPressure: ");
  //Serial.println(returnValue);
  return returnValue;

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

void ESD() {  //Emergency Shut Down, run outlet pump for a set number of minutes.

  //ESDcomplete is used to flag the initial drain sequence being in progress
  //activeESD is used to flag the entire ESD sequence including the delay period of the auto reset being in progress
  
  InletClose();
  
    if (ESDcomplete == false) { //if ESD drain sequence has not completed
    
      if (activeESD == false) {  //very first ESD check/update, switch flag
        
        ESDtime = millis();     //record time of initial ESD trigger

        activeESD = true;     //flag ESD auto reset delay period is active
        
      } else { //repeated ESD checks
  
        if (millis() - ESDtime > ESDpumpDuration) {  //enough time has elapsed since inital ESD trigger
          
          PumpOff();
          ESDcomplete = true; //mark ESD draw down sequence as completed
          ESDtime = millis(); //updateESDtime to the time initial ESD drain sequence is finished
          
        } else {  //not enough time has lapsed since initial ESD trigger, keep outlet pump running
          PumpOn(); 
        }
      
      }
    } else {  //ESD drain sequence already completed
      PumpOff();
      autoResetESD();
    }
}

void autoResetESD() {
  //check if ESD error can be cleared and reset system to normal operation mode
  //check if minimal delay has lapsed
  //check if no sensor indicating high level
  
  if (ESDallowAutoReset == true && ESDresets < maxAutoResets) {
    if (millis() - ESDtime > autoResetDelay) { //enough time has lapsed since ESD sequence finished
      if (checkTankLevel() < 255) { //if tank level is not high high
        ESDcomplete = false;
        activeESD = false;
        errorFlag = false;
      }
    }
  } else {  //auto reset not enabled or max number of auto-resets exceeded
    opMode = 0;   //disables system
    ESDcomplete = true;
    activeESD = true;
  }
  
}


void masterReset() {
// resets whole system, enable normal functioning
  errorFlag = false;
  ESDcomplete = false;
  activeESD = false;
  ESDresets = 0;
  LCDmsgType = 1;
}


