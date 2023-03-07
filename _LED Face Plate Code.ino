#include <FastLED.h>
#include <EEPROM.h>
#include <JC_Button.h>

#define LED_PIN           5           // Output pin for LEDs [5]
#define MODE_PIN          3           // Input pin for button to change mode [3]
#define PLUS_PIN          2           // Input pin for plus button [2]
#define MINUS_PIN         4           // Input pin for minus button [4]
#define MIC_PIN           A6          // Input pin for microphone [A6]
#define COLOR_ORDER       GRB         // Color order of LED string [GRB]
#define CHIPSET           WS2812B     // LED string type [WS2182B]
#define BRIGHTNESS        50          // Overall brightness [50]
#define LAST_VISIBLE_LED  102         // Last LED that's visible [102]
#define MAX_MILLIAMPS     500         // Max current in mA to draw from supply [500]
#define SAMPLE_WINDOW     100         // How many ms to sample audio for [100]
#define DEBOUNCE_MS       20          // Number of ms to debounce the button [20]
#define LONG_PRESS        500         // Number of ms to hold the button to count as long press [500]
#define PATTERN_TIME      10          // Seconds to show each pattern on autoChange [10]
#define kMatrixWidth      15          // Matrix width [15]
#define kMatrixHeight     11          // Matrix height [11]
#define NUM_LEDS (kMatrixWidth * kMatrixHeight)                                       // Total number of Leds
#define MAX_DIMENSION ((kMatrixWidth>kMatrixHeight) ? kMatrixWidth : kMatrixHeight)   // Largest dimension of matrix

CRGB leds[ NUM_LEDS ];
uint8_t brightness = BRIGHTNESS;
uint8_t soundSensitivity = 10;

// Used to check RAM availability. Usage: Serial.println(freeRam());
int freeRam () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// Button stuff
uint8_t buttonPushCounter = 10;
uint8_t state = 0;
bool autoChangeVisuals = false;
Button modeBtn(MODE_PIN, DEBOUNCE_MS);
Button plusBtn(PLUS_PIN, DEBOUNCE_MS);
Button minusBtn(MINUS_PIN, DEBOUNCE_MS);

void incrementButtonPushCounter() {
  buttonPushCounter = (buttonPushCounter + 1) %11;
  EEPROM.write(1, buttonPushCounter);
}

// Include various patterns
#include "Sound.h"
#include "Rainbow.h"
#include "Fire.h"
#include "Squares.h"
#include "Circles.h"
#include "Plasma.h"
#include "Matrix.h"
#include "CrossHatch.h"
#include "Drops.h"
#include "Snake.h"

// Helper to map XY coordinates to irregular matrix
uint16_t XY( uint8_t x, uint8_t y) {
  // any out of bounds address maps to the first hidden pixel
  if ( (x >= kMatrixWidth) || (y >= kMatrixHeight) ) {
    return (LAST_VISIBLE_LED + 1);
  }

  const uint8_t XYTable[] = {
   188, 187, 186, 185, 184, 183, 182, 181,   6,   5,   4,   3,   2,   1,   0, 180, 179, 178, 177, 176, 175, 174, 173,
   189, 190, 191, 192, 193, 194,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17, 195, 196, 197, 198, 199, 200,
   208, 207, 206, 205,  32,  31,  30,  29,  28,  27,  26,  25,  24,  23,  22,  21,  20,  19,  18, 204, 203, 202, 201,
   209, 210,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51, 211, 212,
   214,  72,  71,  70,  69,  68,  67,  66,  65,  64,  63,  62,  61,  60,  59,  58,  57,  56,  55,  54,  53,  52, 213,
    73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
   216, 116, 115, 114, 113, 112, 111, 110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100,  99,  98,  97,  96, 215,
   217, 218, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 219, 220,
   228, 227, 226, 225, 150, 149, 148, 147, 146, 145, 144, 143, 142, 141, 140, 139, 138, 137, 136, 224, 223, 222, 221,
   229, 230, 231, 232, 233, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 234, 235, 236, 237, 238,
   252, 251, 250, 249, 248, 247, 246, 172, 171, 170, 169, 168, 167, 166, 165, 164, 245, 244, 243, 242, 241, 240, 239
  };

  uint8_t i = (y * kMatrixWidth) + x;
  uint8_t j = XYTable[i];
  return j;
}

void setup() {
  FastLED.addLeds < CHIPSET, LED_PIN, COLOR_ORDER > (leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);

  modeBtn.begin();
  plusBtn.begin();
  minusBtn.begin();
  buttonPushCounter = (int)EEPROM.read(1);    // load previous setting
  
  Serial.begin(57600);
  Serial.print(F("Starting pattern "));
  Serial.println(buttonPushCounter);
}

void checkBrightnessButton() {
  // On all none-sound reactive patterns, plus and minus control the brightness
  plusBtn.read();
  minusBtn.read();

  if (plusBtn.wasReleased()) {
    brightness += 20;
    FastLED.setBrightness(brightness);
  }

  if (minusBtn.wasReleased()) {
    brightness -= 20;
    FastLED.setBrightness(brightness);
  }
}

void checkSoundLevelButton(){
  // On all sound reactive patterns, plus and minus control the sound sensitivity
  plusBtn.read();
  minusBtn.read();

  if (plusBtn.wasReleased()) {
    // Increase sound sensitivity
    soundSensitivity -= 1;
    if (soundSensitivity == 0) soundSensitivity = 1;
  }

  if (minusBtn.wasReleased()) {
    // Decrease sound sensitivity
    soundSensitivity += 1;
  }
}

bool checkModeButton() {  
  modeBtn.read();

  switch (state) {
    case 0:                
      if (modeBtn.wasReleased()) {
        incrementButtonPushCounter();
        Serial.print(F("Short press, pattern "));
        Serial.println(buttonPushCounter);
        autoChangeVisuals = false;
        return true;
      }
      else if (modeBtn.pressedFor(LONG_PRESS)) {
        state = 1;
        return true;
      }
      break;

    case 1:
      if (modeBtn.wasReleased()) {
        state = 0;
        Serial.print(F("Long press, auto, pattern "));
        Serial.println(buttonPushCounter);
        autoChangeVisuals = true;
        return true;
      }
      break;
  }

  if(autoChangeVisuals){
    EVERY_N_SECONDS(PATTERN_TIME) {
      incrementButtonPushCounter();
      Serial.print("Auto, pattern ");
      Serial.println(buttonPushCounter); 
      return true;
    }
  }  

  return false;
}

// Functions to run patterns. Done this way so each class stays in scope only while
// it is active, freeing up RAM once it is changed.

void runSound(){
  bool isRunning = true;
  Sound sound = Sound();
  while(isRunning) isRunning = sound.runPattern();
}

void runRainbow(){
  bool isRunning = true;
  Rainbow rainbow = Rainbow();
  while(isRunning) isRunning = rainbow.runPattern();
}

void runFire(){
  bool isRunning = true;
  Fire fire = Fire();
  while(isRunning) isRunning = fire.runPattern();
}

void runSquares(){
  bool isRunning = true;
  Squares squares = Squares();
  while(isRunning) isRunning = squares.runPattern();
}

void runCircles(){
  bool isRunning = true;
  Circles circles = Circles();
  while(isRunning) isRunning = circles.runPattern();
}

void runPlasma(){
  bool isRunning = true;
  Plasma plasma = Plasma();
  while(isRunning) isRunning = plasma.runPattern();
}

void runMatrix(){
  bool isRunning = true;
  Matrix matrix = Matrix();
  while(isRunning) isRunning = matrix.runPattern();
}

void runCrossHatch(){
  bool isRunning = true;
  CrossHatch crossHatch = CrossHatch();
  while(isRunning) isRunning = crossHatch.runPattern();
}

void runDrops(){
  bool isRunning = true;
  Drops drops = Drops();
  while(isRunning) isRunning = drops.runPattern();
}


void runSnake(){
  bool isRunning = true;
  Snake snake = Snake();
  while(isRunning) {
    isRunning = snake.runPattern();
  }
}

// Run selected pattern
void loop() {
  switch (buttonPushCounter) {
    case 0:
      runSound();
      break;
    case 1:
      runRainbow();
      break;
    case 2:
      runFire();
      break;
    case 3:
      runSquares();
      break;
    case 4:
      runCircles();
      break;
    case 5:
      runPlasma();
      break;
    case 6:
      runMatrix();
      break;
    case 7:
      runCrossHatch();
      break;
    case 8:
      runDrops();
      break;
      case 9:
      runSnake();
      break;
  }
}