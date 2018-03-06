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
