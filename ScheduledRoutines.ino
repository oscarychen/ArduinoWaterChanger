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
