/**
 * Corona_Counter.ino
 * Luke Baldan - V1.0
 * Pieced together from various projects/sources on the internet
 *
 * Designed to work with I2C 16x2 LCD Display
 * 
 */

/* Steps to get SSL fingerprint 
 * openssl s_client -connect covid-19-coronavirus-statistics.p.rapidapi.com:443
 * copy the certificate (from "-----BEGIN CERTIFICATE-----" to "-----END CERTIFICATE-----") and paste into a file (cert.pem)
 * openssl x509 -noout -in ./cert.pem -fingerprint -sha1
 *
 * Alternaitvely:
 * echo "${$(openssl s_client -connect covid-19-coronavirus-statistics.p.rapidapi.com:443 2>/dev/null </dev/null | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' | openssl x509 -noout -fingerprint -sha1)//:/ }"
 */

/* Data:
 *  Source 1 -- Currently not using because memory usage too high
 *    https://covid-19-coronavirus-statistics.p.rapidapi.com
 *    /v1/stats
 *    1d 29 9c d8 2b cd d7 34 fc 3e 13 b0 da a0 94 57 de7d 15 69
 *    http.addHeader("x-rapidapi-host", "covid-19-coronavirus-statistics.p.rapidapi.com");
 *    http.addHeader("x-rapidapi-key", "INSERT KEY HERE");
 *   
 *   Source 2 - Need to pay
 *   https://covid-19-data.p.rapidapi.com
 *   /totals
 *   da 9d c5 b8 b4 c3 1c 1b 0e 10 d1 97 17 e9 9b e9 a9 c3 b1 8a
 *   http.addHeader("x-rapidapi-host", "covid-19-data.p.rapidapi.com");
 *   http.addHeader("x-rapidapi-key", "INSERT KEY HERE");
 *  
 *  Source 3 - Free
 *  wuhan-coronavirus-api.laeyoung.endpoint.ainize.ai
 *  /jhu-edu/brief
 *  /jhu-edu/latest?iso2=AU&onlyCountries=true
 *  5c 47 58 67 ad 88 1f 3d 38 50 be 97 d9 64 d7 3e 17 77 1a fa
 */

#include <Arduino.h>
#include <Streaming.h>

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// | ---------------------- ONLY MODIFY THESE VALUE ---------------------- |

// User defined paramaters
static const long interval = 30000;

// Wifi Info
static const char* ssid = "SSID";
static const char* password = "PASSWORD";
static const char* esp_hostname = "Corona_ESP";

// LCD address and geometry and library initialization
static const byte lcdAddr = 0x3f;  // Address of I2C backpack
static const byte lcdCols = 16;    // Number of character in a row
static const byte lcdRows = 2;     // Number of lines

// API Details
static const char* fingerprint = "";
// You must specify the exact number of headers below and ensure the headers array has the same number of values
static const int numHeaders = 1;
static const String headers[numHeaders] = {};
//static const String headers[numHeaders] = { "x-rapidapi-host", "covid-19-data.p.rapidapi.com", "x-rapidapi-key", "4615065aabmsh789ee2a096f6e4ap1c0e39jsnbaad66e41839" };
static const char* host =  "wuhan-coronavirus-api.laeyoung.endpoint.ainize.ai";
static const int port = 80;
static const String getstringAll = "/jhu-edu/brief";
static const String country = "AU";
static const String getstringCountry = "/jhu-edu/latest?iso2=" + country + "&onlyCountries=true";

// | --------------------------------------------------------------------- |

bool success = true;
LiquidCrystal_I2C lcd(lcdAddr, lcdCols, lcdRows);
bool updateFlag = true;
static long currentMillis;

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);
  lcd.init();

  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("COVID19 Tracker");
  lcd.setCursor(0,1);
  lcd.print("Init Wifi");
   
   Serial.begin(115200);
   //Serial.setDebugOutput(true);

  for(uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  
  checkWifi();
  lcd.setCursor(0,1); 
  lcd.print("Wifi Connected");
  delay(1000);
  lcd.setCursor(0,1); 
  lcd.print(WiFi.localIP());
  delay(3000);
 
  // Setup LCD
  lcd.clear();
  lcd.setCursor(0,0); 
  lcd.print("Waiting for data");
  delay(500);
  //Initial data grab before we rely on loop schedule
  updateLCD(grabData(true, fingerprint, host, getstringAll, port, headers));
  currentMillis = millis();
}

// Main loop
void loop() {
   static byte statCounter = 0;
   if (millis() - currentMillis >= interval)
   {
      if (statCounter> 10)
      {
         statCounter = 0;
      }
      statCounter++;
      Serial.println("Grabbing data");
      
      // Cycle between world and country stats on each update
      if(updateFlag) {
        updateFlag = false;
        updateLCD(grabData(true, fingerprint, host, getstringCountry, port, headers));
      }
      else { 
        updateFlag = true;
        updateLCD(grabData(true, fingerprint, host, getstringAll, port, headers));
      }
   }

   // Calculate time to next update
   int timeUntilUpdate = (interval - (millis() - currentMillis))/1000;

   //Print time to next update
   updateTime(timeUntilUpdate);
}

// Blink onboard LED
void ledBlink(int interval) {
  digitalWrite(BUILTIN_LED, LOW);                                       
  delay(interval);                  
  digitalWrite(BUILTIN_LED, HIGH);  
  delay(interval);                  
}

// Update the remaining time until update
void updateTime(int timeUntilUpdate) {
  lcd.setCursor(13,0);
  if(timeUntilUpdate != 0) {
    lcd.printf("%02ds",timeUntilUpdate);
  }
  else {
    lcd.print("UPD");  
  }
  // Flash onboard ESP LED
  ledBlink(100);
}

void updateLCD(String payload) {
  // Payload empty, update has failed
  if(payload.length()==0) {
    lcd.setCursor(13,0);
    lcd.print("ERR");
  }
  // Update successful
  else {
    lcd.setCursor(13,0);
    Serial.println(payload);
    parseResponseData(payload,country);
  }
  // Reset the update interval;
  currentMillis = millis();
}

// grabData from API
// if retry is false, do not attempt to retry a failed GET request
String grabData(bool retry, const char* fingerprint, const char* host, String requeststring, const int port, const String* headers) {

  // Keep on retrying if we do not get a 200 OK http response code
  do {
    // Wait for WiFi connection
    checkWifi();
    HTTPClient http;
    
    Serial.print("[HTTP] begin...\n");
    Serial.println(host + requeststring);
    //http.begin(host, port, requeststring, true, fingerprint); //HTTPS
    http.begin(host, port, requeststring); //HTTP

    // Add headers
    for(int i=0; i<numHeaders-1; i++) {
      http.addHeader(headers[i], headers[i+1]);  
    }
      
    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    if(httpCode) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // Success
        if(httpCode == 200) {
            success = true;
            return http.getString();
        }
        if(httpCode != 200) {
          Serial.printf("Bad HTTP response: %d\n", httpCode);
          success = false;
          if(retry) {
            // Retry delay
            ledBlink(500);
            delay(30000);
            ledBlink(500);
          }
        }
    } 
    else {
      success = false;
      Serial.print("[HTTP] GET... failed, no connection or no HTTP server\n");
    }
    http.end();
  } while(!success && retry);
  success = false;
  return "";
}

// This function, checks if the WiFi connection is avaialble and attempts reconnection
void checkWifi() {
  while(WiFi.status() != WL_CONNECTED) {
    Serial.println("Waiting for Wifi connection");
    // Re-init Wifi
    WiFi.mode(WIFI_STA);
    WiFi.hostname(esp_hostname);
    WiFi.begin(ssid, password);
    delay(60000);
  }
  //Serial.print("Wifi Reconnected with IP: ");
  //Serial.println(WiFi.localIP());
}

// This method takes the raw JSON response and parses out the relevant data
void parseResponseData(String in, String country) {
  StaticJsonDocument<400> doc;
  
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, in);

  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return;
  }

  // Fetch values.
  // Test to see if we are getting global stats or country stats
  long confirmed, recovered, deaths = 0;

  lcd.clear();
  // Global stats
  long test = doc["confirmed"];
  if(test != 0) {
    Serial.println("Printing global stats");
    confirmed = doc["confirmed"];
    recovered = doc["recovered"];
    deaths = doc["deaths"];
    lcd.setCursor(14,1); 
    lcd.print("  ");
  // Country stats
  } else {
    Serial.println("Printing country stats");
    confirmed = doc[0]["confirmed"];
    recovered = doc[0]["recovered"];
    deaths = doc[0]["deaths"];
    lcd.setCursor(14,1); 
    lcd.print(country.substring(0,2));  
  }
  
  // Print values to LCD, use smart formatting
  lcd.setCursor(0,0); 
  lcd.print("C:"); 
  if (confirmed >= 1000000) {
    float confirmedF = (float)confirmed/(float)1000000;
    lcd.print(confirmedF, 4);
    lcd.print("m");  
  } else if (confirmed >= 1000) {
    float confirmedF = (float)confirmed/(float)1000;
    lcd.print(confirmedF, 2);
    lcd.print("k");
  } else {
    lcd.print(confirmed);
  }
  
  lcd.setCursor(0,1); 
  lcd.print("R:");
  if (recovered >= 1000000) {
    float recoveredF = (float)recovered/(float)1000000;
    lcd.print(recoveredF, 1);
    lcd.print("m");  
  } else if (recovered >= 1000) {
    float recoveredF = (float)recovered/(float)1000;
    lcd.print(recoveredF, 0);
    lcd.print("k");
  } else {
    lcd.print(recovered);
  }

  lcd.print(" D:");
  if (deaths >= 1000000) {
    float deathsF = (float)deaths/(float)1000000;
    lcd.print(deathsF, 1);
    lcd.print("m");  
  } else if (deaths >= 1000) {
    float deathsF = (float)deaths/(float)1000;
    lcd.print(deathsF, 0);
    lcd.print("k");
  } else {
    lcd.print(deaths);
  }
}
