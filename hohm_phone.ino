#include <avr/sleep.h>
#include "Adafruit_FONA.h"

//////////////////////
///// PARAMETERS /////
//////////////////////

// Arduino will wake from sleep when WAKE_PIN is pulled low
#define WAKE_PIN 2
#define BUT_ANS 7
#define BUT_END 8

// RX/TX are in reference to the Arduino
#define GSM_RX 3
#define GSM_TX 4
#define GSM_RST 5
#define GSM_RING 6

// TIMEOUT_SLEEP is the time to stay awake from last activity until sleep (milliseconds)
#define TIMEOUT_SLEEP

// Comment the following line to use HW serial
#define usb_testing_config

//////////////////////////
///// END PARAMETERS /////
//////////////////////////

char replybuffer[255];
uint8_t phonenumber[15];
uint8_t phonenumberlength = 0;

#ifdef usb_testing_config
#include "SoftwareSerial.h"
SoftwareSerial fonaSS = SoftwareSerial(GSM_RX, GSM_TX);
SoftwareSerial *fonaSerial = &fonaSS;
#else
HardwareSerial *fonaSerial = &Serial;
#endif

Adafruit_FONA fona = Adafruit_FONA(GSM_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);


void wake_from_sleep() {
  sleep_disable();
  detachInterrupt(WAKE_PIN);
}

void go_to_sleep() {
  sleep_enable();
  attachInterrupt(WAKE_PIN, wake_from_sleep, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  cli();
  sleep_bod_disable();
  sei();
  sleep_cpu();
}

void in_call() {
  while (1) {
    if ( !digitalRead(BUT_END) ) { // End button pressed
      if ( fona.hangUp() ) {
        break;
      }
    }
    // numpad stuff
  }
}

void begin_call() {
  fona.sendCheckReply( F("AT+STTONE=0"), F("OK") ); // End tone
  for(int i = 0; i<phonenumberlength; i++){
    fona.playDTMF( phonenumber[i] );
    if( fona.callPhone( phonenumber ) ) {
      in_call();
    }
    for( int j = 0; j<phonenumberlength; j++) {
      phonenumber[j] = 0;
    }
    phonenumberlength = 0;
  }
}

void setup() {
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) while (1); //fona didn't start
}

void loop() {
  if ( !digitalRead(GSM_RING) ) {
    while ( digitalRead(BUT_ANS) & !digitalRead(GSM_RING) ) delay(10); // Wait for answer button or end ring/call
    if ( !digitalRead(BUT_ANS) ) {
      if ( fona.pickUp() ) {
        in_call();
      }
    }
  }

  fona.sendCheckReply( F("AT+STTONE=1,20,1000"), F("OK") ); // Start tone
}





