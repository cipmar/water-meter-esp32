#ifndef LC709203F_H
#define LC709203F_H

#include "Arduino.h"
#include <Wire.h>

#ifndef SerialDebug
#define SerialDebug Serial
#endif

#define LC709203F_I2CADDR_DEFAULT 0x0B     ///< LC709203F default i2c address
#define LC709203F_CMD_THERMISTORB 0x06     ///< Read/write thermistor B
#define LC709203F_CMD_INITRSOC 0x07        ///< Initialize RSOC calculation
#define LC709203F_CMD_CELLTEMPERATURE 0x08 ///< Read/write batt temperature
#define LC709203F_CMD_CELLVOLTAGE 0x09     ///< Read batt voltage
#define LC709203F_CMD_DIRECTION 0x0A       ///< Current direction
#define LC709203F_CMD_APA 0x0B             ///< Adjustment Pack Application
#define LC709203F_CMD_RSOC 0x0D            ///< Read state of charge
#define LC709203F_CMD_CELLITE 0x0F         ///< Read batt indicator to empty
#define LC709203F_CMD_ICVERSION 0x11       ///< Read IC version
#define LC709203F_CMD_BATTPROF 0x12        ///< Set the battery profile
#define LC709203F_CMD_ALARMRSOC 0x13       ///< Alarm on percent threshold
#define LC709203F_CMD_ALARMVOLT 0x14       ///< Alarm on voltage threshold
#define LC709203F_CMD_POWERMODE 0x15       ///< Sets sleep/power mode
#define LC709203F_CMD_STATUSBIT 0x16       ///< Temperature obtaining method
#define LC709203F_CMD_PARAMETER 0x1A       ///< Batt profile code

static uint8_t crc8(uint8_t *data, int len);

/*!  Battery temperature source */
typedef enum {
  LC709203F_TEMPERATURE_I2C = 0x0000,
  LC709203F_TEMPERATURE_THERMISTOR = 0x0001,
} lc709203_tempmode_t;

/*!  Chip power state */
typedef enum {
  LC709203F_POWER_OPERATE = 0x0001,
  LC709203F_POWER_SLEEP = 0x0002,
} lc709203_powermode_t;

/*!  Approx battery pack size */
typedef enum {
  LC709203F_APA_100MAH = 0x08,
  LC709203F_APA_200MAH = 0x0B,
  LC709203F_APA_500MAH = 0x10,
  LC709203F_APA_1000MAH = 0x19,
  LC709203F_APA_2000MAH = 0x2D,
  LC709203F_APA_3000MAH = 0x36,
} lc709203_adjustment_t;

bool lc_begin(TwoWire *wire = &Wire);
bool lc_initRSOC(void);

bool lc_setPowerMode(lc709203_powermode_t t);
bool lc_setPackSize(lc709203_adjustment_t apa);
bool lc_setPackAPA(uint8_t apa_value);

uint16_t lc_getICversion(void);
uint16_t lc_cellVoltage(void);
uint16_t lc_cellPercent(void);

uint16_t lc_getThermistorB(void);
bool lc_setThermistorB(uint16_t b);

uint16_t lc_getBattProfile(void);
bool lc_setBattProfile(uint16_t b);

bool lc_setTemperatureMode(lc709203_tempmode_t t);
float lc_getCellTemperature(void);

bool lc_setAlarmRSOC(uint8_t percent);
bool lc_setAlarmVoltage(float voltage);

uint16_t lc_getCurrentDirection(void) ;


bool lc_readWord(uint8_t command, uint16_t *data) ;
bool lc_writeWord(uint8_t command, uint16_t data) ;
uint8_t lc_crc8(uint8_t *data, int len) ;

#endif