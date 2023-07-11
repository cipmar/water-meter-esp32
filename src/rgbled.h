#include <NeoPixelBusLg.h>

#ifdef MOD_NEOPIXEL
extern NeoPixelBusLg<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> strip;
#endif

// TinyPico LED Color
#define DOTSTAR_BLACK     0x000000
#define DOTSTAR_RED       0xFF0000
#define DOTSTAR_ORANGE    0xFF2200
#define DOTSTAR_YELLOW    0xFFAA00
#define DOTSTAR_GREEN     0x00FF00
#define DOTSTAR_CYAN      0x00FFFF
#define DOTSTAR_BLUE      0x0000FF
#define DOTSTAR_VIOLET    0x9900FF
#define DOTSTAR_MAGENTA   0xFF0033
#define DOTSTAR_PINK      0xFF3377
#define DOTSTAR_AQUA      0x557DFF 
#define DOTSTAR_WHITE     0xFFFFFF


// Simulate used TinyPico Function
// To be done, fill theese fonction to relevant code specific 
// to board used (Hardware GPIO)
void DotStar_SetBrightness(uint8_t b);
void DotStar_SetPixelColor(uint32_t c);
void DotStar_SetPixelColor(uint32_t c, bool show);
void DotStar_SetPixelColor(uint16_t index, uint32_t c, bool show=true);
void DotStar_Clear();
void DotStar_Show();
