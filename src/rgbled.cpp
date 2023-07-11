
#include "rgbled.h"

#ifdef MOD_NEOPIXEL
NeoPixelBusLg<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip( NEOPIXEL_LEDS, NEOPIXEL_DATA);
uint32_t R, G, B;
uint8_t rgb_brightness = MY_RGB_BRIGHTNESS; 


void DotStar_SetBrightness(uint8_t b)  { 
  strip.SetLuminance(b);
}

void DotStar_SetPixelColor(uint32_t c, bool show ) { 
  strip.SetPixelColor(0, HtmlColor(c));
  if (show) {
    strip.Show();
  }
}

void DotStar_SetPixelColor(uint32_t c ) { 
  DotStar_SetPixelColor( c, true);
}

void DotStar_SetPixelColor(uint16_t index, uint32_t c, bool show) { 
  strip.SetPixelColor(index, HtmlColor(c));
  if (show) {
    strip.Show();
  }
}

void DotStar_Clear() { 
  strip.ClearTo(0);
  strip.Show();
}

void DotStar_Show() { 
  strip.Show();
}

#else 

void DotStar_SetBrightness(uint8_t b) {}
void DotStar_SetPixelColor(uint32_t c, bool show ) {}
void DotStar_SetPixelColor(uint32_t c ) {}
void DotStar_SetPixelColor(uint16_t index, uint32_t c, bool show) {}
void DotStar_Clear() {}
void DotStar_Show() {}

#endif // MOD_NEOPIXEL
