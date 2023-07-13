 /*  the radian_trx SW shall not be distributed  nor used for commercial product*/
 /*  it is exposed just to demonstrate CC1101 capability to reader water meter indexes */
 /*  there is no Warranty on radian_trx SW */

#include "Arduino.h"
#include "time.h"
#include "stdio.h"
#include "stdarg.h"
#include "stdlib.h"
#include "stdint.h"
#include "string.h"
#include "cc1101.h"
#include "rgbled.h"
#include "utils.h"

#ifndef LED_BUILTIN
// Change this pin if needed
#define LED_BUILTIN NOT_A_PIN
#else
#ifdef MOD_NEOPIXEL
// Avoid same pin led buildin and NeoPixel
#undef LED_BUILTIN
#define LED_BUILTIN NOT_A_PIN
#endif
#endif

#define CRLF "\r\n"

#if defined (ESP32C3)
#include "driver/adc.h"
#include "esp_adc_cal.h"
#define BATT_CHANNEL ADC1_CHANNEL_4  // Battery voltage ADC input (GPIO4)
#endif



