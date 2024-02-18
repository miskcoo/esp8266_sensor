
#if !defined(ESP8266)
  #error This code is designed to run on ESP8266 and ESP8266-based boards! Please check your Tools->Board setting.
#endif

#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include "ESP8266TimerInterrupt.h"
#include "AM2320.h"
#include "SGP30.h"
#include "credentials.h"

WiFiServer wifiServer(80);

// TODO: CHANGE THE BASELINES FOR YOUR SENSOR
// #define SGP30_BASELINE_CO2  36622
// #define SGP30_BASELINE_TVOC 41061

#define AM2320_SDA   12
#define AM2320_SCL   14
#define SGP30_SDA    4
#define SGP30_SCL    5


// AM2320 
#define AM2320_MEASURE_FREQUENCY   60
AM2320 sensor_am2320;
volatile float am2320_temperature, am2320_humidity;
volatile int am2320_status = 1;
volatile unsigned long am2320_last_time = millis();

// SGP30
SGP30 sensor_sgp30;
volatile int sgp30_status = -1;
volatile uint16_t sgp30_co2, sgp30_tvoc, sgp30_baseline_co2 = 0, sgp30_baseline_tvoc = 0;
volatile unsigned long sgp30_last_time = millis(), sgp30_baseline_last_time = millis();

// Init ESP8266 timer 1
#define TIMER_INTERVAL_MS       1000    // DO NOT change this. SGP30 suggests a measure in every 1 second.
ESP8266Timer ITimer;
volatile uint32_t lastMillis = 0;

// Variable to store the HTTP request
String header;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds 
const long timeoutTime = 1000;



void IRAM_ATTR TimerHandler()
{
    static int count = AM2320_MEASURE_FREQUENCY - 1;

    if (sensor_sgp30.isConnected() && sensor_sgp30.measure(true)) {
        sgp30_last_time = millis();
        sgp30_co2  = sensor_sgp30.getCO2();
        sgp30_tvoc = sensor_sgp30.getTVOC();
        sgp30_status = 0;
    } else {
        if (!sensor_sgp30.isConnected())
            sgp30_status = -1;
        else sgp30_status = sensor_sgp30.lastError();
    }

    if(++count >= AM2320_MEASURE_FREQUENCY) {
        count = 0;

        Wire.begin(AM2320_SDA, AM2320_SCL);
        if (sensor_am2320.measure()) {
            am2320_last_time = millis();
            am2320_status = 0;
            am2320_temperature = sensor_am2320.getTemperature();
            am2320_humidity = sensor_am2320.getHumidity();

            Wire.begin(SGP30_SDA, SGP30_SCL);
            sensor_sgp30.setRelHumidity(am2320_temperature, am2320_humidity);
        } else {
            Wire.begin(AM2320_SDA, AM2320_SCL);
            am2320_status = sensor_am2320.getErrorCode();
        }

        uint16_t sgp30_baseline_co2_local, sgp30_baseline_tvoc_local;
        Wire.begin(SGP30_SDA, SGP30_SCL);
        if (sensor_sgp30.getBaseline(&sgp30_baseline_co2_local, &sgp30_baseline_tvoc_local)) {
            sgp30_baseline_co2 = sgp30_baseline_co2_local;
            sgp30_baseline_tvoc = sgp30_baseline_tvoc_local;
            sgp30_baseline_last_time = millis();
        } 
    }
}

void setup_timer()
{
    Serial.print("\nStarting TimerInterruptTest on ");
    Serial.println(ARDUINO_BOARD);
    Serial.println(ESP8266_TIMER_INTERRUPT_VERSION);
    Serial.print("CPU Frequency = ");
    Serial.print(F_CPU / 1000000);
    Serial.println(" MHz");

    // Interval in microsecs
    if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler)) {
        lastMillis = millis();
        Serial.print("Starting ITimer OK, millis() = ");
        Serial.println(lastMillis);
    } else {
        Serial.println("Can't set ITimer correctly. Select another freq. or interval");
    }
}

void setup_wifi()
{
    Serial.println("Starting Wifi Connection...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());

    wifiServer.begin();
}

void setup()
{
    Serial.begin(115200);

    // AM2320 
    pinMode(AM2320_SDA, INPUT_PULLUP);
    pinMode(AM2320_SCL, INPUT_PULLUP);
    sensor_am2320.begin(AM2320_SDA, AM2320_SCL);

    // SGP30
    pinMode(SGP30_SDA, INPUT_PULLUP);
    pinMode(SGP30_SCL, INPUT_PULLUP);
    sensor_sgp30.begin(SGP30_SDA, SGP30_SCL);
    sensor_sgp30.GenericReset();
#if defined(SGP30_BASELINE_CO2) && defined(SGP30_BASELINE_TVOC)
    sensor_sgp30.setBaseline(SGP30_BASELINE_CO2, SGP30_BASELINE_TVOC);
#endif

    setup_wifi();
    setup_timer();
}

void loop()
{
    /* The following code of HTTP server is modified from
     * https://randomnerdtutorials.com/esp8266-web-server/ 
     */
    WiFiClient client( wifiServer.available() );
    if (client) {                             // If a new client connects,
        Serial.println("New Client.");          // print a message out in the serial port
        String currentLine = "";                // make a String to hold incoming data from the client
        currentTime = millis();
        previousTime = currentTime;
        while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
            currentTime = millis();         
            if (client.available()) {             // if there's bytes to read from the client,
                char c = client.read();             // read a byte, then
                Serial.write(c);                    // print it out the serial monitor
                header += c;
                if (c == '\n') {                    // if the byte is a newline character
                                                    // if the current line is blank, you got two newline characters in a row.
                                                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0) {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type: application/json");
                        client.println("Connection: close");
                        client.println();

                        client.print("{ \"uptime\": ");
                        client.print(millis());
                        client.print(", \"am2320\": ");

                        if (am2320_status == 0) {
                            client.print("{ \"status\": \"ok\", \"temperature\": ");
                            client.print(am2320_temperature);
                            client.print(", \"humidity\": ");
                            client.print(am2320_humidity);
                            client.print(", \"last_time\": ");
                            client.print(millis() - am2320_last_time);
                            client.print(" }");
                        } else {
                            switch (am2320_status) {
                                case 1: client.print("{ \"status\": \"ERR: Sensor is offline.\""); break;
                                case 2: client.print("{ \"status\": \"ERR: CRC validation failed.\""); break;
                            }
                            client.print(", \"last_time\": ");
                            client.print(millis() - am2320_last_time);
                            client.print(" }");
                        }

                        client.print(", \"sgp30\": ");

                        if (sgp30_status == 0) {
                            client.print("{ \"status\": \"ok\", \"eCO2\": ");
                            client.print(sgp30_co2);
                            client.print(", \"TVOC\": ");
                            client.print(sgp30_tvoc);
                            client.print(", \"last_time\": ");
                            client.print(millis() - sgp30_last_time);
                            client.print(" }");
                        } else {
                            client.print("{ \"status\": \"ERR: ");
                            client.print(sgp30_status);
                            client.print(".\", \"last_time\": ");
                            client.print(millis() - sgp30_last_time);
                            client.print(" }");
                        }

                        client.print(", \"sgp30_baseline\": ");
                        client.print("{ \"eCO2\": ");
                        client.print(sgp30_baseline_co2);
                        client.print(", \"TVOC\": ");
                        client.print(sgp30_baseline_tvoc);
                        client.print(", \"last_time\": ");
                        client.print(millis() - sgp30_baseline_last_time);
                        client.print(" }");

                        client.print(" }");

                        Serial.print("Time: ");
                        Serial.print(millis() - currentTime);
                        Serial.println();

                        break;
                    } else { // if you got a newline, then clear currentLine
                        currentLine = "";
                    }
                } else if (c != '\r') {  // if you got anything else but a carriage return character,
                    currentLine += c;      // add it to the end of the currentLine
                }
            }
        }
        // Clear the header variable
        header = "";
        // Close the connection
        client.stop();
        Serial.println("Client disconnected.");
        Serial.println("");
    }
}
