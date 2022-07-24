#include <EEPROM.h>
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"

#define I2C_ADDRESS 0x3C
SSD1306AsciiWire oled;

// EEPROM adresses
const byte EE_pumpInterval = 0;
const byte EE_pumpDuration = 1;

// encoder
const byte SW = 3;
const byte CLK = 4;
const byte DT = 5;
float tmp = 1;
long oldPosition = -999;
bool isInterval = false;

// relay
const byte triggerPin = 6;

// save button
const byte buttonPin = 7;

// interval
unsigned int pumpInterval = 0; // a counter for the dial
unsigned int lastPumpInterval = 0;     // change management
static boolean rotating_int = false;        // debounce management
boolean A_set_int = true;
boolean B_set_int = true;

// duration
unsigned int pumpDuration = 0; // a counter for the dial
unsigned int lastPumpDuration = 0;     // change management
static boolean rotating_dur = false;        // debounce management
boolean A_set_dur = true;
boolean B_set_dur = true;

// +SW Encoder button
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output 
// -SW Encoder button

// +save button
int sButtonState;             // the current reading from the input pin
int sLastButtonState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long sLastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long sDebounceDelay = 50;    // the debounce time; increase if the output 
// -save button

// oled timer
unsigned long oledPrevious = 0;
const long oledInterval = 5000;
bool updateOled = true;

// pump state
bool pumpRunning = false;

// pump timer
unsigned long pumpPreviousTimer = 0;
const long pumpIntervalTimer = 1000;
unsigned long remainingTime = 0;

void updateOledDisplay() {
  oled.clear();
  if(isInterval) {
    oled.setInvertMode(1);
  }
  else {
    oled.setInvertMode(0);
  }
  oled.print("interval: ");
  oled.println(pumpInterval);
  if(!isInterval) {
    oled.setInvertMode(1);
  }
  else {
    oled.setInvertMode(0);
  }
  oled.print("duration: ");
  oled.println(pumpDuration);
  oled.setInvertMode(0);
  if(!pumpRunning){
    oled.print("start in ");
    oled.print(remainingTime);
  }
  else {
    oled.print("stop in ");
    oled.print(remainingTime);
  }
  oled.println(" min");
  //updateOled = false;
}

void setup()
{
  Serial.begin(9600);
  pinMode(triggerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(SW, INPUT_PULLUP);
  pinMode(CLK, INPUT_PULLUP);
  pinMode(DT, INPUT_PULLUP);

  // HIGH is OFF, LOW is ON
  digitalWrite(triggerPin, HIGH);
  pumpInterval = EEPROM.read(EE_pumpInterval) * 5;
  lastPumpInterval = pumpInterval;
  pumpDuration = EEPROM.read(EE_pumpDuration);
  lastPumpDuration = pumpDuration;

  Wire.begin();
  Wire.setClock(400000L);
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(X11fixed7x14);
}

void loop() {
  // +SW pressed
  int reading = digitalRead(SW);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        isInterval = !isInterval;
        updateOledDisplay();
      }
    }
  }
  lastButtonState = reading;
  // -SW pressed
  
  // +save button pressed
  int sReading = digitalRead(buttonPin);
  if (sReading != sLastButtonState) {
    sLastDebounceTime = millis();
  }
  if ((millis() - sLastDebounceTime) > sDebounceDelay) {
    if (sReading != sButtonState) {
      sButtonState = sReading;

      if (sButtonState == HIGH) {
        // save to EEPROM here
        EEPROM.update(EE_pumpInterval, pumpInterval/5); // divide by 5 for saving larger values in 1 byte
        EEPROM.update(EE_pumpDuration, pumpDuration);
      }
    }
  }
  sLastButtonState = sReading;
  // -save button pressed

  if (isInterval == true) {
    // +pump interval
    rotating_int = true; // reset the debouncer
    if (rotating_int) delay (1);  // wait a little until the bouncing is done
    if (lastPumpInterval != pumpInterval) {
      lastPumpInterval = pumpInterval;
      rotating_int = false;  // no more debouncing until loop() hits again
      updateOledDisplay();
    }
    
    if( digitalRead(CLK) != A_set_int ) { // debounce once more
      A_set_int = !A_set_int;
      // adjust counter + if A leads B
      if ( A_set_int && !B_set_int )
      pumpInterval += 5;
      if (rotating_int) delay (1);  // wait a little until the bouncing is done
    }
    
    if( digitalRead(DT) != B_set_int ) {
      B_set_int = !B_set_int;
      // adjust counter – 1 if B leads A
      if( B_set_int && !A_set_int )
      pumpInterval -= 5;
      if (pumpInterval < 5) {
        pumpInterval = 5;
      }
    }
    // -pump interval
  }
  else {
    // +pump duration
    rotating_dur = true; // reset the debouncer
    if (rotating_dur) delay (1);  // wait a little until the bouncing is done
    if (lastPumpDuration != pumpDuration) {
      lastPumpDuration = pumpDuration;
      rotating_dur = false;  // no more debouncing until loop() hits again
      updateOledDisplay();
    }
    
    if( digitalRead(CLK) != A_set_dur ) { // debounce once more
      A_set_dur = !A_set_dur;
      // adjust counter + if A leads B
      if ( A_set_dur && !B_set_dur )
      pumpDuration += 1;
      if (rotating_dur) delay (1);  // wait a little until the bouncing is done
    }
    
    if( digitalRead(DT) != B_set_dur ) {
      B_set_dur = !B_set_dur;
      // adjust counter – 1 if B leads A
      if( B_set_dur && !A_set_dur )
      pumpDuration -= 1;
      if (pumpDuration < 1) {
        pumpDuration = 1;
      }
    }
    // -pump duration
  }

  // lazy update of oled display
  unsigned long oledCurrent = millis();
  if(oledCurrent - oledPrevious >= oledInterval) {
    oledPrevious = oledCurrent;
    //if(!updateOled) {
      updateOledDisplay();
    //}
  }

  unsigned long pumpCurrentTimer = millis();
  // multiply by 60 for release
  if(pumpCurrentTimer - pumpPreviousTimer >= getInterval() && !pumpRunning) {
    pumpPreviousTimer = pumpCurrentTimer;
    pumpRunning = true;
    digitalWrite(triggerPin, LOW);
    updateOledDisplay();
    Serial.println("pumpRunning: true");
  }
  // multiply by 60 for release
  if(pumpCurrentTimer - pumpPreviousTimer >= getDuration() && pumpRunning) {
    pumpPreviousTimer = pumpCurrentTimer;
    pumpRunning = false;
    digitalWrite(triggerPin, HIGH);
    updateOledDisplay();
    Serial.println("pumpRunning: false");
  }

  if(pumpRunning) { // display remaining runtime
    remainingTime = (pumpPreviousTimer + getDuration() - pumpCurrentTimer) /60000L;
  }
  else { // display remaining time until pump start
    remainingTime = (pumpPreviousTimer + getInterval() - pumpCurrentTimer) /60000L;
  }
  
}

unsigned long getDuration() {
  return pumpDuration * 60000L;
}

unsigned long getInterval() {
  return pumpInterval * 60000L;
}
