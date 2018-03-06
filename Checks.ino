
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
  
  if (targetVolume > waterChangeVol) {
    returnValue = true;
    
  } else {
    returnValue = false;
    waterChangeInProgress = false;  //water change is completed
  }

  return returnValue;
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
