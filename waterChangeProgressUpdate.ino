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
