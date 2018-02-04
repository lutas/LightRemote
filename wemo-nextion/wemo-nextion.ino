#include <Arduino.h>
#include <ArduinoJson.h>

#include <Nextion.h>
#include <NextionText.h>
#include <NextionButton.h>
#include <NextionDualStateButton.h>
#include <SoftwareSerial.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include "private.h"
#define LIGHT_API PI_URL "light/"

namespace {
  ESP8266WiFiMulti WiFiMulti;
  
  SoftwareSerial nextionSerial(D2, D1);
  
  Nextion nex(nextionSerial);
  NextionText text(nex, 0, 2, "t0");
  NextionDualStateButton btn(nex, 0, 1, "btnOnOff");
  NextionButton btnLeft(nex, 0, 3, "btnLeft");
  NextionButton btnRight(nex, 0, 4, "btnRight");
  
  const int DELAY_MILLIS = 50;
  const int EXTERNAL_LIGHT_CHECK_SECONDS = 5;
  const int REFRESH_LIGHT_COUNT = EXTERNAL_LIGHT_CHECK_SECONDS * 1000 / DELAY_MILLIS;
  
  int g_lightId = 0;
  const int NUM_LIGHTS = 6;
  
  bool g_prevButtonState = false;
  int g_updateWait = REFRESH_LIGHT_COUNT;
}

void setLightName(const char* szName) {

  char* szText = new char[strlen(szName) + 1];
  strcpy(szText, szName);
  
  text.setText(szText);
  delete[] szText;
}

void setup() {    

  Serial.begin(9600);
  Serial.println("Starting application");
  
  nextionSerial.begin(9600);
  nex.init();
  
  WiFiMulti.addAP(WIFI_AP, WIFI_PWD);
  bool gotData = false;
  int attempts = 0;
  while (!gotData && attempts < 10) {
    gotData = refreshLightData(g_lightId);
    if (gotData) {
      Serial.println("Got light data");
    } else {
      Serial.println("Failed to get light data");
      delay(1000);
    }

    ++attempts;
  }

  btnLeft.attachCallback(&prevLight);
  btnRight.attachCallback(&nextLight);
}

bool refreshLightData(int lightId) {
  if((WiFiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;

    char szUrl[200];
    sprintf(szUrl, LIGHT_API "%d", lightId);
    http.begin(szUrl);
  
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if(httpCode < 0) {
      Serial.println("error getting light data");
      http.end();
      return false;
    }
     
    String payload = http.getString();
    Serial.println(payload.c_str());
    
    StaticJsonBuffer<400> jsonBuffer;
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

bool setLightState(int id, bool on) {
  char szUrl[200];
  sprintf(szUrl, LIGHT_API "%d/%s", id, on ? "on" : "off");

  return callHTTP(szUrl);
}

bool callHTTP(const char* szUrl) {
  // wait for WiFi connection
  if((WiFiMulti.run() != WL_CONNECTED)) {
    return false;
  }

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
    
    http.end();
    return false;
  }

  http.end();
  return true;
}

void prevLight(NextionEventType type, INextionTouchable *widget)
{
  --g_lightId;
  if (g_lightId < 0)
    g_lightId = NUM_LIGHTS + g_lightId;

  updateLight();
}

void nextLight(NextionEventType type, INextionTouchable *widget)
{
  ++g_lightId;
  g_lightId %= NUM_LIGHTS;

  updateLight();
}

void updateLight() {  
  if (refreshLightData(g_lightId)) {
    g_prevButtonState = btn.isActive();
  }

  g_updateWait = REFRESH_LIGHT_COUNT;
}

void loop() { 

  // if time elapsed, confirm light status
  // (in case the light has been turned on externally)
  --g_updateWait;
  if (g_updateWait <= 0) {
    updateLight();
  }

  // check button state
  bool btnActive = btn.isActive();
  if (btnActive != g_prevButtonState) {
    setLightState(g_lightId, btnActive);

    g_prevButtonState = btnActive;
  }


  unsigned long wait = millis() + DELAY_MILLIS;
  while (wait > millis()) {    
    nex.poll();
  }
}

