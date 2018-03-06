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
