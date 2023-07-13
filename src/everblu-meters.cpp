#include <ArduinoOTA.h>
#include <Preferences.h>
#include <EspMQTTClient.h>
#include <ArduinoJson.h>
#include "esp_sntp.h"
#include "timezone.h"
#include "everblu_meters.h"


EspMQTTClient mqtt;
Preferences preferences;
char hostname[32];
int _retry = 0;
String topic = TOPIC_BASE;
uint32_t next_wake = 0;



void printMeterData(struct tmeter_data * data)
{
    SerialDebug.printf_P(PSTR("Liters  : %d" CRLF), data->liters);
    SerialDebug.printf_P(PSTR("Battery : %d months" CRLF), data->battery_left);
    SerialDebug.printf_P(PSTR("Counter : %d" CRLF), data->reads_counter);
}

void printFormatDate( time_t tnow, tm *ptm, char * iso8601)
{
    SerialDebug.printf("Current date (UTC) : %04d/%02d/%02d %02d:%02d:%02d" CRLF, 
                            ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, 
                            ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
                        
    SerialDebug.printf("Current Time       : %s" CRLF, String(tnow, DEC).c_str());

    if (iso8601) {
        strftime(iso8601, sizeof(iso8601), "%FT%TZ", gmtime(&tnow));
        //strftime(iso8601, sizeof(iso8601), "%FT%TZ", ptm);
        SerialDebug.printf("Current ISO8601    : %s" CRLF, iso8601);
    }
}

bool UpdateData()
{
    struct tmeter_data meter_data;
    meter_data = get_meter_data();

    time_t tnow = time(nullptr);
    struct tm *ptm = gmtime(&tnow);
    char iso8601[128] = "";
    printFormatDate(tnow, ptm, iso8601);

    if (meter_data.reads_counter == 0 || meter_data.liters == 0) {
        return false;
    }

    printMeterData(&meter_data);

    StaticJsonDocument<256> doc;
    String output;
    digitalWrite(LED_BUILTIN, LOW); // turned on
    doc["liters"]  = meter_data.liters;
    doc["counter"] = meter_data.reads_counter;
    doc["battery"] = meter_data.battery_left;
    doc["timestamp"] = iso8601;
    serializeJson(doc, output);

    // send all data as a json message
    mqtt.publish(topic + "json", output, true); 
    delay(50); // Do not remove

    mqtt.publish(topic + "liters", String(meter_data.liters, DEC), true);
    delay(50); // Do not remove
    mqtt.publish(topic + "counter", String(meter_data.reads_counter, DEC), true);
    delay(50); // Do not remove
    mqtt.publish(topic + "battery", String(meter_data.battery_left, DEC), true);
    delay(50); // Do not remove
    mqtt.publish(topic + "timestamp", iso8601, true); // timestamp since epoch in UTC
    delay(50); // Do not remove

    return true;
}


void printLocalTime()
{
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("No time available (yet)");
    return;
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  Serial.println(&timeinfo, "%B %d %Y %H:%M:%S");
}

void onNtpSync(struct timeval *t) 
{
    struct tm nowinfo;  // Now (today)
    struct tm *tominfo; // Tomorrow
    time_t now, tom, wake;

    SerialDebug.print(CRLF "***** Got time adjustment from NTP *****" CRLF);
    if (!getLocalTime(&nowinfo)) {
        SerialDebug.println("No time available (yet)");
        return;
    }
    //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    
    // Get current timestamp in UTC
    now = time(nullptr);
    // Add 24H (always in UTC)
    tom = now + 3600 * 24;
    //SerialDebug.println(now);
    //SerialDebug.println(tom);
    // Get local time of tomorrow (so 24H up today localtime)
    tominfo = localtime( &tom );
    //SerialDebug.println(&nowinfo, "%B %d %Y %H:%M:%S");
    //SerialDebug.println( tominfo, "%B %d %Y %H:%M:%S");

    // We will calculate delay for next measure reading 
    // We passed programmed time for today schedule for tomorrow ?
    if (nowinfo.tm_hour > WAKE_HOUR) {
        tominfo->tm_hour = WAKE_HOUR;
        tominfo->tm_min = 0;
        tominfo->tm_sec = 0;
        SerialDebug.print("Next wake tomorrow at ");
        SerialDebug.print(tominfo, "%B %d %Y %H:%M:%S");
        // Number of second to wait for wakeup
        next_wake = mktime(tominfo) - now ;
    } else {
        nowinfo.tm_hour = WAKE_HOUR;
        nowinfo.tm_min = 0;
        nowinfo.tm_sec = 0;
        SerialDebug.print("Next wake today at ");
        SerialDebug.print(&nowinfo, "%B %d %Y %H:%M:%S");
        // Number of second to wait for wakeup
        next_wake = mktime(&nowinfo) - now ;
    }
    SerialDebug.printf_P(PSTR(" (in %d seconds)" CRLF), next_wake);
}

void onWiFiConnectionEstablished() 
{
    SerialDebug.print("***** Connected to WiFi *****" CRLF);
    
    // set notification call-back function
    configTzTime(TZ_Europe_Paris, "fr.pool.ntp.org", "time.nist.gov");
    sntp_set_time_sync_notification_cb( onNtpSync );
    delay(1000); 

    ArduinoOTA.onStart([]() {
        SerialDebug.print("Start updating ");
        if (ArduinoOTA.getCommand() == U_FLASH) {
            SerialDebug.print("sketch" CRLF);
        } else { // U_FS
            SerialDebug.print("filesystem" CRLF);
        }
        // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    });
    
    ArduinoOTA.onEnd([]() {
        SerialDebug.print(CRLF "End updating." CRLF);
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        SerialDebug.printf_P(PSTR("%u%%" CRLF), (progress / (total / 100)));
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        SerialDebug.printf_P(PSTR("Error[%u]: "), error);
        if (error == OTA_AUTH_ERROR) {
            SerialDebug.print("Auth Failed" CRLF);
        } else if (error == OTA_BEGIN_ERROR) {
            SerialDebug.print("Begin Failed" CRLF);
        } else if (error == OTA_CONNECT_ERROR) {
            SerialDebug.print("Connect Failed" CRLF);
        } else if (error == OTA_RECEIVE_ERROR) {
            SerialDebug.print("Receive Failed" CRLF);
        } else if (error == OTA_END_ERROR) {
            SerialDebug.print("End Failed" CRLF);
        }
    });

    ArduinoOTA.setHostname(hostname);
    ArduinoOTA.begin();

}

void onConnectionEstablished()
{
    SerialDebug.print("***** Connected to MQTT Broker *****" CRLF);
    //delay(1000);
    /*
    mqtt.subscribe(topic + "trigger", [](const String& message) {
        if (message.length() > 0) {
            SerialDebug.print("Update data from meter from MQTT trigger" CRLF);
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
    */
}

void stop_error(uint32_t wakeup) 
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
    // Sleep forever or not
    deep_sleep(wakeup);
}

void setup()
{
    StaticJsonDocument<512> doc;
    String output;

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
    sprintf_P(hostname, PSTR("cyble-%02d-%07d"), METER_YEAR, METER_SERIAL );
    topic += String(hostname) + "/";

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
    SerialDebug.printf("Device Name  : %s" CRLF, hostname);
    SerialDebug.print ("Wakeup by    : "); show_wakeup_reason();
    SerialDebug.printf("Meter Year   : %02d" CRLF, METER_YEAR);
    SerialDebug.printf("Meter Serial : %06d" CRLF, METER_SERIAL);
    SerialDebug.printf("SPI Speed    : %.1fMHz" CRLF, ((float)_spi_speed) / 1000.0f / 1000.0f);
    SerialDebug.printf("Frequency    : %.4fMHz" CRLF, frequency);
    SerialDebug.println("===========================");

    if (!cc1101_init(433.825f,true)) {
        SerialDebug.print("Unable to find CC1101 Chip !!\n");
        // Stop and never retry
        stop_error(0);
    }

    SerialDebug.printf("Found CC1101\n");
    // Blink Green
    DotStar_SetPixelColor(DOTSTAR_GREEN, true);
    delay(500);
    DotStar_Clear();

    // Start network stuff
    mqtt.setMqttClientName(hostname);
    mqtt.setWifiCredentials(WIFI_SSID, WIFI_PASS);
    mqtt.setMqttServer(MQTT_SERVER); // default no user/pass and port 1883
    mqtt.setMaxPacketSize(1024);
    // Optional functionalities of EspMQTTClient
    //mqtt.enableDebuggingMessages(); // Enable debugging messages sent to serial output
    mqtt.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
    mqtt.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
    //mqtt.enableLastWillMessage(String(topic + "lastwill").c_str(), "offline", true);  

    // Wait for WiFi and MQTT connected for 30s 
    int time_out = 0;
    bool wifi_init = false;
    SerialDebug.print(F("Trying to connect WiFi") );
    // 30s loop
    while (!mqtt.isConnected() && time_out++<60 ) {
        mqtt.loop();
        delay(450);
        if (mqtt.isWifiConnected()) {
            if (wifi_init == false) {
                wifi_init = true;
                onWiFiConnectionEstablished();
            }
            SerialDebug.print('*');
            DotStar_SetPixelColor(DOTSTAR_PINK, true);
        }else {
            SerialDebug.print('.');
            DotStar_SetPixelColor(DOTSTAR_CYAN, true);
        }
        digitalWrite(LED_BUILTIN, LOW); 
        mqtt.loop();
        delay(50);
        digitalWrite(LED_BUILTIN, HIGH); 
        DotStar_Clear();
    }

    if (!mqtt.isConnected()) {
        SerialDebug.printf_P(PSTR(CRLF "Unable to connect in %ds" CRLF), time_out/2);
        // Stop and retry in 1H
        stop_error(3600);
    }

    SerialDebug.printf_P(PSTR(CRLF "Connected in %ds" CRLF), time_out);

    // Scan for correct frequency if not already found one in config
    if ( frequency == 0.0f ) {
        struct tmeter_data meter_data;
        
        // Scan al frequencies and blink Blue Led
        for (frequency = 433.76f ; frequency < 433.890f ; frequency += 0.0005f) {
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
                printMeterData(&meter_data);
                digitalWrite(LED_BUILTIN, LOW); // turned on
            } else {
                SerialDebug.print("No answer" CRLF);
                if (f_start!=0.0f) {
                    f_end = frequency - 0.0005f ;
                    break;
                }
            }
        }

        // Found frequency Range, setup in the middle
        if (f_start || f_end) {
            SerialDebug.printf_P(PSTR(CRLF "Working from %.4fMHz to %.4fMhz" CRLF), f_start, f_end);
            frequency = (f_end - f_start) / 2;
            frequency += f_start ;
            // Save in NVS
            preferences.putFloat("frequency", frequency);
        } else {
            SerialDebug.print(CRLF "Not found a working Frequency!" CRLF);
            frequency = 0.0f;
        }
    } 

    if (frequency == 0.0f) {
        SerialDebug.print("Nothing more to do" CRLF);
        // Blink RED 
        DotStar_SetPixelColor(DOTSTAR_RED, true);
        delay(500);
        DotStar_Clear();
        // put CC1101 to sleep mode
        cc1101_sleep();
        // Retry scan in 2H
        deep_sleep(3600 * 2);
    }

/*
    char buffer[64];

    mqtt.publish(topic + "scan", String(meter_data.liters, DEC), true);


    sprintf_P(topic, PSTR("everblu/%s/%s"), hostname, "frequencies");
    sprintf_P(buffer, PSTR("%.4f;%.4f;%.4f"), f_start, frequency, f_end);
    mqtt.publish(topic, buffer, true);
    delay(50);
    sprintf_P(topic, PSTR("everblu/%s/%s"), hostname, "frequency");
    sprintf_P(buffer, PSTR("%.4f"), frequency);
    mqtt.publish(topic, buffer, true);
*/
    SerialDebug.printf_P(PSTR("Setting to %fMHz" CRLF), frequency);
    DotStar_SetPixelColor(DOTSTAR_GREEN, true);
    delay(500);
    DotStar_Clear();
    cc1101_init(frequency);
}

void loop()
{
    int16_t retries = preferences.getShort("retries", 0);

    if (retries) {
        SerialDebug.printf_P(PSTR("Trying Reading #%d ou of 10 from meter" CRLF), retries );
    } else {
        SerialDebug.print("Reading data from meter" CRLF);
    }

    if (UpdateData()) {
        // Ok clear retries counter
        if (retries) {
            preferences.putShort("retries", 0);
        }
    } else {
        SerialDebug.print("Unable to retrieve data from meter" CRLF);
        SerialDebug.printf_P(PSTR("%d retries left " CRLF), 10 - retries );
        preferences.putShort("retries", ++retries);
        // Force next wake in 5min
        next_wake = 300;
    }

    // put CC1101 to sleep mode
    cc1101_sleep();
    // do nothing for now
    deep_sleep(next_wake);
}
