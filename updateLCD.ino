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
