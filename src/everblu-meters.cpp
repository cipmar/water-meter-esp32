#include <ArduinoOTA.h>

#include "everblu_meters.h"

// Project source : 
// http://www.lamaisonsimon.fr/wiki/doku.php?id=maison2:compteur_d_eau:compteur_d_eau

// Require EspMQTTClient library (by Patrick Lapointe) version 1.13.3
// Install from Arduino library manager (and its dependancies)
// https://github.com/plapointe6/EspMQTTClient/releases/tag/1.13.3
#include "EspMQTTClient.h"

#ifndef LED_BUILTIN
// Change this pin if needed
#define LED_BUILTIN NOT_A_PIN
#endif


EspMQTTClient mqtt(
  "MyESSID",            // Your Wifi SSID
  "MyWiFiKey",          // Your WiFi key
  "mqtt.server.com",    // MQTT Broker server ip
  "MQTTUsername",       // Can be omitted if not needed
  "MQTTPassword",       // Can be omitted if not needed
  "EverblueCyble",      // Client name that uniquely identify your device
  1883                  // MQTT Broker server port
);

const char *jsonTemplate = "{ \"liters\":%d, \"counter\":%d, \"battery\":%d, \"timestamp\":\"%s\" }";

int _retry = 0;
void onUpdateData()
{
  struct tmeter_data meter_data;
  meter_data = get_meter_data();

  time_t tnow = time(nullptr);
  struct tm *ptm = gmtime(&tnow);
  SerialDebug.printf("Current date (UTC) : %04d/%02d/%02d %02d:%02d:%02d - %s\n", 
                                    ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, 
                                    String(tnow, DEC).c_str());

  char iso8601[128];
  strftime(iso8601, sizeof iso8601, "%FT%TZ", gmtime(&tnow));

  if (meter_data.reads_counter == 0 || meter_data.liters == 0) {
    SerialDebug.println("Unable to retrieve data from meter. Retry later...");

    // Call back this function in 10 sec (in miliseconds)
    if (_retry++ < 10)
      mqtt.executeDelayed(1000 * 10, onUpdateData);

    return;
  }

  digitalWrite(LED_BUILTIN, LOW); // turned on

  SerialDebug.printf("Liters : %d\nBattery (in months) : %d\nCounter : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter);

  mqtt.publish("everblu/cyble/liters", String(meter_data.liters, DEC), true);
  delay(50); // Do not remove
  mqtt.publish("everblu/cyble/counter", String(meter_data.reads_counter, DEC), true);
  delay(50); // Do not remove
  mqtt.publish("everblu/cyble/battery", String(meter_data.battery_left, DEC), true);
  delay(50); // Do not remove
  mqtt.publish("everblu/cyble/timestamp", iso8601, true); // timestamp since epoch in UTC
  delay(50); // Do not remove

  char json[512];
  sprintf(json, jsonTemplate, meter_data.liters, meter_data.reads_counter, meter_data.battery_left, iso8601);
  mqtt.publish("everblu/cyble/json", json, true); // send all data as a json message
}


// This function calls onUpdateData() every days at 10:00am UTC
void onScheduled()
{
  time_t tnow = time(nullptr);
  struct tm *ptm = gmtime(&tnow);


  // At 10:00:00am UTC
  if (ptm->tm_hour == 10 && ptm->tm_min == 0 && ptm->tm_sec == 0) {

    // Call back in 23 hours
    mqtt.executeDelayed(1000 * 60 * 60 * 23, onScheduled);

    SerialDebug.println("It is time to update data from meter :)");

    // Update data
    _retry = 0;
    onUpdateData();

    return;
  }

  // Every 500 ms
  mqtt.executeDelayed(500, onScheduled);
}


String jsonDiscoveryDevice1(
"{ \
  \"name\": \"Compteur Eau Index\", \
  \"unique_id\": \"water_meter_value\",\
  \"object_id\": \"water_meter_value\",\
  \"icon\": \"mdi:water\",\
  \"state\": \"{{ states(sensor.water_meter_value)|float / 1 }}\",\
  \"unit_of_measurement\": \"L\",\
  \"device_class\": \"water\",\
  \"state_class\": \"total_increasing\",\
  \"qos\": \"0\",\
  \"state_topic\": \"everblu/cyble/liters\",\
  \"force_update\": \"true\",\
  \"device\" : {\
  \"identifiers\" : [\
  \"14071984\" ],\
  \"name\": \"Compteur Eau\",\
  \"model\": \"Everblu Cyble ESP8266/ESP32\",\
  \"manufacturer\": \"Psykokwak\",\
  \"suggested_area\": \"Home\"}\
}");

String jsonDiscoveryDevice2(
"{ \
  \"name\": \"Compteur Eau Batterie\", \
  \"unique_id\": \"water_meter_battery\",\
  \"object_id\": \"water_meter_battery\",\
  \"device_class\": \"battery\",\
  \"icon\": \"mdi:battery\",\
  \"unit_of_measurement\": \"%\",\
  \"qos\": \"0\",\
  \"state_topic\": \"everblu/cyble/battery\",\
  \"value_template\": \"{{ [(value|int), 100] | min }}\",\
  \"force_update\": \"true\",\
  \"device\" : {\
  \"identifiers\" : [\
  \"14071984\" ],\
  \"name\": \"Compteur Eau\",\
  \"model\": \"Everblu Cyble ESP32\",\
  \"manufacturer\": \"Psykokwak\",\
  \"suggested_area\": \"Home\"}\
}");

String jsonDiscoveryDevice3(
"{ \
  \"name\": \"Compteur Eau Compteur\", \
  \"unique_id\": \"water_meter_counter\",\
  \"object_id\": \"water_meter_counter\",\
  \"icon\": \"mdi:counter\",\
  \"qos\": \"0\",\
  \"state_topic\": \"everblu/cyble/counter\",\
  \"force_update\": \"true\",\
  \"device\" : {\
  \"identifiers\" : [\
  \"14071984\" ],\
  \"name\": \"Compteur Eau\",\
  \"model\": \"Everblu Cyble ESP32\",\
  \"manufacturer\": \"Psykokwak\",\
  \"suggested_area\": \"Home\"}\
}");

String jsonDiscoveryDevice4(
  "{ \
  \"name\": \"Compteur Eau Timestamp\", \
  \"unique_id\": \"water_meter_timestamp\",\
  \"object_id\": \"water_meter_timestamp\",\
  \"device_class\": \"timestamp\",\
  \"icon\": \"mdi:clock\",\
  \"qos\": \"0\",\
  \"state_topic\": \"everblu/cyble/timestamp\",\
  \"force_update\": \"true\",\
  \"device\" : {\
  \"identifiers\" : [\
  \"14071984\" ],\
  \"name\": \"Compteur Eau\",\
  \"model\": \"Everblu Cyble ESP32\",\
  \"manufacturer\": \"Psykokwak\",\
  \"suggested_area\": \"Home\"}\
}");

void onConnectionEstablished()
{
  SerialDebug.println("Connected to MQTT Broker :)");
  SerialDebug.println("> Configure time from NTP server.");
  configTzTime("UTC0", "pool.ntp.org");
  SerialDebug.println("> Configure Arduino OTA flash.");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    }
    else { // U_FS
      type = "filesystem";
    }
    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    SerialDebug.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    SerialDebug.println("\nEnd updating.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    SerialDebug.printf("%u%%\r\n", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    SerialDebug.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      SerialDebug.println("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR) {
      SerialDebug.println("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR) {
      SerialDebug.println("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR) {
      SerialDebug.println("Receive Failed");
    }
    else if (error == OTA_END_ERROR) {
      SerialDebug.println("End Failed");
    }
  });
  ArduinoOTA.setHostname("EVERBLUREADER");
  ArduinoOTA.begin();

  mqtt.subscribe("everblu/cyble/trigger", [](const String& message) {
    if (message.length() > 0) {
      SerialDebug.println("Update data from meter from MQTT trigger");
      _retry = 0;
      onUpdateData();
    }
  });

  SerialDebug.println("> Send MQTT config for HA.");
  // Auto discovery
  delay(50); // Do not remove
  mqtt.publish("homeassistant/sensor/water_meter_value/config", jsonDiscoveryDevice1, true);
  delay(50); // Do not remove
  mqtt.publish("homeassistant/sensor/water_meter_battery/config", jsonDiscoveryDevice2, true);
  delay(50); // Do not remove
  mqtt.publish("homeassistant/sensor/water_meter_counter/config", jsonDiscoveryDevice3, true);
  delay(50); // Do not remove
  mqtt.publish("homeassistant/sensor/water_meter_timestamp/config", jsonDiscoveryDevice4, true);
  delay(50); // Do not remove

  onScheduled();
}

extern int _spi_speed;
void setup()
{
  bool led_state = LOW; // turned on
  //pinMode(LED_BUILTIN, OUTPUT);
  //digitalWrite(LED_BUILTIN, led_state); 

  SerialDebug.begin(115200);
  delay(2000);

  // Wait for serial to be up in 2s
  //while (!SerialDebug && millis()<2000) {
  //  delay(250);
  //  led_state = !led_state;
  //  digitalWrite(LED_BUILTIN, led_state); 
  //}
  // turned off
  //digitalWrite(LED_BUILTIN, HIGH); 

  SerialDebug.println();
  SerialDebug.printf("===========================\n");
  SerialDebug.printf("EverBlu Meter Reading\n");
  SerialDebug.printf("Meter Year   : %02d\n", METER_YEAR);
  SerialDebug.printf("Meter Serial : %06d\n", METER_SERIAL);
  SerialDebug.printf("SPI Speed    : %dKHz\n", _spi_speed/1000);
  SerialDebug.printf("===========================\n");

  mqtt.setMaxPacketSize(1024);
  //mqtt.enableDebuggingMessages(true);

  if (!cc1101_init(FREQUENCY,true)) {
    SerialDebug.printf("Unable to find CC1101 Chip !!\n");
  } else {
    SerialDebug.printf("Found CC1101\n");
    float f = FREQUENCY;
    // Scan for correct frequency
    if ( f == 0.0f ) {
      float f_start = 0.0f;
      float f_end = 0.0f;
      // Use this piece of code to find the right frequency.
      for (f = 433.76f; f < 433.890f; f += 0.0005f) {
        SerialDebug.printf("Test frequency : %.4fMHz\n", f);
        cc1101_init(f);

        struct tmeter_data meter_data;
        meter_data = get_meter_data();

        if (meter_data.reads_counter != 0 || meter_data.liters != 0) {
          if (f_start == 0) {
            f_start = f;
          } 
          SerialDebug.println("------------------------------");
          SerialDebug.printf("Got frequency : %.4f", f);
          SerialDebug.println("------------------------------");
          SerialDebug.printf("Liters : %d\nBattery (in months) : %d\nCounter : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter);
          digitalWrite(LED_BUILTIN, LOW); // turned on
        } else {
          if (f_start!=0) {
            f_end = f;
            break;
          }
        }
      }

      if (f_start || f_end) {
        SerialDebug.printf("\nWorking from %fMHz to %fMhz\n", f_start, f_end);
        f = (f_end - f_start) / 2;
        f += f_start ;
      } else {
        SerialDebug.printf("\nNot found a working Frequency!\n");
        f = FREQUENCY;
      }
    } 

    SerialDebug.printf("Setting to %fMHz\n", f);
    cc1101_init(f);
  }

  // Use this piece of code to test
  //struct tmeter_data meter_data;
  //meter_data = get_meter_data();
  //SerialDebug.printf("\nLiters : %d\nBattery (in months) : %d\nCounter : %d\nTime start : %d\nTime end : %d\n\n", meter_data.liters, meter_data.battery_left, meter_data.reads_counter, meter_data.time_start, meter_data.time_end);

}

void loop()
{
  mqtt.loop(); 
  ArduinoOTA.handle();
}
