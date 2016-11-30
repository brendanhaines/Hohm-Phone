#include "avr/sleep.h"
#include "Adafruit_FONA.h"
#include "Keypad.h"

////////////////////////////////
////////// PARAMETERS //////////
////////////////////////////////

#define LED_BAT_LOW 3
#define LED_NO_SERVICE 2

#define BUT_ANS A3
#define BUT_END A4

#define GSM_RST A2
#define GSM_RING A5

// Charging voltage thresholds. Will turn on charging at CHG_VLO and turn off charging at CHG_VHI (millivolts)
#define CHG_VLO 3900
#define CHG_VHI 4100
#define CHG_PIN A0

#define MAG_SENSE A1

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
bool dialtoneActive = false;
bool startDialtone = false;

HardwareSerial *fonaSerial = &Serial;

Adafruit_FONA fona = Adafruit_FONA(GSM_RST);
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, 4, 3 );


///////////////////////////////
////////// FUNCTIONS //////////
///////////////////////////////

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
  fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End dialtone
  dialtoneActive = false;
  if ( fona.callPhone( phoneNumber ) ) {
    inCall();
  }
}

void resumeDialtone() {
  fona.sendCheckReply( F("AT+STTONE=1,20,30000" ), F("OK") ); // Start dialtone
  dialtoneActive = true;
}

//////////////////////////////////
///// ARDUINO CORE FUNCTIONS /////
//////////////////////////////////

void setup() {
  pinMode( BUT_ANS, INPUT_PULLUP );
  pinMode( BUT_END, INPUT_PULLUP );
  pinMode( GSM_RST, OUTPUT );
  pinMode( GSM_RING, INPUT_PULLUP );
  pinMode( CHG_PIN, OUTPUT );
  pinMode( LED_BAT_LOW, OUTPUT);
  pinMode( LED_NO_SERVICE, OUTPUT);

  digitalWrite( LED_BAT_LOW, HIGH );
  digitalWrite( LED_NO_SERVICE, HIGH );

  digitalWrite( GSM_RST, HIGH );
  digitalWrite( CHG_PIN, HIGH );

  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    while (1); //fona didn't start
    digitalWrite( LED_BAT_LOW, HIGH );
  }
  //while ( !fona.setAudio(FONA_EXTAUDIO) ) {}

  startDialtone = true;
  lastActiveTime = millis();
  
  digitalWrite( LED_BAT_LOW, LOW );
  fona.sendCheckReply( F("AT+CLVL=20"), F("OK") ); // set dialtone volume

  digitalWrite( LED_BAT_LOW, LOW );
  digitalWrite( LED_NO_SERVICE, LOW );
}

void loop() {
  if ( !digitalRead(GSM_RING) ) {
    while ( digitalRead(BUT_ANS) & !digitalRead(GSM_RING) ) delay(10); // Wait for answer button or end ring/call
    if ( !digitalRead(BUT_ANS) ) {
      fona.pickUp();
      delay(100);
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

    if ( dialtoneActive ) {
      fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End dialtone
      dialtoneActive = false;
      delay(50);
    }
    fona.playDTMF( key );  // Play DTMF tone

    lastActiveTime = millis();
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
    resumeDialtone();
  }

  // Battery Management
  uint16_t vbat;
  fona.getBattVoltage(&vbat);
  if ( vbat < CHG_VLO ) {
    digitalWrite( CHG_PIN, HIGH );
  } else if ( vbat > CHG_VHI ) {
    digitalWrite( CHG_PIN, LOW );
  }

}





