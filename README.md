# everblu-meters-esp32 - Water usage data for MQTT

<!--
![With CC1101 Shield on PI Zero](pictures/cc1101-pi-zero.jpg)
-->

Fetch water/gas usage data from Cyble EverBlu meters using RADIAN protocol on 433Mhz. Integrated with MQTT. 

Meters supported:
- Itron EverBlu Cyble Enhanced

Software original code (but also all the hard work to get thingd working was originaly done [here][4] then put on github by @neutrinus [here][5] and then forked by [psykokwak](https://github.com/psykokwak-com/everblu-meters-esp8266)

I added some changes to the original firmware to work with my custom esp32-c3 board (including RGB led control, battery monitoring, deepsleep, battery charger), just plug, and play.

## Hardware

The project runs on ESP32 with an RF transreciver (CC1101). 

Hardware can be any ESP32+CC1101 with correct wiring/

I used my open source one [cc1101-e07-pi](https://github.com/hallard/cc1101-e07-pi), you can check it out for the already made hardware or look at the original [repo][5] if you want to build your own.

<!--
![With CC1101 Custom Mini Shield](pictures/cc1101-pi-antennas.jpg)
-->

## Configuration

All configuration is done thru `platformio.ini` file

TBD

## MQTT Topics and data

### Base Topic

Base topic is `everblu` followed by serial number (with leading 0 this time) followed by `-espabcd`, because I've also device with PI so I need to know who is sending data, of course you can remove it if needed.

```
everblu/cyble-22-0828979-espbf84
```

### Reading OK

When read suceeded, results are sent to the base topic plus `json`

Example received on `everblu/cyble-22-0828979-espbf84/json`

the format is `JSON`

TBD

```json
{
    "ts":1689626877,
    "date":"Mon Jul 17 22:47:57 2023",
    "esp_battery":{
        "percent":90,
        "vbat":4094,
        "vin":4800,
        "dir":0}
}
```

Values can also be sent in raw format enabling setting `PUBLISH_RAW` in `platformio.ini`

```ini
    -D PUBLISH_RAW  ; Enable to publish also RAW values (keeping JSON)
```

- `everblu/cyble-22-0828979-espbf84/liters`
- `everblu/cyble-22-0828979-espbf84/battery`
- `everblu/cyble-22-0828979-espbf84/read`
- `everblu/cyble-22-0828979-espbf84/date`
- `everblu/cyble-22-0828979-espbf84/ts`
- `everblu/cyble-22-0828979-espbf84/hours`
- `everblu/cyble-22-0828979-espbf84/rssi`
- `everblu/cyble-22-0828979-espbf84/lqi`


### Read failed

When read failed, results are sent in `JSON` tExample received on `everblu/cyble-22-0828979-espbf84/error`
```json
{
    "ts":1689626877,
    "date":"Mon Jul 17 22:47:57 2023",
    "esp_battery":{
        "percent":90,
        "vbat":4094,
        "vin":4800,
        "dir":0},
    "type":"No Data",
    "retries":5
}
```

When read failed device will retry 5 times (5min in between), this is the retrie field. After 5 tries, it cancel and set wake up to programmed hour `WAKE_HOUR` of `platformio.ini`


### Scanning

When scanning, results are sent in real time to the base topic plus `scanning`

Example received on `everblu/cyble-22-0828979-espbf84/scanning`

the format is `JSON`

```json
{ 
	"date": "Sat Jul 15 12:27:05 2023", 
	"frequency": "433.8160", 
	"result":0 
}
```

`result` is `-1` if data can't be read, `-2` if read but saw an error else it's #reads of the counter (so positive)

### End of Scan

When scanning done results are sent to the base topic plus `scan`

Example received on `everblu/cyble-22-0828979-espbf84/scan`

If `frequency` is `0` this mean no working frequency has been found.


```json
{ 
	"date":"Sat Jul 15 13:06:04 2023", 
	"frequency":"0.0000",
}
```

If `frequency` is found it will be the center frequency of the working frequencies boundaries. Boundaries will be sent also.

```json
{ 
	"date":"Sat Jul 15 13:06:04 2023", 
	"frequency":"433.8000", 
	"min":"433.7900", 
	"max":"433.8100"
}
```

### Next Wake

Before going into deep sleep until next try or programmed hour `WAKE_HOUR` of `platformio.ini` the device will send a `JSON` message to topic `sleep_until` 

Example received on `everblu/cyble-22-0828979-espbf84/sleep_until`

```json
{
    "seconds": 29526,
    "ts": 1689656406,
    "date": "Tue Jul 18 07:00:06 2023"
}
```



## Troubleshooting

### Frequency adjustment

Your transreciver module may be not calibrated correctly, please find working frequency enabling a scan by setting `FORCE_SCAN` in `platformio.ini`

```ini
    -D FORCE_SCAN ; // Force a frequency scan on boot
```

Once found device will save frequency and use it on each boot.

### Business hours

Your meter may be configured in such a way that is listens for request only during hours when data collectors work - to conserve energy. 
If you are unable to communicate with the meter, please try again during business hours (8-16).

It seems also that you can't read it on Saturday/Sunday but works on off day such as July 14th.

Mine are 06H to 18H, don't know if they are UTC or local time with daligh saving.

### Serial number starting with 0

Please ignore the leading 0, provide serial in configuration without it.

### Save power

The meter has internal battery, which should last for 10 years when queried once a day. 

As basic advice, **Please do not query your meter more than once a day**

According Water manager here they need to change about 10/15 on each measure session, my previous one from 2017 was not working anymore, now they came and put a new one.

## Origin and license

This code is based on code from [lamaisonsimon][4]


The license is unknown, citing one of the authors (fred):

> I didn't put a license on this code maybe I should, I didn't know much about it in terms of licensing.
> this code was made by "looking" at the radian protocol which is said to be open source earlier in the page, I don't know if that helps?

# Links

There is a very nice port to ESP8266/ESP32: https://github.com/psykokwak-com/everblu-meters-esp8266

# Origin and License

Software original code (but also all the hard work to get thingd working was originaly done [here][4] then put on github by @neutrinus [here][5].

The license is unknown, citing one of the authors (fred):

> I didn't put a license on this code maybe I should, I didn't know much about it in terms of licensing.
> this code was made by "looking" at the radian protocol which is said to be open source earlier in the page, I don't know if that helps?

## Misc

See news and other projects on my [blog][2] 

[1]: https://www.cdebyte.com/products/E07-M1101S
[2]: https://hallard.me
[3]: https://oshpark.com/shared_projects/BVwV2j3b
[4]: http://www.lamaisonsimon.fr/wiki/doku.php?id=maison2:compteur_d_eau:compteur_d_eau
[5]: https://github.com/neutrinus/everblu-meters
[6]: https://github.com/hallard/everblu-meters-pi
[7]: https://github.com/hallard/cc1101-e07-pi


