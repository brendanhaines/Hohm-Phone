#include "avr/sleep.h"
#include "Adafruit_FONA.h"
#include "Keypad.h"

////////////////////////////////
////////// PARAMETERS //////////
////////////////////////////////

// Arduino will wake from sleep when WAKE_PIN is pulled low. Connect any waking buttons to WAKE_PIN
#define WAKE_PIN 2

#define BUT_ANS A3
#define BUT_END A4

// RX/TX are in reference to the Arduino, not SIM800
#define GSM_RX 3
#define GSM_TX 11
#define GSM_RST 12
#define GSM_RING A5
#define GSM_KEY A6

// TIMEOUT_SLEEP is the time to stay awake from last activity until sleep (milliseconds)
#define TIMEOUT_SLEEP 6000

// Comment the following line to use HW serial for Fona and disable debugging information
#define USB_DEBUG

// Comment the following line to disable sleeping the Arduino
//#define SLEEP

// Keypad pinout
byte rowPins[4] = {9, 4, 5, 7};
byte colPins[3] = {8, 10, 6};

////////////////////////////////////
////////// END PARAMETERS //////////
////////////////////////////////////

char phoneNumber[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int phoneNumberLength = 0;

char keys[4][3] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

unsigned long lastActiveTime;

bool startDialtone = false;

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
  startDialtone = true;
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
    if ( !digitalRead(BUT_END) || fona.getCallStatus() < 3 ) { // End button pressed
      fona.hangUp();
#ifdef USB_DEBUG
      Serial.println("Hanging up");
#endif
      break;
    }

    // numpad stuff
  }
  for ( int j = 0; j < phoneNumberLength; j++) {
    phoneNumber[j] = 0;
  }
  phoneNumberLength = 0;
  startDialtone = true;
}

void beginCall() {
#ifdef USB_DEBUG
  Serial.println("Starting Call");
#endif
  fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End dialtone
  if ( fona.callPhone( phoneNumber ) ) {
#ifdef USB_DEBUG
    Serial.println("Call Started");
#endif
    inCall();
  }
#ifdef USB_DEBUG
  Serial.println("Call ended");
#endif
}

void resumeDialtone() {
  fona.sendCheckReply( F("AT+STTONE=1,20,500" ), F("OK") ); // Start dialtone
}

//////////////////////////////////
///// ARDUINO CORE FUNCTIONS /////
//////////////////////////////////

void setup() {
#ifdef USB_DEBUG
  Serial.begin(9600);
  Serial.println("Booted. Setting up");
#endif
  pinMode( WAKE_PIN, INPUT_PULLUP );
  pinMode( BUT_ANS, INPUT_PULLUP );
  pinMode( BUT_END, INPUT_PULLUP );
  pinMode( GSM_RST, OUTPUT );
  pinMode( GSM_RING, INPUT_PULLUP );
  pinMode( GSM_KEY, OUTPUT );

  digitalWrite( GSM_KEY, LOW );
  digitalWrite( GSM_RST, HIGH );

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
#ifdef USB_DEBUG
    Serial.println("Fona failed to respond");
#endif
    while (1); //fona didn't start
  }
  while ( !fona.setAudio(FONA_EXTAUDIO) ) {}

#ifdef USB_DEBUG
  Serial.println("Setup Complete");
#endif
  startDialtone = true;
  lastActiveTime = millis();

  fona.sendCheckReply( F("AT+CLVL=20"), F("OK") ); // set dialtone volume
}

void loop() {
  if ( !digitalRead(GSM_RING) ) {
    while ( digitalRead(BUT_ANS) & !digitalRead(GSM_RING) ) delay(10); // Wait for answer button or end ring/call
    if ( !digitalRead(BUT_ANS) ) {
      fona.pickUp();
      delay(500);
      inCall();
    }
  }

  if ( startDialtone ) {
    resumeDialtone();
    startDialtone = false;
  }

  // Read from keypad
  char key = keypad.getKey();
  if ( key != NO_KEY ) {
    phoneNumber[ phoneNumberLength ] = key;
    phoneNumberLength = phoneNumberLength + 1;

    fona.playDTMF( key );  // Play DTMF tone

    lastActiveTime = millis();
#ifdef USB_DEBUG
    int i;
    for (i = 0; i < 15; i = i + 1) {
      Serial.print(phoneNumber[i]);
      Serial.print(", ");
    }
    Serial.println(phoneNumberLength);
#endif
    // Check for complete phone number (including +1 country code)
    if ( ( phoneNumberLength == 10 & phoneNumber[0] != '1' ) || phoneNumberLength > 10 ) {
      beginCall();
    }
  }


  // Clear stored phone number on press of end button
  if ( !digitalRead(BUT_END) ) {
    for ( int j = 0; j < phoneNumberLength; j++) {
      phoneNumber[j] = 0;
    }
    phoneNumberLength = 0;
#ifdef USB_DEBUG
    Serial.println( phoneNumberLength );
#endif
    resumeDialtone();
  }

#ifdef SLEEP
  // Autoshutdown if inactive for extended period
  // Typecast to long avoids "rollover" issues
  if ( (long)(millis() - lastActiveTime) > TIMEOUT_SLEEP ) {
    goToSleep();
  }
#endif
}





