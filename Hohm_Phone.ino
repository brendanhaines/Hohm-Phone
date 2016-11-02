#include "avr/sleep.h"
#include "Adafruit_FONA.h"
#include "Keypad.h"

////////////////////////////////
////////// PARAMETERS //////////
////////////////////////////////

// Arduino will wake from sleep when WAKE_PIN is pulled low
#define WAKE_PIN 2
#define BUT_ANS 12
#define BUT_END 12

// RX/TX are in reference to the Arduino, not SIM800
#define GSM_RX 3
#define GSM_TX A7
#define GSM_RST 12
#define GSM_RING A6

// TIMEOUT_SLEEP is the time to stay awake from last activity until sleep (milliseconds)
#define TIMEOUT_SLEEP 60000

// Comment the following line to use HW serial
#define usb_testing_config

// Keypad pinout
byte rowPins[ROWS] = {9, 4, 5, 7};
byte colPins[COLS] = {8, 10, 6};

////////////////////////////////////
////////// END PARAMETERS //////////
////////////////////////////////////

char phoneNumber[15];
uint8_t phoneNumberLength = 0;

char keys[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'#', '0', '*'}
};

unsigned long lastActiveTime;

#ifdef usb_testing_config
#include "SoftwareSerial.h"
SoftwareSerial fonaSS = SoftwareSerial(GSM_RX, GSM_TX);
SoftwareSerial *fonaSerial = &fonaSS;
#else
HardwareSerial *fonaSerial = &Serial;
#endif

Adafruit_FONA fona = Adafruit_FONA(GSM_RST);
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, 4, 3 );


///////////////////////////////
////////// FUNCTIONS //////////
///////////////////////////////

void wakeFromSleep() {
  sleep_disable();
  detachInterrupt(WAKE_PIN);
}

void goToSleep() {
  sleep_enable();
  attachInterrupt(WAKE_PIN, wakeFromSleep, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_bod_disable();
  sei();
  sleep_cpu();
}

void inCall() {
  while (1) {
    if ( !digitalRead(BUT_END) ) { // End button pressed
      if ( fona.hangUp() ) {
        break;
      }
    }
    // numpad stuff
  }
}

void beginCall() {
  fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End dialtone
  for (int i = 0; i < phoneNumberLength; i++) {
    fona.playDTMF( phoneNumber[i] );  // Play DTMF tones
    if ( fona.callPhone( phoneNumber ) ) {
      inCall();
    }
    for ( int j = 0; j < phoneNumberLength; j++) {
      phoneNumber[j] = 0;
    }
    phoneNumberLength = 0;
  }
}

//////////////////////////////////
///// ARDUINO CORE FUNCTIONS /////
//////////////////////////////////

void setup() {
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) while (1); //fona didn't start
}

void loop() {
  if ( !digitalRead(GSM_RING) ) {
    while ( digitalRead(BUT_ANS) & !digitalRead(GSM_RING) ) delay(10); // Wait for answer button or end ring/call
    if ( !digitalRead(BUT_ANS) ) {
      if ( fona.pickUp() ) {
        inCall();
      }
    }
  }

  fona.sendCheckReply( F("AT+STTONE=1,20,1000"), F("OK") ); // Start tone

  // Read from keypad
  char key = keypad.getKey();
  if ( key != NO_KEY ) {
    phoneNumber[ phoneNumberLength ] = key;
    phoneNumberLength++;
    lastActiveTime = millis();
  }

  // Check for complete phone number (including +1 country code)
  if ( ( phoneNumberLength = 10 & phoneNumber[0] != '1' ) || phoneNumberLength > 10 ) {
    beginCall();
  }

  // Autoshutdown if inactive for extended period
  // Typecast to long avoids "rollover" issues
  if ( (long)(millis() - lastActiveTime) > TIMEOUT_SLEEP ) {
    goToSleep();
  }
}





