#include "Arduino.h"
#include "LC709203F.h"

uint8_t lc_addr = LC709203F_I2CADDR_DEFAULT;
uint8_t lc_perccent_offset = 0;
TwoWire* pwire = nullptr;


bool lc_begin(TwoWire *wire) 
{
  pwire = wire;
  uint16_t param ;

  SerialDebug.print(F("LC709203F_CMD_ICVERSION "));
  if (!lc_readWord(LC709203F_CMD_ICVERSION, &param)) {
    SerialDebug.println(F("Error"));
    return false;
  }
  SerialDebug.printf_P(PSTR("%04X\r\n"), param);

  SerialDebug.print(F("LC709203F_CMD_PARAMETER "));
  if (!lc_readWord(LC709203F_CMD_PARAMETER, &param)) {
    SerialDebug.println(F("Error"));
    return false;
  }
  SerialDebug.printf_P(PSTR("%04X\r\n"), param);

  // Chip is   LC709203Fxx−05xx (incorrect type 3.8V 4.35V) 
  // should be LC709203Fxx−01xx (type 3.7V 4.2V) 
  if (param == 0x0706) {
    // Add 10% to percentage calculation
    lc_perccent_offset = 10 ;
    SerialDebug.printf_P(PSTR("Bad Battery chip version, fix offset by %d%%"), lc_perccent_offset);
  }

  if (!lc_setPowerMode(LC709203F_POWER_OPERATE)) {
    return false;
  }

  // use 4.2V profile
  if (!lc_setBattProfile(0x1)) {
    return false;
  }

  if (!lc_setTemperatureMode(LC709203F_TEMPERATURE_THERMISTOR))
    return false;
  return true;
}

uint16_t lc_getICversion(void) 
{
  uint16_t vers = 0;
  lc_readWord(LC709203F_CMD_ICVERSION, &vers);
  return vers;
}

bool lc_initRSOC(void) 
{
  return lc_writeWord(LC709203F_CMD_INITRSOC, 0xAA55);
}

uint16_t lc_cellVoltage(void) 
{
  uint16_t voltage = 0;
  lc_readWord(LC709203F_CMD_CELLVOLTAGE, &voltage);
  return voltage ;
}

uint16_t lc_cellPercent(void) 
{
  uint16_t percent = 0;
  lc_readWord(LC709203F_CMD_CELLITE, &percent);
  percent += lc_perccent_offset * 10;
  return percent>1000 ? 1000 : percent;
}

float lc_getCellTemperature(void) 
{
  uint16_t temp = 0;
  lc_readWord(LC709203F_CMD_CELLTEMPERATURE, &temp);
  float tempf = map(temp, 0x9E4, 0xD04, -200, 600);
  return tempf / 10.0;
}

bool lc_setTemperatureMode(lc709203_tempmode_t t) 
{
  return lc_writeWord(LC709203F_CMD_STATUSBIT, (uint16_t)t);
}

bool lc_setPackSize(lc709203_adjustment_t apa) 
{
  return lc_writeWord(LC709203F_CMD_APA, (uint16_t)apa);
}

bool lc_setPackAPA(uint8_t apa_value) 
{
  return lc_writeWord(LC709203F_CMD_APA, (uint16_t)apa_value);
}

bool lc_setAlarmRSOC(uint8_t percent) 
{
  return lc_writeWord(LC709203F_CMD_ALARMRSOC, percent);
}

bool lc_setAlarmVoltage(float voltage) 
{
  return lc_writeWord(LC709203F_CMD_ALARMVOLT, voltage * 1000);
}

bool lc_setPowerMode(lc709203_powermode_t t) {
  return lc_writeWord(LC709203F_CMD_POWERMODE, (uint16_t)t);
}

uint16_t lc_getThermistorB(void) 
{
  uint16_t val = 0;
  lc_readWord(LC709203F_CMD_THERMISTORB, &val);
  return val;
}

bool lc_setThermistorB(uint16_t b) 
{
  return lc_writeWord(LC709203F_CMD_THERMISTORB, b);
}

uint16_t lc_getBattProfile(void) 
{
  uint16_t val = 0;
  lc_readWord(LC709203F_CMD_BATTPROF, &val);
  return val;
}

bool lc_setBattProfile(uint16_t b) 
{
  return lc_writeWord(LC709203F_CMD_BATTPROF, b);
}

uint16_t lc_getCurrentDirection(void) 
{
  uint16_t val = 0;
  lc_readWord(LC709203F_CMD_DIRECTION, &val);
  return val;
}

bool lc_readWord(uint8_t command, uint16_t *data) 
{
  size_t recv ;
  uint8_t reply[6];
  int err;
  reply[0] = (lc_addr << 1); // writebyte
  reply[1] = command ;       // command
  reply[2] = reply[0] | 0x1; // read byte

  // Write byte
  pwire->beginTransmission(lc_addr);
  if ( pwire->write(command) != 1 ) {
    return false;
  }
  if ( pwire->endTransmission(false) != 0 ) {
    return false;
  } 

  //pwire->beginTransmission(reply[2]);
  recv = pwire->requestFrom(lc_addr, (uint8_t)3);
  if (recv != 3) {
    return false;
  }
  reply[3] = Wire.read();
  reply[4] = Wire.read();
  reply[5] = Wire.read();
  uint8_t crc = lc_crc8(reply, 5);

/*
  SerialDebug.print(F("\tI2CREAD  @ 0x"));
  SerialDebug.print(lc_addr, HEX);
  SerialDebug.print(F(" :: "));
  for (uint16_t i = 0; i < sizeof(reply); i++) {
    SerialDebug.print(F("0x"));
    SerialDebug.print(reply[i], HEX);
    SerialDebug.print(F(", "));
  }
  SerialDebug.print(F(" CRC 0x"));
  SerialDebug.println(crc, HEX);
*/
  // CRC failure?
  if (crc != reply[5]) {
    return false;
  }

  *data = reply[4];
  *data <<= 8;
  *data |= reply[3];

  return true;
}

bool lc_writeWord(uint8_t command, uint16_t data) 
{
  uint8_t send[5];
  send[0] = lc_addr << 1; // write byte
  send[1] = command;      // command / register
  send[2] = data & 0xFF;
  send[3] = data >> 8;
  send[4] = lc_crc8(send, 4);

  pwire->beginTransmission(lc_addr);
  if (pwire->write(&send[1], 4) != 4) {
    return false;
  }

  if (pwire->endTransmission() == 0) {
    return true;
  } else {
    return false;
  }
}

uint8_t lc_crc8(uint8_t *data, int len) 
{
  const uint8_t POLYNOMIAL(0x07);
  uint8_t crc(0x00);

  for (int j = len; j; --j) {
    crc ^= *data++;

    for (int i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
    }
  }
  return crc;
}