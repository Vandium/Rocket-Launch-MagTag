

/* 
Function: Connnects to the launch library API and displays name date and description of
the latest or upcoming launch on an E ink Display

Author: Brad Dranko
Date Created: 12/17/22

*/

#include <TimeLib.h>

#include <ArduinoJson.h>
#include <ArduinoJson.hpp> 

#include "Adafruit_ThinkInk.h"
#include "Anita_semi_square9pt7b.h"
#include "Anita_semi_square8pt7b.h"
#include "Bitmaps.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>

//E Ink display pin deffintions
#define EPD_DC 7     // can be any pin, but required!
#define EPD_CS 8     // can be any pin, but required!
#define EPD_BUSY -1  // can set to -1 to not use a pin (will wait a fixed delay)
#define SRAM_CS -1   // can set to -1 to not use a pin (uses a lot of RAM!)
#define EPD_RESET 6  // can set to -1 and share with chip Reset (can't deep sleep)


// 2.9" Grayscale for MagTag
ThinkInk_290_Grayscale4_T5 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

#define COLOR1 EPD_BLACK
#define COLOR2 EPD_LIGHT
#define COLOR3 EPD_DARK


#define RSTButton 15


const int UTC_Correction = -5; //Offset in housrs of current time zone from UTC
tmElements_t UTC_Launch_Time;
time_t old_unix;
time_t new_unix;


WiFiClient client;

void setup() {

  pinMode(BUTTON_A, INPUT_PULLUP);
  Serial.begin(115200);
  delay(3000);
  Serial.println("Next Launch Tag V0.1");

  display.begin(THINKINK_GRAYSCALE4);

  WiFiManager wm;
  bool res;
  res = wm.autoConnect("LaunchTag");

  if (!res) {
    Serial.println("Wifi Connection Failed");
    wm.resetSettings();
    //ESP.restart
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(3000);




  GetLaunch();
}


void ManualReset() {
  Serial.println("Manual Reset triggered.");

  //display.begin(THINKINK_GRAYSCALE4);
  display.clearBuffer();
  display.setFont(&Anita_semi_square8pt7b);
  display.setTextColor(EPD_BLACK);
  display.setCursor(0, 10);
  display.print("Wifi Reset.");
  display.setCursor(0, 30);
  display.print("Please connect to SSID \"LaunchTag\" and enter WiFi credentials.");
  display.drawBitmap(100,58,Git_QR_Bitmap_Small,60,60, EPD_BLACK);
  display.display();

  WiFiManager wmr;
  wmr.resetSettings();
  bool res;
  res = wmr.autoConnect("LaunchTag");

  if (!res) {
    Serial.println("Wifi Connection Failed");
    //ESP.restart
  }

  display.clearBuffer();
  display.setCursor(0, 10);
  display.print("Connected");
  display.setCursor(0, 30);
  display.print(WiFi.localIP());
  display.display();
  delay(5000);
  
}


void loop() {
  // put your main code here, to run repeatedly:
  if (!digitalRead(BUTTON_A))
  {
    ManualReset();
  }
}


void GetLaunch() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient client;
    client.begin("https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=1&format=json&status=1");
    int httpCode = client.GET();

    String payload = client.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    int count = doc["count"];  // 238
    const char* next = doc["next"];
    // doc["previous"] is null

    JsonObject Results = doc["results"][0];
    const char* Results_id = Results["id"];  // "bf91b6d9-f517-4198-843c-dc82dfa6f9b0"
    const char* Results_url = Results["url"];
    const char* Results_slug = Results["slug"];  // "falcon-9-block-5-starlink-group-4-37"
    const char* Results_name = Results["name"];  // "Falcon 9 Block 5 | Starlink Group 4-37"

    JsonObject status = Results["status"];
    int status_id = status["id"];                            // 3
    const char* status_name = status["name"];                // "Launch Successful"
    const char* status_abbrev = status["abbrev"];            // "Success"
    const char* status_description = status["description"];  // "The launch vehicle ...

    const char* Results_last_updated = Results["last_updated"];  // "2022-12-18T00:58:06Z"
    const char* Results_net = Results["net"];                    // "2022-12-17T21:32:00Z"
    const char* Results_window_end = Results["window_end"];      // "2022-12-17T21:32:00Z"
    const char* Results_window_start = Results["window_start"];  // "2022-12-17T21:32:00Z"
    int Results_probability = Results["probability"];            // 60
    const char* Results_holdreason = Results["holdreason"];      // nullptr
    const char* Results_failreason = Results["failreason"];      // nullptr
    // Results["hashtag"] is null

    JsonObject Mission = Results["mission"];
    const char* Mission_Name = Mission["name"];
    const char* Mission_Description = Mission["description"];

    Time_Correction(Results_window_start);


    Serial.print("Launch name:");
    Serial.println(Results_name);

    char buffer[400];
    sprintf(buffer,"NEXT LAUNCH: %02d/%02d/%4d %02d:%02d:%02d \n %s \n %s",month(), day(), year(), hour(), minute(), second(),Results_name,Mission_Description );
    display.clearBuffer();
    display.setFont(&Anita_semi_square8pt7b);
    display.setTextColor(EPD_BLACK);
    display.setCursor(0, 10);
    display.print(buffer);
    /* 
    display.print("Next Launch");
    display.setCursor(0, 30);
    display.print(Results_name);
    display.setCursor(0, 60);
    display.printf("Date: %02d/%02d/%4d %02d:%02d:%02d", month(), day(), year(), hour(), minute(), second());
    */
    display.display();


    client.end();


  }  //end of connected


  else {
    Serial.println("No WiFi Connection");
    return;
  }


}  //GetLaunch end




void Time_Correction(String UTC_String)
{
UTC_Launch_Time.Second = UTC_String.substring(17,19).toInt();
UTC_Launch_Time.Minute = UTC_String.substring(14,16).toInt();
UTC_Launch_Time.Hour = UTC_String.substring(11,13).toInt();
UTC_Launch_Time.Day = UTC_String.substring(8,10).toInt();
UTC_Launch_Time.Month = UTC_String.substring(5,7).toInt(); //january is at index 0
UTC_Launch_Time.Year = UTC_String.substring(0,4).toInt() - 1970; // should be number of years since 1970

old_unix = makeTime(UTC_Launch_Time); //convert launch date/time into unix time
new_unix = old_unix + UTC_Correction * SECS_PER_HOUR;
setTime(new_unix);
Serial.print("Unix launch time in EST: ");
Serial.println(new_unix);
Serial.printf("%02d/%02d/%4d %02d:%02d:%02d", day(), month(), year(), hour(), minute(), second());

}
