/**
 * Convert:
 *  for name in `ls *.wav| cut -d "." -f 1`; do sox $name.wav -c 1 -r 10000 -b 8 -e unsigned-integer $name.raw; done
 * 
 * Pins:
 *  Keypad
 *   Yel - ard 5 - atmega 11 r
 *   Org - ard 8 - atmega 14 r
 *   Red - ard 7 - atmega 13 r
 *   Brn - ard 6 - atmega 12 r
 *   Wht - ard 9 - atmega 15 c
 *   Pink - ard 2 - atmega 4 c
 *  Speaker
 *   Ard 3 + gnd - atmega 5
 *  SD Card (From switch side moving away)
 *   Ard 12 - atmega 18
 *   gnd
 *   Ard 13 - atmega 19
 *   +5
 *   gnd
 *   Ard 11 - atmega 17
 *   Ard 4 - atmega 6
 */

#include <SD.h>
#include <SPI.h>
#include <Keypad.h>

File wav;

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#define SAMPLE_RATE 10000

int speakerPin = 3;
unsigned char buf[512];

void stopPlayback() {
    // Disable playback per-sample interrupt.
    TIMSK1 &= ~_BV(OCIE1A);

    // Disable the per-sample timer completely.
    TCCR1B &= ~_BV(CS10);
}

unsigned long filesize;
volatile unsigned long progress;
bool opened = false;

// This is called at SAMPLE_RATE Hz to load the next sample.
ISR(TIMER1_COMPA_vect) {
    if (progress >= filesize) {
        stopPlayback();
    } else {
      OCR2B = buf[progress % 512]-127;            
    }

    ++progress;
}

void startPlayback() {
  pinMode(speakerPin, OUTPUT);

  // Set up Timer 2 to do pulse width modulation on the speaker
  // pin.

  // Use internal clock (datasheet p.160)
  ASSR &= ~(_BV(EXCLK) | _BV(AS2));

  // Set fast PWM mode  (p.157)
  TCCR2A |= _BV(WGM21) | _BV(WGM20);
  TCCR2B &= ~_BV(WGM22);

  // Do non-inverting PWM on pin OC2B (p.155)
  // On the Arduino this is pin 3.
  TCCR2A = (TCCR2A | _BV(COM2B1)) & ~_BV(COM2B0);
  TCCR2A &= ~(_BV(COM2A1) | _BV(COM2A0));
  // No prescaler (p.158)
  TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  // Set initial pulse width to the first sample.
  //OCR2B = 0;

  cli();

  // Set CTC mode (Clear Timer on Compare Match) (p.133)
  // Have to set OCR1A *after*, otherwise it gets reset to 0!
  TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
  TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

  // No prescaler (p.134)
  TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

  // Set the compare register (OCR1A).
  // OCR1A is a 16-bit register, so we have to do this with
  // interrupts disabled to be safe.
  OCR1A = F_CPU / SAMPLE_RATE;    // 16e6 / 8000 = 2000

  // Enable interrupt when TCNT1 == OCR1A (p.136)
  TIMSK1 |= _BV(OCIE1A);

  progress = 0;
  sei();
}

const byte cols = 2;
const byte rows = 4;
char keys[rows][cols] = {
  '1', '2', '3', '4', '5', '6', '7', '8'
};
byte rowPins[rows] = {5, 6, 7, 8};
byte colPins[cols] = {2, 9};
Keypad kp = Keypad(makeKeymap(keys), rowPins, colPins, rows, cols);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  Serial.println("Welcome aboard.");
  if (!SD.begin(4)) {
    Serial.println("Could not open SD card.");
    return;
  }
  Serial.println("Opened SD.");
}

void startFile(const char *filename) {
  stopPlayback();
  if (opened) wav.close();
  if (!(wav = SD.open(filename, FILE_READ))) {
    Serial.print("Could not open ");
    Serial.println(filename);
    return;
  }
  filesize = wav.size();
  Serial.println("Preloading buffer...");
  for (int i=0; i<256; i++) {
    buf[i] = wav.read()-127;
  }
  opened = true;
  startPlayback();
}

void loop() {
  switch(kp.getKey()) {
    case '1': startFile("strong.raw"); break;
    case '2': startFile("wrong.raw"); break;
    case '3': startFile("dothat.raw"); break;
    case '4': startFile("myway.raw"); break;
    case '5': startFile("gohome.raw"); break;
    case '6': startFile("beep.raw"); break;
    case '7': startFile("nice.raw"); break;
    case '8': startFile("rude.raw"); break;
  }
  if (progress % 256 == 0) {
    unsigned int base = (progress%512==0)?256:0;
    for (int i=0; i<256; i++) {
      buf[base+i] = wav.read()-127;
    }
  }
}
