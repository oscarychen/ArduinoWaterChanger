void setup() {
  //Initialization routine, this routine is ran only once when the microprocessor is powered up or reset

  Serial.begin(9600);
  while(!Serial);
  
  setTime(18,45,0,1,28,18); //set default time upon powering up (hr, min, sec, day, month, year)

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
  
  //external interrupt attached to high level switch, triggers ESD routine
  //attachInterrupt(digitalPinToInterrupt(HighLevelSw), ESD, FALLING);
  //attachPCINT(digitalPinToPCINT(HighLevelSw), ESD, FALLING); //only use this line if pin has no external interrupt, needs library <PinChangeInterrupt.h>

  rotaryEncoderInit();  //initialize rotary encoder
  menuInit(); //initialize menu system
  
  lcd.init(); // initialize the lcd 
  lcd.backlight();
  
}

