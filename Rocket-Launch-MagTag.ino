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
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h>

//E Ink display pin deffintions for MagTag
#define EPD_DC 7     
#define EPD_CS 8     
#define EPD_BUSY -1  
#define SRAM_CS -1   
#define EPD_RESET 6  


// 2.9" Grayscale for MagTag
ThinkInk_290_Grayscale4_T5 display(EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
#define COLOR1 EPD_BLACK
#define COLOR2 EPD_LIGHT
#define COLOR3 EPD_DARK


const int UTC_Correction = -5; //Offset in housrs of current time zone from UTC
tmElements_t UTC_Launch_Time;
time_t old_unix;
time_t new_unix;

unsigned long Update_Interval = 1800000; //numbers of milliseconds between API calls. Note: withpout api key only 15 updates perr hour are allowed
unsigned long Last_Update = 0;

bool last_btnA_State = true; //buttons on magtag go LOW when pressed so logic is inverted
unsigned long btnA_init_time = 0;
unsigned int Reset_Hold_Time = 10000; //number of milliseconds the button must be held to clear the saved configuration


StaticJsonDocument<160> filter;


WiFiManager wm;
const char* config_str = "<p> Optional feature selection:</p>"
                    "<input style='width: auto; margin: 0 10px 0 10px;' type='radio' id='option1' name='NeoPixel' value'NPX'><label for='option1'>Lights</label><br>";
WiFiManagerParameter config_field(config_str);
//WiFiClient client;


Adafruit_NeoPixel *pixels;
uint32_t StandardColor = pixels->ColorHSV(186,19,100);


void setup() {

  pinMode(BUTTON_A, INPUT_PULLUP);

  //Neopixel config
  pixels = new Adafruit_NeoPixel(NEOPIXEL_NUM, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
  // The magtag has the ability to shutoff the neopixels for power savings. LOW = On
  pinMode(NEOPIXEL_POWER, OUTPUT); 
  digitalWrite(NEOPIXEL_POWER, NEOPIXEL_POWER_ON);
  delay(200);
  pixels->begin();
  pixels->clear(); //make sure all pixels are off
  pixels->show();

  display.begin(THINKINK_GRAYSCALE4);

  Serial.begin(115200);
  Serial.println("Next Launch Tag V0.1");
 
  wm.setClass("invert"); //set the config portal to darkl theme
  wm.addParameter(&config_field);
  wm.setSaveConfigCallback(saveParamsCallback);
  bool res;
  res = wm.autoConnect("LaunchTag");
  wm.setWiFiAutoReconnect(true);

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


  //Create json document to filter the stream from the api. this reduces the amount of memory needed since there is a lot of information provided that we dont use.
  JsonObject filter_results_0 = filter["results"].createNestedObject();
  filter_results_0["window_start"] = true;
  filter_results_0["status"]["id"] = true;
  filter_results_0["rocket"]["configuration"]["name"] = true;
  filter_results_0["mission"]["name"] = true;


  GetLaunch(); 
  Last_Update = millis();
}


void ManualReset() {
  Serial.println("Manual Reset triggered.");

  display.clearBuffer();
  display.setFont(&Anita_semi_square8pt7b);
  display.setTextColor(EPD_BLACK);
  display.setCursor(0, 10);
  display.print("Wifi Reset.");
  display.setCursor(0, 30);
  display.print("Connect to SSID \"LaunchTag\" and enter WiFi credentials.");
  display.drawBitmap(100,58,Git_QR_Bitmap_Small,60,60, EPD_BLACK);
  display.display();

  wm.resetSettings();
  bool res;
  res = wm.autoConnect("LaunchTag");

  if (!res) {
    Serial.println("Wifi Connection Failed");
    //ESP.restart
  }

  display.clearBuffer();
  display.setCursor(0, 10);
  display.print("Sucessfully Connected to: ");
  display.print(WiFi.SSID());
  display.setCursor(0, 60);
  display.print("IP Address: ");
  display.print(WiFi.localIP());
  display.display();
  delay(5000);

  GetLaunch(); //update the display with launch data since we jsut connected to a network
  
}


void loop() {

  last_btnA_State = digitalRead(BUTTON_A); 

  while(!digitalRead(BUTTON_A)) //put in a while loop so we do not go off doing some other task while button A is being held
  {
    if(last_btnA_State) //if the button was not previously pressed start the timer.
    {
      last_btnA_State = false;
      btnA_init_time =  millis();
    }

    if((millis() - btnA_init_time) > Reset_Hold_Time)
    {
      ManualReset();
    }
  }


  if((millis() - Last_Update) > Update_Interval) //get updated launch info after a period of time has elapsed
  {
    pixels->fill(StandardColor);
    pixels->show();
    GetLaunch();
    Last_Update = millis();
    delay(5000); //leave the lights on for 5 seconds after display updates
    pixels->fill();
    pixels->show();
  }

  if(!)
}


void GetLaunch() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient client;
    client.begin("https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=10&format=json");
    int httpCode = client.GET();

    String payload = client.getString();
    //Serial.println(payload);
    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (error) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      display.clearBuffer();
      display.setFont(&Anita_semi_square8pt7b);
      display.setTextColor(EPD_BLACK);
      display.setCursor(0, 15);
      display.println("ERROR: JSON could not be deserialized.");
      display.println(error.c_str());
      display.display();
      return;
    }

    for (JsonObject result : doc["results"].as<JsonArray>()) {

      int result_status_id = result["status"]["id"]; // 1, 2, 2, 2, 2, 1, 2, 2, 2, 2
      const char* result_window_start = result["window_start"]; // "2023-01-03T14:56:00Z", ...
      const char* result_rocket_configuration_name = result["rocket"]["configuration"]["name"]; // "Falcon 9", ...
      const char* result_mission_name = result["mission"]["name"]; // "Transporter 6 (Dedicated SSO ...


      if (result_status_id != 3 && result_status_id != 4 ) // id's 3 and 4 corrispond to a launch that has already happend
      {
        client.end(); // close the connection

        Serial.printf("Status ID: %i \n", result_status_id);
        Serial.printf("Window Start: %s \n", result_window_start);
        Serial.printf("Rocket: %s \n", result_rocket_configuration_name);
        Serial.printf("Mission name: %s \n",result_mission_name);

        Time_Correction(result_window_start);
        const char* meridiem = "";

        if(isAM())
        {
          meridiem = "AM";
        }
        else
        {
          meridiem = "PM";
        }

        char Present_Buffer[300];
        sprintf(Present_Buffer,"----------NEXT LAUNCH----------\nDate: %02d/%02d/%4d \nTime: %02d:%02d:%02d %s\nRocket: %s\nMission: %s",month(), day(), year(), hourFormat12(), minute(), second(), meridiem, result_rocket_configuration_name, result_mission_name);
        display.clearBuffer();
        display.setFont(&Anita_semi_square8pt7b);
        display.setTextColor(EPD_BLACK);
        display.setCursor(0, 15);
        display.print(Present_Buffer);

        display.display();
        return; //found the data and displayed it we are done here

      }

      else
      {
        Serial.printf("Status ID indicates old launch: %i", result_status_id);
      }

    }

  }  //end of connected

  else 
  {
    Serial.println("No WiFi Connection");
    display.clearBuffer();
    display.setFont(&Anita_semi_square8pt7b);
    display.setTextColor(EPD_BLACK);
    display.setCursor(0, 15);
    display.println("ERROR: No WiFi connection.");
    display.display();
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

void saveParamsCallback()
{
  Serial.println("Get Params");
  Serial.print("Selection: ");
  Serial.println(getParam("NeoPixel"));
}

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}
