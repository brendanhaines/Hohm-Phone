#include "avr/sleep.h"
#include "Adafruit_FONA.h"
#include "Keypad.h"

////////////////////////////////
////////// PARAMETERS //////////
////////////////////////////////

// Arduino will wake from sleep when WAKE_PIN is pulled low. Connect any waking buttons to WAKE_PIN
#define WAKE_PIN 2

#define BUT_ANS 12
#define BUT_END 12

// RX/TX are in reference to the Arduino, not SIM800
#define GSM_RX 3
#define GSM_TX A7
#define GSM_RST 12
#define GSM_RING A6
#define GSM_KEY 12

// TIMEOUT_SLEEP is the time to stay awake from last activity until sleep (milliseconds)
#define TIMEOUT_SLEEP 60000

// Comment the following line to use HW serial for Fona and disable debugging information
#define USB_DEBUG

// Keypad pinout
byte rowPins[4] = {9, 4, 5, 7};
byte colPins[3] = {8, 10, 6};

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

#ifdef USB_DEBUG
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
#ifdef USB_DEBUG
  Serial.println("Woke up");
#endif
}

void goToSleep() {
#ifdef USB_DEBUG
  Serial.println("Going to sleep");
#endif
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
#ifdef USB_DEBUG
        Serial.println("Hanging up");
#endif
        break;
      }
    }
    // numpad stuff
  }
}

void beginCall() {
#ifdef USB_DEBUG
  Serial.println("Starting Call");
#endif
  fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End dialtone
  for (int i = 0; i < phoneNumberLength; i++) {
    fona.playDTMF( phoneNumber[i] );  // Play DTMF tones
    if ( fona.callPhone( phoneNumber ) ) {
#ifdef USB_DEBUG
      Serial.println("Call Started");
#endif
      inCall();
    }
    for ( int j = 0; j < phoneNumberLength; j++) {
      phoneNumber[j] = 0;
    }
    phoneNumberLength = 0;
  }
#ifdef USB_DEBUG
  Serial.println("Call ended");
#endif
}

//////////////////////////////////
///// ARDUINO CORE FUNCTIONS /////
//////////////////////////////////

void setup() {
  pinMode( WAKE_PIN, INPUT_PULLUP );
  pinMode( BUT_ANS, INPUT_PULLUP );
  pinMode( BUT_END, INPUT_PULLUP );
  pinMode( GSM_RST, OUTPUT );
  pinMode( GSM_RING, INPUT_PULLUP );
  pinMode( GSM_KEY, OUTPUT );

  digitalWrite( GSM_KEY, LOW );
  digitalWrite( GSM_RST, HIGH );

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) while (1); //fona didn't start

#ifdef USB_DEBUG
  Serial.begin(9600);
  Serial.flush();
  Serial.println("Setup Complete");
#endif
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





