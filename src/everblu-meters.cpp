#include <ArduinoOTA.h>
#include <Preferences.h>
#include <EspMQTTClient.h>
#include <ArduinoJson.h>
#include <string.h>
#include "esp_sntp.h"
#include "timezone.h"
#include "everblu_meters.h"
#include "secrets.h"

EspMQTTClient mqtt;
Preferences preferences;
char hostname[32];
int _retry = 0;
String topic = TOPIC_BASE;
uint32_t next_wake = 0;
struct tmeter_data meter_data;
float f_min, f_max ; // Scan results
uint32_t r_min, r_max ; // Scan results
volatile bool time_synched = false;

// Battery related
uint16_t bat_pc  ;
uint16_t bat_mv  ;
uint16_t bat_dir ;
uint16_t bat_vin ;


void delay_loop(unsigned long _delay) 
{
    while (_delay >= 10) {
        mqtt.loop();
        _delay-=10;
        delay(_delay);
    }
}

void show_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0 : SerialDebug.print("external signal using RTC_IO"); break;
        case ESP_SLEEP_WAKEUP_EXT1 : SerialDebug.print("external signal using RTC_CNTL"); break;
        case ESP_SLEEP_WAKEUP_TIMER : SerialDebug.print("timer"); break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD : SerialDebug.print("touchpad"); break;
        case ESP_SLEEP_WAKEUP_ULP : SerialDebug.print("ULP program"); break;
        default : SerialDebug.printf_P(PSTR("not from sleep:%d"),wakeup_reason); break;
    }
    SerialDebug.print(CRLF);
}

// Return a *rough* estimate of the current battery voltage
uint16_t GetVinVoltage()
{
#ifdef BAT_VOLTAGE    
    static unsigned long nextVoltage = millis() + 1000;
    static float lastMeasuredVoltage;
    uint32_t raw, mv;
    esp_adc_cal_characteristics_t chars;

    // only check voltage every 1 second
    if ( nextVoltage - millis() > 0 ) {
        nextVoltage = millis() + 1000; 

        // grab latest voltage
        analogRead(BAT_VOLTAGE);  // Just to get the ADC setup
        raw = adc1_get_raw(BATT_CHANNEL);  // Read of raw ADC value

        // Get ADC calibration values
        esp_adc_cal_characterize(ADC_UNIT_1,ADC_ATTEN_11db ,ADC_WIDTH_BIT_12, 1100, &chars);

        // Convert to calibrated mv then volts
        mv = esp_adc_cal_raw_to_voltage(raw, &chars) * (LOWER_DIVIDER+UPPER_DIVIDER) / LOWER_DIVIDER;
        // When battery is charging or charged, sometime this value may complely off range (due to reverse charging current I guess)
        if (mv > 4800) {
          mv = 4800;
        }

        //lastMeasuredVoltage = (float)mv / 1000.0;
        lastMeasuredVoltage = (uint16_t) mv;

        //SerialDebug.printf_P(PSTR(" VIN:%dmV\r\n"), mv);

        // Vin > 4.4V looks like we are usb connected
        if (mv >= 4400) {
            // Whaever you like
        } 
    }

    return ( lastMeasuredVoltage );
#else
    return 0;
#endif
}

void deep_sleep(uint32_t seconds)
{
	SerialDebug.print(F("Going to deep sleep mode for"));
	if (seconds) {
		SerialDebug.printf_P(PSTR(" %d seconds" CRLF), seconds);
		esp_sleep_enable_timer_wakeup(  ((uint64_t) seconds) * uS_TO_S_FACTOR);
	} else {
		SerialDebug.print(F("ever" CRLF));
	}
    // Dim down RGB LED
	for (int i = 128 ; i > 0 ; i--) {
		//DotStar_SetBrightness(i);
		//DotStar_SetPixelColor(DOTSTAR_YELLOW, true);
		delay(10);
	}
    DotStar_Clear();

    WiFi.disconnect();
	esp_deep_sleep_start();
}

void printMeterData(struct tmeter_data * data)
{
    SerialDebug.printf_P(PSTR("Consumption   : %d Liters" CRLF), data->liters);
    SerialDebug.printf_P(PSTR("Battery left  : %d Months" CRLF), data->battery_left);
    SerialDebug.printf_P(PSTR("Read counter  : %d times" CRLF), data->reads_counter);
    SerialDebug.printf_P(PSTR("Working hours : from %02dH to %02d" CRLF), data->time_start, data->time_end);
    SerialDebug.printf_P(PSTR("Local Time    : %s" CRLF), getDate());
    SerialDebug.printf_P(PSTR("RSSI  /  LQI  : %ddBm  /  %d" CRLF), data->rssi, data->lqi);
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


// To be reworked for more robust code
char * getDate(time_t rawtime) 
{
    struct tm * timeinfo;
    timeinfo = localtime ( &rawtime );
    char * str = asctime(timeinfo) ;
    // remove ending \n
    str[strlen(str)-1] ='\0';
    return str;
}

// To be reworked for more robust code
char * getDate() 
{
   return getDate( time(nullptr) );
}



void printDate( time_t tnow, tm *ptm)
{
    SerialDebug.printf("Current date (UTC) : %04d/%02d/%02d %02d:%02d:%02d" CRLF, 
                            ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, 
                            ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
                        
    SerialDebug.printf("Current Time       : %s" CRLF, String(tnow, DEC).c_str());
}

char * formatLocalTime() 
{
    static char buffer[80];
    struct tm now;  

    if (!getLocalTime(&now)) {
        strcpy_P(buffer, PSTR("No time available (yet)"));
    } else {
        strftime( buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &now );
    }
    return buffer;
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
    
    time_synched = true;

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
    if (nowinfo.tm_hour>WAKE_HOUR || (nowinfo.tm_hour==WAKE_HOUR && nowinfo.tm_min>=WAKE_MIN) ) {
        tominfo->tm_hour = WAKE_MIN;
        tominfo->tm_min = 0;
        tominfo->tm_sec = 0;
        SerialDebug.print("Next wake tomorrow at ");
        SerialDebug.print(tominfo, "%B %d %Y %H:%M:%S");
        // Number of second to wait for wakeup
        next_wake = mktime(tominfo) - now ;
    } else {
        nowinfo.tm_hour = WAKE_HOUR;
        nowinfo.tm_min = WAKE_MIN;
        nowinfo.tm_sec = 0;
        SerialDebug.print("Next wake today at ");
        SerialDebug.print(&nowinfo, "%B %d %Y %H:%M:%S");
        // Number of second to wait for wakeup
        next_wake = mktime(&nowinfo) - now ;
    }
    SerialDebug.printf_P(PSTR(" (in %d seconds)" CRLF), next_wake);
}

    
void onConnectionEstablished()
{
    SerialDebug.print("***** Connected to MQTT Broker *****" CRLF);

    // set notification call-back function
    configTzTime(TZ_Europe_Paris, "fr.pool.ntp.org", "time.nist.gov");
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb( onNtpSync );

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

void stop_error(uint32_t wakeup) 
{
    // Blink RED 10 time
    for (int i =0; i<10; i++) {
        delay_loop(240);
        DotStar_SetPixelColor(DOTSTAR_RED, true);
        digitalWrite(LED_BUILTIN, LOW); 
        delay_loop(10);
        digitalWrite(LED_BUILTIN, HIGH); 
        DotStar_Clear();
    }
    // Sleep forever or not
    deep_sleep(wakeup);
}

char * frequency_str( float _frequency) 
{
    static char _str[16];
    sprintf(_str, "%.4f", _frequency);
    return _str;
}

// test a read on specified frequency using CC1101 register settings
int test_frequency_register(uint32_t reg)  
{
    char buff[256];

    // keep only 3 bytes
    reg &= 0xFFFFFF;
    float _frequency = (26.0f/65536.0f) * (float) reg ;

    DotStar_SetPixelColor(DOTSTAR_BLUE, true);
    delay(250);
    DotStar_Clear();
    SerialDebug.printf_P(PSTR("Test register : 0x%06X (%.4fMHz) => "), reg, _frequency);
    cc1101_init(0.0f, reg, false);
    // Used for testing my code only by simulation
    //if (_frequency>433.83f && _frequency<433.84f) {
    //    meter_data.ok = true;
    //} else {
    //    meter_data.ok = false;
    //}
    meter_data = get_meter_data();
    // Got datas ?
    if (meter_data.error>0 ) {
        SerialDebug.print("** OK! **");
        // check and adjust working boundaries
        if (_frequency > f_max) {
            f_max = _frequency;
            r_max = reg;
        }
        if (_frequency < f_min) {
            f_min = _frequency;
            r_min = reg;
        }
        DotStar_SetPixelColor(DOTSTAR_GREEN, true);
    } else {
        if (meter_data.error == -1 ) {
            SerialDebug.print("Read error");
        } else if (meter_data.error == -2 ) {
            SerialDebug.print("Read error");
        } else {
            SerialDebug.print("Unknown answer");
        }
        DotStar_SetPixelColor(DOTSTAR_ORANGE, true);
    }

    // Found working boudaries?
    if (r_min<(REG_DEFAULT+REG_SCAN_LOOP) && r_max>(REG_DEFAULT-REG_SCAN_LOOP)) {
        DotStar_SetPixelColor(DOTSTAR_PINK, true);
        SerialDebug.printf_P(PSTR("    %.4f < Works < %.4f "), f_min, f_max);
        SerialDebug.printf_P(PSTR("    RSSI:%ddBm LQI:%d "), meter_data.rssi, meter_data.lqi);
    }
    printf("\n");
    delay(250);
    DotStar_Clear();

    StaticJsonDocument<512> doc;
    String output;
    doc["date"] = formatLocalTime();
    sprintf(buff, "%.4f", _frequency);
    doc["frequency"] = buff;
    doc["timestamp"] = time(nullptr);
    sprintf(buff, "0x%06X", reg);
    doc["register"] = buff;
    doc["result"] = meter_data.error;
    if ( meter_data.error > 0 ) {
        doc["rssi"] = meter_data.rssi ;
        doc["lqi"] = meter_data.lqi;
    }
    serializeJson(doc, output);
    mqtt.publish(topic + "scanning", output, true); 
    delay(50);

    return meter_data.error;
}


void setup()
{
    StaticJsonDocument<512> doc;
    String output;

    SerialDebug.begin(115200);

    // this resets all the neopixels to an off state
    // strip.Begin();
    // strip.Show();
    delay(100); // Needed to initialize
    // DotStar_Clear();
    // DotStar_SetBrightness( MY_RGB_BRIGHTNESS );

    // Wire.begin(I2C_SDA, I2C_SCL);

    // Set hostanme
    uint32_t chipId = 0;
    for (int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    // Set Network Hostname
    //sprintf_P(hostname, PSTR("everblu-cyble-%04x"), chipId & 0xFFFF );
    sprintf_P(hostname, PSTR("cyble-%02d-%07d-esp%04x"), METER_YEAR, METER_SERIAL, chipId & 0xFFFF );
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
    #if defined FORCE_SCAN
    float frequency = preferences.getFloat("frequency", 0.0f);
    #else 
    float frequency = preferences.getFloat("frequency",  433.82000f);
    #endif
    float f_start = 0.0f;
    float f_end = 0.0f;

#ifdef USE_LC709203F
    if (lc_begin()) {
        SerialDebug.printf_P(PSTR("Found LC70920x Version: 0x%04X\r\n"), lc_getICversion() );
        // Set 500mAh Battery
        lc_setPackSize(LC709203F_APA_500MAH);
        //lc_setAlarmVoltage(3.5);
        bat_pc  = lc_cellPercent();
        bat_mv  = lc_cellVoltage();
        bat_dir = lc_getCurrentDirection();
    }
#endif
#ifdef BAT_VOLTAGE    
    bat_vin = GetVinVoltage();
#endif

    SerialDebug.println();
    SerialDebug.println("================================");
    SerialDebug.printf("Device Name   : %s" CRLF, hostname);
    SerialDebug.print ("Wakeup by     : "); show_wakeup_reason();
    SerialDebug.printf("Meter Year    : %02d" CRLF, METER_YEAR);
    SerialDebug.printf("Meter Serial  : %06d" CRLF, METER_SERIAL);
    SerialDebug.printf("SPI Speed     : %.1fMHz" CRLF, ((float)_spi_speed) / 1000.0f / 1000.0f);
    SerialDebug.printf("Frequency     : %.4fMHz" CRLF, frequency);
    SerialDebug.printf("Retries left  : %d/%d" CRLF, RETRIES -  preferences.getShort("retries", RETRIES), RETRIES );
    SerialDebug.printf("Retries delay : %d" CRLF, RETRIES_DELAY );

#ifdef BAT_VOLTAGE    
    SerialDebug.printf("Vin          : %.2fV" CRLF, ((float)bat_vin)/1000.0f );
#endif
#ifdef USE_LC709203F
    SerialDebug.printf("Battery      : %.1f%%  %.2fV  %d" CRLF, ((float) bat_pc)/10.0f, ((float)bat_mv)/1000.0f, bat_dir );
#endif
    SerialDebug.println("===========================");

    if (!cc1101_init(0.0f, REG_DEFAULT, true)) {
        SerialDebug.print("Unable to find CC1101 Chip !!\n");
        // Stop and never retry
        stop_error(0);
    }

    SerialDebug.printf("Found CC1101\n");
    // Blink Green
    DotStar_SetPixelColor(DOTSTAR_GREEN, true);
    // Start network stuff
    mqtt.setMqttClientName(hostname);
    mqtt.setWifiCredentials(WIFI_SSID, WIFI_PASS);
    mqtt.setMqttServer(MQTT_SERVER, MQTT_USER, MQTT_PASS, MQTT_PORT); // default no user/pass and port 1883
    mqtt.setMaxPacketSize(1024);
    // Optional functionalities of EspMQTTClient
    //mqtt.enableDebuggingMessages(); // Enable debugging messages sent to serial output
    mqtt.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overridded with enableHTTPWebUpdater("user", "password").
    mqtt.enableOTA(); // Enable OTA (Over The Air) updates. Password defaults to MQTTPassword. Port is the default OTA port. Can be overridden with enableOTA("password", port).
    //mqtt.enableLastWillMessage(String(topic + "lastwill").c_str(), "offline", true);  
    delay(250);
    DotStar_Clear();

    // Wait for WiFi and MQTT connected 
    int time_out = 0;
    SerialDebug.print(F("Trying to connect WiFi") );
    while (!mqtt.isConnected() ) {
        mqtt.loop();
        delay(450);
        if (mqtt.isWifiConnected() ) {
            SerialDebug.print('*');
            // DotStar_SetPixelColor(DOTSTAR_PINK, true);
        } else {
            SerialDebug.print('.');
            // DotStar_SetPixelColor(DOTSTAR_CYAN, true);
        }
        digitalWrite(LED_BUILTIN, LOW); 
        delay(50);
        digitalWrite(LED_BUILTIN, HIGH); 
        // DotStar_Clear();
        // >60s (500ms loop)
        if (++time_out >= 120) {
            SerialDebug.print(F(CRLF "Unable to connect in 60s" CRLF) );
            // Stop and retry in 1H
            stop_error(3600);
        }
    }

    SerialDebug.printf_P(PSTR(CRLF "Connected in %ds" CRLF), time_out/2);
    SerialDebug.print(F("Waiting for time sync") );

    time_out=0;
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        mqtt.loop();
        delay(225);
        DotStar_SetPixelColor(DOTSTAR_PINK, true);
        digitalWrite(LED_BUILTIN, LOW); 
        delay(25);
        digitalWrite(LED_BUILTIN, HIGH); 
        DotStar_Clear();
        // >60s (250ms loop)
        if (++time_out >= 240) {
            SerialDebug.print(F(CRLF "Unable to sync time in 60s" CRLF) );
            // Stop and retry in 1H
            stop_error(3600);
        }
    }

    SerialDebug.printf_P(PSTR(CRLF "Synced in %ds" CRLF), time_out/4);

    // Scan for correct frequency if not already found one in config
    if ( frequency == 0.0f ) {
        // value for 433.82MHz is REG_DEFAULT => 0x10AF75
        uint32_t reg = REG_DEFAULT;
        uint32_t scanned = reg;
        uint32_t index = 0;
        r_min = reg + REG_SCAN_LOOP;
        r_max = reg - REG_SCAN_LOOP;
        f_min = 450; // Just to be sure out of bounds
        f_max = 400; // Just to be sure out of bounds

        // Step is 26000000 / 2^16 => 0,0004Mhz
        // so REG_SCAN_LOOP=128 => 0.05MHz step
        // so scan is from 433.77 to 433.87
        while ( index <= REG_SCAN_LOOP ) {
            scanned = REG_DEFAULT - index;
            test_frequency_register(scanned);
            // Avoid duplicate on 1st loop
            if ( index > 0 ) {
                scanned = REG_DEFAULT + index;
                test_frequency_register(scanned);
            }
            index++;
        }

        doc.clear();
        output.clear();
        doc["date"] = formatLocalTime();
        doc["ts"] = time(nullptr);
        // Found frequency Range, setup in the middle
        if (r_min<(REG_DEFAULT+REG_SCAN_LOOP) && r_max>(REG_DEFAULT-REG_SCAN_LOOP)) {
            SerialDebug.printf_P(PSTR("Working from %06X to %06X => "), r_min, r_max);
            reg = r_min + ((r_max - r_min) / 2);
            frequency = (26.0f/65536.0f) * (float) reg;
            f_min = (26.0f/65536.0f) * (float) r_min;
            f_max = (26.0f/65536.0f) * (float) r_max;
            SerialDebug.printf_P(PSTR("%.4f to %.4f" CRLF), f_min, f_max);
            SerialDebug.printf_P(PSTR( "Please use %.4f as frequency" CRLF), frequency);
            doc["min"] = frequency_str(f_min);
            doc["max"] = frequency_str(f_max);
        } else {
            SerialDebug.print( "No working frequency found!" CRLF);
            frequency = 0.0f;
        }
        doc["frequency"] = frequency_str(frequency);
        serializeJson(doc, output);
        mqtt.publish(topic + "scan/", output, true); 
        delay(50);
    }

    // Even after scan no valid frequency?
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

    SerialDebug.printf_P(PSTR("Setting to %f.4fMHz" CRLF), frequency);
    cc1101_init(frequency, 0);
}

void loop()
{   
    StaticJsonDocument<512> doc;
    String output;
    char buff[32];
    int16_t retries = preferences.getShort("retries", RETRIES);

    if (retries) {
        SerialDebug.printf_P(PSTR("Trying Reading #%d out of %d from meter" CRLF), retries, RETRIES );
    } else {
        SerialDebug.print("Reading data from meter" CRLF);
    }

    // Read meter data
    meter_data = get_meter_data();

    doc["ts"] = time(nullptr);
    doc["date"] = getDate();
    doc["esp_battery"]["percent"] = bat_pc/10;
    doc["esp_battery"]["vbat"] = bat_mv;

    if (meter_data.error>0) {
        DotStar_SetPixelColor(DOTSTAR_GREEN, true);
        printMeterData(&meter_data);
        // Read successfull, clean up retries
        if (retries) {
            preferences.putShort("retries", 0);
        }
        doc["liters"]  = meter_data.liters;
        doc["battery"] = meter_data.battery_left;
        doc["read"] = meter_data.reads_counter;
        doc["rssi"] = meter_data.rssi;
        doc["lqi"] = meter_data.lqi;
        sprintf_P(buff, PSTR("%02d:%02d"), meter_data.time_start, meter_data.time_end);
        doc["hours"] = buff;

        serializeJson(doc, output);
        mqtt.publish(topic + "json", output, true); // timestamp since epoch in UTC
        delay_loop(100); // Do not remove
        DotStar_Clear();

        // If you need specific individual values (not json object) please selec
        // the one you need below
        #ifdef PUBLISH_RAW
        mqtt.publish(topic + "liters", String(meter_data.liters, DEC), true);
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "read", String(meter_data.reads_counter, DEC), true);
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "battery", String(meter_data.battery_left, DEC), true);
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "ts", String(time(nullptr)), true); // timestamp since epoch in UTC
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "date", getDate(), true); 
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "hours", buff, true); 
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "rssi",  String(meter_data.rssi, DEC), true); 
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "lqi",  String(meter_data.lqi, DEC), true); 
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "esp_bat_pc",  String(bat_pc, DEC), true); 
        delay_loop(50); // Do not remove
        mqtt.publish(topic + "esp_bat_mv",  String(bat_mv, DEC), true); 
        delay_loop(50); // Do not remove
        #endif


    } else {

        DotStar_SetPixelColor(DOTSTAR_RED, true);
        SerialDebug.print("No data, are you in business hours?" CRLF);
        doc["type"]  = "No Data";
        doc["retries"]  = retries;
        serializeJson(doc, output);
        mqtt.publish(topic + "error", output, false); // timestamp since epoch in UTC
        delay_loop(100);
        DotStar_Clear();

        if (++retries <= RETRIES) {
            SerialDebug.printf_P(PSTR("%d retries left out of %d" CRLF), RETRIES+1 - retries, RETRIES );
            // Force next wake in programmed delay 
            next_wake = RETRIES_DELAY;
        } else {
            SerialDebug.printf_P(PSTR("No more retries left, next try on scheduled time " CRLF) );
            retries = 0;
        }
        preferences.putShort("retries", retries);

    }
    delay_loop(250);

    // publish next wake informations
    doc.clear();
    output.clear();
    time_t rawtime = time(nullptr);
    rawtime += next_wake;
    doc["seconds"] = next_wake;
    doc["ts"] = rawtime;
    doc["date"] = getDate(rawtime);
    serializeJson(doc, output);
    mqtt.publish(topic + "sleep_until", output, true); 

    // put CC1101 to sleep mode
    cc1101_sleep();

    // wait with some random so all devices don't wake exactlty same time.
    delay_loop( random(5, 15) );

    // sleep until next time
    deep_sleep(next_wake);
}
