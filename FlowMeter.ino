
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
