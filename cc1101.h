#ifndef __CC1101_H__
#define __CC1101_H__

struct tmeter_data {
  int liters;
  int reads_counter; // how many times the meter has been readed
  int battery_left; //in months
  int time_start; // like 8am
  int time_end; // like 4pm
};

void  cc1101_init(void);
struct tmeter_data get_meter_data(void);

#endif // __CC1101_H__