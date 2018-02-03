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

Nextion nex(Serial);
NextionDualStateButton btn(nex, 0, 1, "btnOnOff");

void setLightName(const char* szName) {
  NextionText text(nex, 0, 2, "t0");
  text.setText((char*)szName);
}

void lightBtnPressed(NextionEventType type, INextionTouchable *widget)
{
  Serial.println("lightBtnPressed");
  if (btn.isActive())
  {
    callHTTP(PI_URL "light/0/off");
  }
  else if (type == NEX_EVENT_POP)
  {
    callHTTP(PI_URL "light/0/on");
  }
}

void setup() {    

    WiFiMulti.addAP(WIFI_AP, WIFI_PWD);

    Serial.begin(9600);
    nex.init();

    getLightData(PI_URL "light/0");
    btn.attachCallback(&lightBtnPressed);
}

bool getLightData(const char* szUrl) {

  Serial.println("getting light data");
  if((WiFiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(szUrl);
  
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode < 0) {
      Serial.println("error");
      return false;
    }
     
    String payload = http.getString();
    
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload.c_str());

    if (!root.success()) {
      Serial.println("Failed to parse JSON");
      return false;
    }

    const char* szLightName = root["name"];
    setLightName(szLightName);

    bool lightIsOn = root["isOn"];
    Serial.println(lightIsOn ? "Light on" : "Light off");
    btn.setActive(lightIsOn);
    
    return true;
  }  
}

void callHTTP(const char* szUrl) {
    // wait for WiFi connection
    if((WiFiMulti.run() == WL_CONNECTED)) {

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

    nex.poll();
}

