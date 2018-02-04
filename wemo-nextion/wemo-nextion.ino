#include <Arduino.h>
#include <ArduinoJson.h>

#include <Nextion.h>
#include <NextionText.h>
#include <NextionDualStateButton.h>
#include <SoftwareSerial.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include "private.h"

ESP8266WiFiMulti WiFiMulti;

SoftwareSerial nextionSerial(D2, D1);

Nextion nex(nextionSerial);
NextionDualStateButton btn(nex, 0, 1, "btnOnOff");

const int DELAY_MILLIS = 50;
const int EXTERNAL_LIGHT_CHECK_SECONDS = 5;
const int REFRESH_LIGHT_COUNT = EXTERNAL_LIGHT_CHECK_SECONDS * 1000 / DELAY_MILLIS;

#define LIGHT_API PI_URL "light/1"

bool g_prevButtonState = false;
int g_updateWait = REFRESH_LIGHT_COUNT;

void setLightName(const char* szName) {
  Serial.printf("Setting light name to %s", szName);
  
  NextionText text(nex, 0, 2, "t0");
  text.setText((char*)szName);
}

void lightBtnPressed(NextionEventType type, INextionTouchable *widget)
{
  Serial.println("lightBtnPressed");
  if (type == NEX_EVENT_PUSH)
  {
  }
  else if (type == NEX_EVENT_POP)
  {
    callHTTP(PI_URL "light/0/on");
  }
}

void setup() {    

    Serial.begin(9600);
    Serial.println("Starting application");
    
    nextionSerial.begin(9600);
    nex.init();
    //btn.attachCallback(&lightBtnPressed);
    
    WiFiMulti.addAP(WIFI_AP, WIFI_PWD);
    bool gotData = false;
    int attempts = 0;
    while (!gotData && attempts < 10) {
      gotData = refreshLightData();
      if (gotData) {
        Serial.println("Got light data");
      } else {
        Serial.println("Failed to get light data");
        delay(1000);
      }

      ++attempts;
    }
}

bool refreshLightData() {
  if((WiFiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(LIGHT_API);
  
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode < 0) {
      Serial.println("error getting light data");
      http.end();
      return false;
    }
     
    String payload = http.getString();
    
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload.c_str());

    if (!root.success()) {
      Serial.println("Failed to parse JSON");
      http.end();
      return false;
    }

    const char* szLightName = root["name"];
    setLightName(szLightName);

    bool lightIsOn = root["isOn"];
    btn.setActive(lightIsOn);

    http.end();
    return true;
  } 

  Serial.println("Wifi not connected");
  return false;
}

void callHTTP(const char* szUrl) {
    // wait for WiFi connection
    if((WiFiMulti.run() == WL_CONNECTED)) {

        Serial.print("Calling: ");
        Serial.println(szUrl);

        HTTPClient http;

        http.begin(szUrl);
        int httpCode = http.GET();

        // httpCode will be negative on error
        if(httpCode > 0) {
            // HTTP header has been send and Server response header has been handled
            Serial.printf("[HTTP] GET... code: %d\n", httpCode);

            // file found at server
            if(httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                Serial.println(payload);
            }
        } else {
            Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }

        http.end();
    }
}

void loop() {  

  // if time elapsed, confirm light status
  // (in case the light has been turned on externally)
  --g_updateWait;
  if (g_updateWait <= 0) {
    if (refreshLightData()) {
      g_prevButtonState = btn.isActive();
    }

    g_updateWait = REFRESH_LIGHT_COUNT;
  }

  // check button state
  bool btnActive = btn.isActive();
  if (btnActive != g_prevButtonState) {
    if (btnActive) {
      callHTTP(LIGHT_API "/on");
    } else {      
      callHTTP(LIGHT_API "/off");
    }

    g_prevButtonState = btnActive;
  }
  
  delay(DELAY_MILLIS);
}

