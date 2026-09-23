#include <Adafruit_NeoPixel.h>

#include "../include/pinout.h"


#define NB_LEDS 240
#define BRIGHTNESS 10

Adafruit_NeoPixel strip(NB_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);


void setup() {
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();
}

void loop() {
  for (int i=0; i<NB_LEDS; i++){
    strip.setPixelColor(i, strip.Color(i%4 == 0 ? 255 : 0, 
                                       i%3 == 0 ? 255 : 0,
                                       i%2 == 0 ? 255 : 0,
                                       (i%4 != 0 && i%3 != 0 && i%2 != 0) ? 255 : 0));
    strip.show();
    delay(10);
  }
  strip.show();
  delay(1000);
  strip.clear();

  for (int i=0; i<NB_LEDS; i++){
    strip.setPixelColor(i, strip.Color(255, 0, 0, 0));
    if (i!=0) {
      strip.setPixelColor(i - 1, 0, 0, 0, 0);
    }
    strip.show();
    delay(10);
  }
  strip.show();
  delay(1000);
  strip.clear();
}