#include <ArduinoOTA.h>
#include <Preferences.h>
#include "everblu_meters.h"

// Project source : 
// http://www.lamaisonsimon.fr/wiki/doku.php?id=maison2:compteur_d_eau:compteur_d_eau

// Require EspMQTTClient library (by Patrick Lapointe) version 1.13.3
// Install from Arduino library manager (and its dependancies)
// https://github.com/plapointe6/EspMQTTClient/releases/tag/1.13.3
#include "EspMQTTClient.h"

/*
EspMQTTClient mqtt(
  WIFI_SSID,            // Your Wifi SSID
  WIFI_PASS,          // Your WiFi key
  "mqtt.server.com",    // MQTT Broker server ip
  "MQTTUsername",       // Can be omitted if not needed
  "MQTTPassword",       // Can be omitted if not needed
  "EverblueCyble",      // Client name that uniquely identify your device
  1883                  // MQTT Broker server port
);
*/

EspMQTTClient mqtt;

const char *jsonTemplate = "{ \"liters\":%d, \"counter\":%d, \"battery\":%d, \"timestamp\":\"%s\" }";

int _retry = 0;

Preferences preferences;
char hostname[32];

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
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();

  mqtt.subscribe("everblu/cyble/trigger", [](const String& message) {
    if (message.length() > 0) {
      SerialDebug.println("Update data from meter from MQTT trigger");
      _retry = 0;
      onUpdateData();
    }
  });

/*
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
*/

  onScheduled();
}



void stop_error() 
{
   // Blink RED 10 time
  for (int i =0; i<10; i++) {
    delay(240);
    DotStar_SetPixelColor(DOTSTAR_RED, true);
    digitalWrite(LED_BUILTIN, LOW); 
    delay(10);
    digitalWrite(LED_BUILTIN, HIGH); 
    DotStar_Clear();
  }
  // Sleep forever
  deep_sleep(0);
}

void setup()
{
  SerialDebug.begin(115200);

  // this resets all the neopixels to an off state
  strip.Begin();
  strip.Show();
  delay(100); // Needed to initialize
  DotStar_Clear();
	DotStar_SetBrightness( MY_RGB_BRIGHTNESS );    

  // Set hostanme
  uint32_t chipId = 0;
  for (int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  // Set Network Hostname
  //sprintf_P(hostname, PSTR("everblu-cyble-%04x"), chipId & 0xFFFF );
  sprintf_P(hostname, PSTR("everblu-cyble-%02d-%07d"), METER_YEAR, METER_SERIAL );

  // Wait for serial to be up in 2s
  while (millis()<2000) {
    delay(200);
    DotStar_SetPixelColor(DOTSTAR_YELLOW, true);
    digitalWrite(LED_BUILTIN, LOW); 
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH); 
    DotStar_Clear();
  }

  // Open Preferences, Namespace name is limited to 15 chars.
  preferences.begin("everblu-cyble", false);
  // Remove all preferences under the opened namespace
  //preferences.clear();
  // Or remove the frequency key only
  //preferences.remove("frequency");

  // Get the frequency value, if the key does not exist, return a default value of 0
  float frequency = preferences.getFloat("frequency", 0.0f);
  float f_start = 0.0f;
  float f_end = 0.0f;

  SerialDebug.println();
  SerialDebug.println("===========================");
  SerialDebug.println(hostname);
  SerialDebug.print ("Wakeup by    : "); show_wakeup_reason();
  SerialDebug.printf("Meter Year   : %02d\n", METER_YEAR);
  SerialDebug.printf("Meter Serial : %06d\n", METER_SERIAL);
  SerialDebug.printf("SPI Speed    : %.1fMHz\n", ((float)_spi_speed) / 1000.0f / 1000.0f);
  SerialDebug.printf("Frequency    : %.4fMHz\n", frequency);
  SerialDebug.println("===========================");

  if (!cc1101_init(433.825f,true)) {
    SerialDebug.print("Unable to find CC1101 Chip !!\n");
    stop_error();
  }

  SerialDebug.printf("Found CC1101\n");
  DotStar_SetPixelColor(DOTSTAR_GREEN, true);

  // Start network stuff
  mqtt.setMqttClientName(hostname);
  mqtt.setWifiCredentials(WIFI_SSID, WIFI_PASS);
  //mqtt.setWifiCredentials("CH2I-HOTSPOT", "Wireless@Ch2i");
  //mqtt.setMqttServer(MQTT_SERVER, MQTT_USER, MQTT_PASS, MQTT_PORT); 
  mqtt.setMqttServer(MQTT_SERVER); // default no user/pass and port 1883
  //mqtt.setMqttServer("192.168.1.8"); // default no user/pass and port 1883
  mqtt.setMaxPacketSize(1024);
  // Optional functionalities of EspMQTTClient
  mqtt.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  mqtt.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
  mqtt.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
  //mqtt.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true

  // Wait for WiFi and MQTT connected for 30s since start
  int time_out = 30;
  while (!mqtt.isConnected() && time_out-- ) {
    mqtt.loop();
    delay(900);
    DotStar_SetPixelColor(DOTSTAR_MAGENTA, true);
    digitalWrite(LED_BUILTIN, LOW); 
    mqtt.loop();
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH); 
    DotStar_Clear();
  }

  if (!mqtt.isConnected()) {
    stop_error();
  }

  SerialDebug.printf_P(PSTR("Connected WiFi MQTT in %ds\r\n"), 30-time_out);

  // Scan for correct frequency
  if ( frequency == 0.0f ) {
    struct tmeter_data meter_data;
    // Use this piece of code to find the right frequency.
    for (frequency = 433.76f; frequency < 433.890f; frequency += 0.0005f) {
      mqtt.loop();
      SerialDebug.printf_P(PSTR("Test frequency : %.4fMHz"), frequency);
      DotStar_SetPixelColor(DOTSTAR_BLUE, true);
      delay(250);
      DotStar_Clear();
      cc1101_init(frequency);
      meter_data = get_meter_data();
      SerialDebug.print(F(" => "));
      // Got datas ?
      if (meter_data.reads_counter != 0 || meter_data.liters != 0) {
        // First working frequency, save it
        if (f_start == 0.0f) {
          f_start = frequency;
        } 
        SerialDebug.printf_P(PSTR("Liters:%d  BatMonth:%d  Counter:%d"), meter_data.liters, meter_data.battery_left, meter_data.reads_counter);
        digitalWrite(LED_BUILTIN, LOW); // turned on
      } else {
        SerialDebug.print("No answer");
        if (f_start!=0.0f) {
          f_end = frequency - 0.0005f ;
          break;
        }
      }
      SerialDebug.println();
    }

    // Found frequency Range, setup in the middle
    if (f_start || f_end) {
      SerialDebug.printf_P(PSTR("\nWorking from %.4fMHz to %.4fMhz\n"), f_start, f_end);
      frequency = (f_end - f_start) / 2;
      frequency += f_start ;
      // Save in NVS
      preferences.putFloat("frequency", frequency);
    } else {
      SerialDebug.println("\nNot found a working Frequency!");
      frequency = 0.0f;
    }
  } 

  if (frequency == 0.0f) {
    SerialDebug.println("Nothing more to do");
    // Blink RED 
    DotStar_SetPixelColor(DOTSTAR_RED, true);
    delay(500);
    DotStar_Clear();
    // put CC1101 to sleep mode
    cc1101_sleep();
    // Retry scan in 2H
    deep_sleep(3600 * 2);
  }

  char buffer[64];
  char topic[64];

  sprintf_P(topic, PSTR("everblu/%s/%s"), hostname, "frequencies");
  sprintf_P(buffer, PSTR("%.4f;%.4f;%.4f"), f_start, frequency, f_end);
  mqtt.publish(topic, buffer, true);
  delay(50);
  sprintf_P(topic, PSTR("everblu/%s/%s"), hostname, "frequency");
  sprintf_P(buffer, PSTR("%.4f"), frequency);
  mqtt.publish(topic, buffer, true);

  SerialDebug.printf_P(PSTR("Setting to %fMHz\n"), frequency);
  DotStar_SetPixelColor(DOTSTAR_GREEN, true);
  delay(500);
  DotStar_Clear();
  cc1101_init(frequency);

  // put CC1101 to sleep mode
  cc1101_sleep();
  // do nothing for now
  deep_sleep(0);

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
