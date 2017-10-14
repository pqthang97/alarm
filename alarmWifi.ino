#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#include <Wire.h> 
#include <RtcDS3231.h>

#define address 0x50

RtcDS3231<TwoWire> Rtc(Wire);

IPAddress local_IP(192, 168, 4, 22);
IPAddress gateway(192, 168, 4, 9);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server(80);


const char *SSID = "Py Wifi";
const int numberOfFeedTimes = 10;
int feedTime[numberOfFeedTimes * 4];

#define countof(a) (sizeof(a) / sizeof(a[0]))

void ReadFeedTimes() {
     for (int i = 0; i < numberOfFeedTimes * 4; i++)
     {
          feedTime[i] = EEPROM.read(i);
     }
}
void WriteFeedTime(int selectedFeedTime, int hour, int min, int value, int pin) {
     int x = (selectedFeedTime - 1) * 4;
     Serial.println(x);
     Serial.println(hour);
     EEPROM.write(x, hour);
     delay(5);
     EEPROM.write(x + 1, min);
     delay(5);
     EEPROM.write(x + 2, value);
     delay(5);
     EEPROM.write(x + 3, pin);
     EEPROM.commit();
     ReadFeedTimes();
}
void setupNewFeedTime() {

     for (int i = 0; i < numberOfFeedTimes * 4; i = i + 4) {
       EEPROM.write(i, -1);
       delay(5);
       EEPROM.write(i + 1, -1);
       delay(5);
       EEPROM.write(i + 2, -1);
       delay(5);
       EEPROM.write(i + 3, -1);
     }
     
     EEPROM.commit();
     ReadFeedTimes();
}
void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}
void setup()
{
  Wire.begin(0, 2);
  Rtc.Begin();
  EEPROM.begin(1024);
  //setupNewFeedTime();
  
  Serial.begin(115200);
  Serial.println();

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  ReadFeedTimes();

  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid()) 
  {
      Serial.println("RTC lost confidence in the DateTime!");
      Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning())
  {
      Serial.println("RTC was not actively running, starting now");
      Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
      Serial.println("RTC is older than compile time!  (Updating DateTime)");
      Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
      Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
      Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone); 
  
  Serial.print("Setting soft-AP configuration ... ");
  Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");

  Serial.print("Setting soft-AP ... ");
  Serial.println(WiFi.softAP(SSID) ? "Ready" : "Failed!");

  Serial.print("Soft-AP IP address = ");
  Serial.println(WiFi.softAPIP());

  server.on("/", []() {
    String webPage = "";
    webPage += "<h1>ESP8266 Web Server</h1><p>Socket #1 <a href=\"socket1On\"><button>ON</button></a>&nbsp;<a href=\"socket1Off\"><button>OFF</button></a></p>";
    webPage += "<p>Socket #2 <a href=\"socket2On\"><button>ON</button></a>&nbsp;<a href=\"socket2Off\"><button>OFF</button></a></p>";
    server.send(200, "text/html", webPage);
    
  });

  server.on("/check", []() {
    RtcDateTime now = Rtc.GetDateTime();
    String currentHour = now.Hour() < 10 ? "0" + String(now.Hour()) : String(now.Hour());
    String currentMinute = now.Minute() < 10 ? "0" + String(now.Minute()) : String(now.Minute());
    String currentSecond = now.Second() < 10 ? "0" + String(now.Second()) : String(now.Second());
    server.send(200, "text/html", "{\"device\": \"IOTALARM\", \"time\": \"" + currentHour + ":" + currentMinute + ":" + currentSecond +"\",\"day\": \"" + now.Day() + "\", \"month\": \"" + now.Month() + "\", \"year\": \"" + now.Year() + "\"}");  
  });

  server.on("/set", []() {
    String strHour = server.arg("hour");
    String strMinute = server.arg("minute");
    String strID = server.arg("id");
    String strValue = server.arg("value");

    WriteFeedTime(atoi(strID.c_str()), atoi(strHour.c_str()), atoi(strMinute.c_str()), atoi(strValue.c_str()), 16);
    server.send(200, "text/html", "successful");
  });

  server.on("/all", []() {
    String content = "[";
    for (int i = 0; i < numberOfFeedTimes * 4; i = i + 4)
    {
      content += "{\"id\":" + String( i / 4 + 1) + ",\"hour\":" + String(feedTime[i]) + ",\"minute\":" + String(feedTime[i+1]) + ",\"value\":" + String(feedTime[i+2]) + ",\"pin\":" + String(feedTime[i+3]) + "}"  + ((i != (numberOfFeedTimes * 4 - 4)) ? "," : "");
    }
    content += "]";
    server.send(200, "text/html", content);
  });
  server.begin();
  Serial.println("HTTP server started");


  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);
}

void loop() {
  Serial.printf("Hour: %d : %d\n", feedTime[0], feedTime[1]);
  Serial.printf("Stations connected to soft-AP = %d \n", WiFi.softAPgetStationNum());
  server.handleClient();

  if (!Rtc.IsDateTimeValid()) 
    {
        
        Serial.println("RTC lost confidence in the DateTime!");
    }

    RtcDateTime now = Rtc.GetDateTime();
    printDateTime(now);
    Serial.println();

  for (int i = 0; i < numberOfFeedTimes * 4; i = i + 4) {
    int hour = feedTime[i];
    int minutes = feedTime[i+1];
    int value = feedTime[i+2];
    int pin = feedTime[i+3];
    if(now.Hour() == hour && minutes == now.Minute())
    {
      Serial.println(now.Second() < 5);
      if(now.Second() < 5)
        digitalWrite(pin, value == 1 ? LOW : HIGH);
      else
        digitalWrite(pin, value == 1 ? HIGH : LOW);
    }
    
  }
  delay(1000);
}
