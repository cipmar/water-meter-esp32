#ifndef __CC1101_H__
#define __CC1101_H__

#ifndef SPI_CSK
#define SPI_CSK  SCK
#endif
#ifndef SPI_MISO
#define SPI_MISO MISO
#endif
#ifndef SPI_MOSI
#define SPI_MOSI MOSI
#endif
#ifndef SPI_SS
#define SPI_SS   SS
#endif

struct tmeter_data {
  int liters;
  int reads_counter;  // how many times the meter has been readed
  int battery_left;   // in months remaining
	int time_start;     // like 8am
	int time_end;       // like 4pm
  int8_t rssi;        // Signal RSSI
  int8_t lqi;         // Signal LQI
	bool ok;            // True if read was ok
};

#define REG_DEFAULT 	0x10AF75 // CC1101 register values for 433.82MHz

void setMHZ(float mhz);
void setFREQxRegister(uint32_t freg) ;
bool cc1101_init(float freq, uint32_t freg, bool show = false);
void cc1101_sleep() ;
struct tmeter_data get_meter_data(void);

extern int _spi_speed;

#endif // __CC1101_H__