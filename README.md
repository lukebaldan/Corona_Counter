# Corona Counter

This is a simple project based on an ESP8266 and 16x2 LCD display to show COVID19 statistics.
The display cycles between globval stats and country stats, updating by default every 30 seconds.
The sketch has been designed to easily interwork with other API's.

## Prequisites

The following libraries are needed and made available to the Arduino IDE:

```
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <LiquidCrystal_I2C.h>
```

## Usage

Modify the variables as required in the section labelled: 'ONLY MODIFY THESE VALUE'
Key parameters include:

```
ssid - Wifi SSID
password - Wifi Password
country - Use 2 digit country code for specific stats on country
```
Use the arduino IDE to upload the code to your ESP8266 module of choice. All testing was done on a Wemos D1 Mini.
Your I2C LCD display should be connected to the appropriate SDA and SCL pins on the ESP module.

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

## License
N/A
