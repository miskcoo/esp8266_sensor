# ESP8266 Sensor

This is an ESP8266-based environmental sensor. We use AM2320 as the temperature / humidity sensor, and SGP30 as the CO2 / TVOC sensor.

## Usage


- Install [arduino-cli](https://github.com/arduino/arduino-cli) and the [ESP8266 core](https://arduino.github.io/arduino-cli/0.33/getting-started/#adding-3rd-party-cores).
- Create and edit `credentials.h` to specify the WiFi SSID / password:
```c++
#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H

#define WIFI_SSID      "your WiFi SSID"
#define WIFI_PASSWORD  "your WiFi password"

#endif
```
- (optional) Set the SGP30 CO2 / TVOC baseline in `esp8266_sensor.ino` (see the `TODO` comment in this file). 
- Wire SGP30 to ESP8266: `SDA -> GPIO4 (D1)` and `SCL -> GPIO5 (D2)`
- Wire AM2320 to ESP8266: `SDA -> GPIO12 (D6)` and `SCL -> GPIO14 (D5)`
- Compile and upload the binary
```bash
  arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 . --libraries libs
  arduino-cli upload --port /dev/ttyUSB0 --fqbn esp8266:esp8266:nodemcuv2 .
```
- Query the sensor data via `curl http://your-esp8266-ip`, an example output is
```json
  { 
      "uptime": 3318604, 
      "am2320": { 
          "status": "ok", 
          "temperature": 28.60, 
          "humidity": 65.10, 
          "last_time": 12154 
      }, 
      "sgp30": { 
          "status": "ok", 
          "eCO2": 408, 
          "TVOC": 273, 
          "last_time": 158 
      }, 
      "sgp30_baseline": { 
          "eCO2": 36622, 
          "TVOC": 41061, 
          "last_time": 12152 
      } 
  }
```

## Instantiation

![](https://blog.miskcoo.com/assets/images/esp8266-sensor/esp8266-sensor-example.jpg)
