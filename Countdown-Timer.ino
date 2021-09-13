// 0: 16738455
//1: 16724175
//2: 16718055
//3: 16743045
//4: 16716015
//5: 16726215
//6: 16734885
//7: 16728765
//8: 16730805
//9:16732845
//EQ: 16748655
//+: 16754775
//-: 16769055
//next: 16712445
//back: 16720605
//play pause: 16761405
//ch+: 16769565
//ch: 16736925
//ch-: 16753245
// 100+: 16750695
// 200+: 16756815
// holding: 4294967295

//eeprom addresses: 0-12 testing, 12-14 countdown, 14-22 remainding_time

#include <math.h>

#include <EEPROM.h>

//for remote
#include <IRremote.h>
const int RECV_PIN = 3;
IRrecv irrcv(RECV_PIN);
decode_results rsults;


//for sevseg led
#include "SevSeg.h"
SevSeg sevseg; //Initiate a seven segment controller object

// Include the Arduino Stepper.h library:
#include <Stepper.h>
// Define number of steps per rotation:
const int stepsPerRevolution = 2048;
// Wiring:
// Pin 8 to IN1 on the ULN2003 driver
// Pin 9 to IN2 on the ULN2003 driver
// Pin 10 to IN3 on the ULN2003 driver
// Pin 11 to IN4 on the ULN2003 driver
// Create stepper object called 'myStepper', note the pin order:
Stepper myStepper = Stepper(stepsPerRevolution, 50, 51, 52, 53);

//general
uint64_t timing;
uint64_t eq_time;
int countdown_index = 0;
int countdown = 0;
uint64_t benchmark_time = 0;
uint64_t elapsed_time = 0;
uint64_t initial_days;
uint64_t initial_hours;
uint64_t initial_time;
uint64_t timeLeftSinceOn;
uint64_t remainding_time;
uint64_t remainding_days;
uint64_t remainding_hours;
const int fullSliderSteps = 7800;
int initialTotalHours;
int stepsPerHour;
int prevTotalHours;
int remaindingTotalHours;
unsigned long lastRemoteCode;
bool runMotorBackwards = false;
bool runMotorForwards = false;
int displayBrightness = -80;
bool displayOn = true;
bool countdownFinished = false;
const uint64_t setting_time = 120000;
uint64_t offsetTime = 0;
bool isSetTime = false;
unsigned long prevTiming32;
int overflowCounter = 0;
int countdownAddress = 12;
int remaindingTimeAddress = 14;
uint64_t lastSlideTime;
int excessSteps;

void setup(){
  Serial.begin(9600);
  timing = ( uint64_t ) millis64() + setting_time + offsetTime;
  eq_time =  ( uint64_t ) millis64();
  lastSlideTime = ( uint64_t ) millis64();

  // Set the speed to 5 rpm:
  myStepper.setSpeed(5);
  turnOffStepper();

  //for IR remote
  irrcv.enableIRIn();

  //for sevseg led: https://github.com/DeanIsMe/SevSeg
  byte numDigits = 4;  
  byte digitPins[] = {2, 22, 4, 5};
  byte segmentPins[] = {6, 7, 8, 9, 10, 11, 12, 13};
  bool resistorsOnSegments = 0; 
  // set variable to 1 if you want to use 8 resistors on the segment pins.
  sevseg.begin(COMMON_ANODE, numDigits, digitPins, segmentPins, resistorsOnSegments);
  sevseg.setBrightness(displayBrightness);

  //set countdown to stored value
  EEPROM.get(countdownAddress, countdown);
  EEPROM.get(remaindingTimeAddress, remainding_time);
  //set initial variables and stepsPerHour
  initial_days = countdown / 100;
  initial_hours = countdown - initial_days * 100;
  initial_time = (initial_days * 24 * 3600 * 1000) + (initial_hours * 3600 * 1000);
  initial_time += 3600000; // account for rounding down at initial time
  initialTotalHours = initial_days * 24 + initial_hours;
  stepsPerHour = fullSliderSteps / initialTotalHours;
  excessSteps = fullSliderSteps - stepsPerHour * initialTotalHours;

  // set prevTotalHours and timeLeftSinceOn
  timeLeftSinceOn = remainding_time;
  benchmark_time = timing;
  remainding_days = remainding_time / 86400000;
  remainding_hours = (remainding_time - remainding_days * 86400000) / (3600000);
  remaindingTotalHours = remainding_days * 24 + remainding_hours;
  prevTotalHours = remaindingTotalHours;
}


void loop(){

  timing = ( uint64_t ) millis64() + setting_time + offsetTime;
  //sevseg led
  sevseg.refreshDisplay(); // Must run repeatedly
  if (displayOn == false) {
    sevseg.blank();
  }
  else if (countdownFinished) {
    sevseg.setNumber(0, 2);
  }
  else {
    sevseg.setNumber(countdown, 2);
  }

  
  //if within certain amount of time of EQ, allow input
  if (((timing - eq_time) < setting_time) && isSetTime)
  {
    if (irrcv.decode(&rsults) && (countdown_index < 4))
    {
        if (rsults.value != 4294967295)
        {
          lastRemoteCode = rsults.value;
          int number = intep_num(rsults.value);
          if (number >= 0 && number <= 9)
          {
            if (countdown_index == 0)
            {
              countdown += (number * 1000);
            }
            if (countdown_index == 1)
            {
              countdown += (number * 100);
            }
            if (countdown_index == 2)
            {
              countdown += (number * 10);
            }
            if (countdown_index == 3)
            {
              countdown += number;
              EEPROM.put(countdownAddress, countdown);
              isSetTime = false;
            }
            countdown_index++;
          }
        }
     irrcv.resume();
    }
    //calculate time left
    initial_days = countdown / 100;
    initial_hours = countdown - initial_days * 100;
    initial_time = (initial_days * 24 * 3600 * 1000) + (initial_hours * 3600 * 1000) - 1;
    initial_time += 3600000; // account for rounding down at initial time
    timeLeftSinceOn = initial_time;
    initialTotalHours = initial_days * 24 + initial_hours;
    stepsPerHour = fullSliderSteps / initialTotalHours;
    excessSteps = fullSliderSteps - stepsPerHour * initialTotalHours;
    prevTotalHours = initialTotalHours;
    if (countdown_index == 4) {
      EEPROM.put(remaindingTimeAddress, initial_time);
    }
   
  }

  // normal operation
  else
  {
    elapsed_time = timing - benchmark_time;
    remainding_time = timeLeftSinceOn - elapsed_time;
    remainding_days = remainding_time / 86400000;
    remainding_hours = (remainding_time - remainding_days * 86400000) / (3600000);
    countdown = remainding_days * 100 + remainding_hours;   
    remaindingTotalHours = remainding_days * 24 + remainding_hours;

    if ((remaindingTotalHours < prevTotalHours) && !countdownFinished) {
      EEPROM.put(remaindingTimeAddress, remainding_time);
      sevseg.blank();
      slide(-stepsPerHour);
      prevTotalHours = remaindingTotalHours;
      if (prevTotalHours == initialTotalHours / 2) {
        slide(-excessSteps);
      }
    }

    // stop display and motor if countdown has ended
    if (remaindingTotalHours == 0) {
      countdownFinished = true;
    }

    // if receive EQ from remote, activate by setting eq_time
    if (irrcv.decode(&rsults)){
      if (rsults.value != 4294967295) {
        lastRemoteCode = rsults.value;
        executeRemoteCode(rsults.value, false);
      }
      else {
        executeRemoteCode(lastRemoteCode, true);
      }
      irrcv.resume();
    }

    // if forward or backwards is pressed
    if (runMotorForwards == true) {
      slide(-10);
    }
    else if (runMotorBackwards == true) {
      slide(10);
    }

    //turn off stepper if not activated for 1s
    if(millis64() - lastSlideTime > 1000) {
    turnOffStepper();
    }
  }
}


void executeRemoteCode(unsigned long input, bool repeated) {
  if(input == 16748655){     // eq button
    offsetTime = 0;
    timing =  ( uint64_t ) millis64() + setting_time + offsetTime;
    eq_time = timing;
    isSetTime = true;
    countdown = 0;
    countdown_index = 0;
    benchmark_time = timing;
    countdownFinished = false;
  }
  else if (input == 16750695) {   // 100+ button
    sevseg.blank();
    offsetTime = 0;
    timing = ( uint64_t ) millis64() + setting_time + offsetTime;
    slide(fullSliderSteps);
    eq_time = timing;
    isSetTime = true;
    countdown = 0;
    countdown_index = 0;
    benchmark_time = timing;
    countdownFinished = false;
  }
  else if (input == 16756815) { // 200+ button
    sevseg.blank();
    slide(-fullSliderSteps);
  }
  else if (input == 16754775) { // + button
    if (!countdownFinished) {
      offsetTime += 3600000;
      timing = ( uint64_t ) millis64() + setting_time + offsetTime;
      elapsed_time = timing - benchmark_time;
      remainding_time = timeLeftSinceOn - elapsed_time;
      EEPROM.put(remaindingTimeAddress, remainding_time);
      prevTotalHours -= 1;
      sevseg.blank();
      slide(-stepsPerHour);
      if (prevTotalHours == initialTotalHours / 2) {
        slide(-excessSteps);
      }
    }
  }
  else if (input == 16769055){ // - button
    countdownFinished = false;
    offsetTime -= 3600000;
    timing = ( uint64_t ) millis64() + setting_time + offsetTime;
    elapsed_time = timing - benchmark_time;
    remainding_time = timeLeftSinceOn - elapsed_time;
    EEPROM.put(remaindingTimeAddress, remainding_time);
    prevTotalHours += 1;
    EEPROM.put(remaindingTimeAddress, remainding_time);
    sevseg.blank();
    slide(stepsPerHour);
    if (prevTotalHours == initialTotalHours / 2 + 1) {
      slide(excessSteps);
    }
    
  }
  else if (input == 16712445) {   // forward button
    displayOn = false;
    runMotorForwards = true;
  }
  else if (input == 16720605){ // back button
    displayOn = false;
    runMotorBackwards = true;
  }
  else if (input == 16761405) { //play-pause button
    runMotorForwards = false;
    runMotorBackwards = false;
    displayOn = true;
  }
  else if (input == 16769565) { //ch+: 16769565
    if (displayBrightness + 20 <= 120){
      displayBrightness += 20;
      sevseg.setBrightness(displayBrightness);
    }
  }
  else if (input == 16753245) { //ch-: 16753245
    if (displayBrightness - 20 >= -120){
      displayBrightness -= 20;
      sevseg.setBrightness(displayBrightness);
    }
  }
  else if (input == 16736925 && !repeated) {  //ch: 16736925
    if (displayOn == true) {
      displayOn = false;
    }
    else {
      displayOn = true;
    }
  }
}

int intep_num(unsigned long input)
{
  if (input == 16738455)
  {
    return 0;
  }
  else if (input == 16724175)
  {
    return 1;
  }
  else if (input == 16718055)
  {
    return 2;
  }
  else if (input == 16743045)
  {
    return 3;
  }
  else if (input == 16716015)
  {
    return 4;
  }
  else if (input == 16726215)
  {
    return 5;
  }
  else if (input == 16734885)
  {
    return 6;
  }
  else if (input == 16728765)
  {
    return 7;
  }
  else if (input == 16730805)
  {
    return 8;
  }
  else if (input == 16732845)
  {
    return 9;
  }
  else {
    return 10;
  }
}

void turnOffStepper() {
  digitalWrite(50,LOW);
  digitalWrite(51,LOW);
  digitalWrite(52,LOW);
  digitalWrite(53,LOW);   
}

uint64_t millis64() {
  uint64_t timing64;
  if (millis() < prevTiming32) {
    overflowCounter++;
  }
  prevTiming32 = millis();
  timing64 = millis() + overflowCounter * 4294967295;
  return timing64;
}

void print(uint64_t value)
{
    const int NUM_DIGITS    = log10(value) + 1;

    char sz[NUM_DIGITS + 1];
    
    sz[NUM_DIGITS] =  0;
    for ( size_t i = NUM_DIGITS; i--; value /= 10)
    {
        sz[i] = '0' + (value % 10);
    }
    Serial.println(sz);
}

void slide (int steps) {
  myStepper.step(steps);
  lastSlideTime = ( uint64_t ) millis64();
}
