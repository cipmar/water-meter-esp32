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
  int reads_counter; // how many times the meter has been readed
  int battery_left; //in months
  int time_start; // like 8am
  int time_end; // like 4pm
};

void setMHZ(float mhz);
bool  cc1101_init(float freq, bool show = false);
struct tmeter_data get_meter_data(void);

#endif // __CC1101_H__