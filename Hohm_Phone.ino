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
#define GSM_BAUDRATE 115200

// Low battery light threshold
#define CHG_VLO 3800

// Time in miliseconds to stop listening for keypad input
#define SLEEP_TIMEOUT 120000

// RSSI value below which No Service LED will light
#define RSSI_THRESHOLD 1

// Keypad pinout
byte rowPins[4] = {5, 10, 9, 7};
byte colPins[3] = {6, 4, 8};

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
bool awake = true;

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
      break;
    }

    // numpad stuff
  }
  clearPhoneNumber();
  startDialtone = true;
  delay(100);
}

void beginCall() {
  fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End dialtone
  dialtoneActive = false;
  if ( fona.callPhone( phoneNumber ) ) {
    inCall();
  }
}

void resumeDialtone() {
  fona.sendCheckReply( ("AT+STTONE=1,20," + String(SLEEP_TIMEOUT)).c_str(), "OK" ); // Start dialtone
  dialtoneActive = true;
}

void clearPhoneNumber() {
  for ( int j = 0; j < phoneNumberLength; j++) {
    phoneNumber[j] = 0;
  }
  phoneNumberLength = 0;
}

void goToSleep() {
  digitalWrite( LED_NO_SERVICE, LOW );
  digitalWrite( LED_BAT_LOW, LOW );
  awake = false;
}

//////////////////////////////////
///// ARDUINO CORE FUNCTIONS /////
//////////////////////////////////

void setup() {
  pinMode( BUT_ANS, INPUT_PULLUP );
  pinMode( BUT_END, INPUT_PULLUP );
  pinMode( GSM_RST, OUTPUT );
  pinMode( GSM_RING, INPUT_PULLUP );
  pinMode( LED_BAT_LOW, OUTPUT);
  pinMode( LED_NO_SERVICE, OUTPUT);

  digitalWrite( LED_BAT_LOW, HIGH );
  digitalWrite( LED_NO_SERVICE, HIGH );

  digitalWrite( GSM_RST, HIGH );
  fonaSerial->begin(GSM_BAUDRATE);
  if (! fona.begin(*fonaSerial)) {
    while (1); //fona didn't start
    digitalWrite( LED_BAT_LOW, HIGH );
  }

  startDialtone = true;
  lastActiveTime = millis();

  fona.sendCheckReply( F("AT+CLVL=100"), F("OK") ); // set volume

  digitalWrite( LED_BAT_LOW, LOW );
  digitalWrite( LED_NO_SERVICE, LOW );
}

void loop() {
  if ( awake ) {

    // Handle Incoming Call
    if ( !digitalRead(GSM_RING) ) {
      while ( digitalRead(BUT_ANS) & !digitalRead(GSM_RING) ) delay(10); // Wait for answer button or end ring/call
      if ( !digitalRead(BUT_ANS) ) {
        fona.pickUp();
        delay(100);
        inCall();
      }
    }

    // Begin dialtone if necessary
    if ( startDialtone ) {
      resumeDialtone();
      startDialtone = false;
    }

    // Read keypad input
    char key = keypad.getKey();
    if ( key != NO_KEY ) {
      phoneNumber[ phoneNumberLength ] = key;
      phoneNumberLength = phoneNumberLength + 1;

      if ( dialtoneActive ) {
        fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End dialtone
        dialtoneActive = false;
        delay(100);
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
      clearPhoneNumber();
      resumeDialtone();
    }

    // Indicator LEDs
    uint16_t vbat;
    fona.getBattVoltage(&vbat);
    if ( vbat < CHG_VLO ) {
      digitalWrite( LED_BAT_LOW, HIGH );
    } else {
      digitalWrite( LED_BAT_LOW, LOW );
    }

    uint8_t rssi;
    rssi = fona.getRSSI();
    if ( rssi < RSSI_THRESHOLD || rssi == 99 ) {
      digitalWrite( LED_NO_SERVICE, HIGH );
    } else {
      digitalWrite( LED_NO_SERVICE, LOW );
    }

    // If inactive, sleep
    if ( (long)(millis() - lastActiveTime) > SLEEP_TIMEOUT ) {
      goToSleep();
    }
  } else {
    // sleeping
    if ( !digitalRead( BUT_ANS ) || !digitalRead( BUT_END ) || !digitalRead( GSM_RING )  ) {
      lastActiveTime = millis();
      awake = true;
    }
  }
}
