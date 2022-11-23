/**********************************************************************
  Filename    : Ultrasonic Ranging
  Description : Use the ultrasonic module to measure the distance.
  Auther      : www.freenove.com
  Modification: 2020/07/11
**********************************************************************/
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

///////////////////CONFIGURATION DEFAULTS/////////////////////
int CONTAINER_HEIGHT = 20;         // cm
float FULL_THRESHOLD_PERCENT = .80; // percent threshold to empty CONTAINER
#define PING_DELAY 5000            // ms (100ms => 20 pings/sec)
///////////////////CONFIGURATION DEFAULTS/////////////////////

///////////////////DEVICE CONNECTION SETUP//////////////////////////////
const char *ssid_Router     = "SSID"; //Enter the router name
const char *password_Router = "PASSWORD"; //Enter the router password
const char* mqtt_server = "broker.hivemq.com";
// const char* mqtt_server = "39958bcd92af469e963bca529830149b.s1.eu.hivemq.cloud";
// "f19f31c276ac420589c0c2288ef8f802.s1.eu.hivemq.cloud";
int port = 1883;
// int port = 8883;
char mac[50];
char clientId[50];
const char* mqttUser = "stmu2022";
const char* mqttPassword = "stmu2022";
///////////////////DEVICE CONNECTION SETUP//////////////////////////////

#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

#define trigPin 13       // define TrigPin
#define echoPin 14       // define EchoPin.
#define SDA 32                    //Define SDA pins
#define SCL 33                    //Define SCL pins
#define MAX_DISTANCE 700 // Maximum sensor distance is rated at 400-500cm.

#define BAUD 115200
#define PING_DELAY 5000            // ms (100ms => 20 pings/sec)
#define SONAR_DELAY 10            // microseconds
float FULL_THRESHOLD = CONTAINER_HEIGHT * (1 - FULL_THRESHOLD_PERCENT);
// timeOut= 2*MAX_DISTANCE /100 /340 *1000000 = MAX_DISTANCE*58.8
float timeOut = MAX_DISTANCE * 60;
int soundVelocity = 340; // define sound speed=340m/s
float sonarDistance = 0;
float thresholdPercentage = 0.00;

String state = "EMPTY";


WiFiClient espClient;
//WiFiClientSecure espClient;

PubSubClient client(espClient);
//PubSubClient * client;

String uniq = "";


/*
   note:If lcd1602 uses PCF8574T, IIC's address is 0x27,
        or lcd1602 uses PCF8574AT, IIC's address is 0x3F.
*/
LiquidCrystal_I2C lcd(0x27, 16, 2);
// LiquidCrystal_I2C lcd(0x3F, 16, 2);


void setup()
{

  Wire.begin(SDA, SCL);           // attach the IIC pin
  lcd.init();                     // LCD driver initialization
  lcd.backlight();                // Open the backlight
  lcd.setCursor(0, 0);            // Move the cursor to row 0, column 0
  lcd.print("Trash Level:   ");     // The print content is displayed on the LCD

  pinMode(trigPin, OUTPUT); // set trigPin to output mode
  pinMode(echoPin, INPUT);  // set echoPin to input mode

  Serial.begin(BAUD);       // Open serial monitor at 115200 baud to see ping results.



  connectWiFi();

  //Get unique mac id
  byte mac[6];
  WiFi.macAddress(mac);
  uniq =  String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  Serial.println("Unique ID from WiFI MAC: " + uniq); //24ac40110

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  client.setServer(mqtt_server, port);
  client.setCallback(callback);
  mqttReconnect();
  if (client.subscribe("testTopic") == true) {
    Serial.println("Success subscribing to topic");
    client.subscribe("testTopic");
  } else {
    Serial.println("Error subscribing to topic");
  }


}

void loop()
{
  DynamicJsonDocument doc(1024);
  doc["device_MAC_ID"] = uniq;

  delay(PING_DELAY); // Wait 100ms between pings (about 20 pings/sec).

  Serial.printf("Threshold: ");
  Serial.print(FULL_THRESHOLD);
  Serial.println("cm");
  //  Serial.printf("CONTAINER_HEIGHT: ");
  //  Serial.print(CONTAINER_HEIGHT);
  //  Serial.printf("\n");
  //  Serial.printf("FULL_THRESHOLD_PERCENT: ");
  //  Serial.print(FULL_THRESHOLD_PERCENT);
  //  Serial.printf("\n");
  sonarDistance = getSonar();
  thresholdPercentage = (1 - (sonarDistance / CONTAINER_HEIGHT)) * 100;
  Serial.printf("Distance: ");
  Serial.print(sonarDistance); // Send ping, get distance in cm and print result
  Serial.println("cm");

  lcd.setCursor(0, 0);            // Move the cursor to row 0, column 0
  lcd.print("Trash Level:    ");     // The print content is displayed on the LCD
  lcd.setCursor(0, 1);            // Move the cursor to row 1, column 0
  lcd.print(thresholdPercentage);
  lcd.print("%   ");          // The count is displayed every second

  if (sonarDistance <= FULL_THRESHOLD)
  {

    lcd.setCursor(0, 0);            // Move the cursor to row 0, column 0
    lcd.print("CONTAINER FULL! ");
    if (state == "EMPTY") // if previous state is EMPTY then send message
    {
      if (!client.connected()) {
        mqttReconnect();
      } else {
        // client.publish("testTopic", "State change to FULL");
        // client.subscribe("testTopic");
        doc["status"] = "FULL";
        doc["threshold_percentage"] = thresholdPercentage;
        doc["container_height_cm"] = CONTAINER_HEIGHT;
        doc["fullness_threshold_percentage"] = FULL_THRESHOLD_PERCENT;
        doc["sonar_distance"] = sonarDistance;
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
          Serial.println("Connection Err");
          return;
        }
        printLocalTime();
        char str[32];
        strftime(str, sizeof str, "%Y/%m/%d:%H.%M.%S", &timeinfo);
        doc["datetimestamp"] = str;

        char message[256];
        size_t n = serializeJson(doc, message);
        if (client.publish("testTopic", message) == true) {
          Serial.println("Success sending message");
          if (client.subscribe("testTopic") == true) {
            Serial.println("Success subscribing to topic");
            client.subscribe("testTopic");
          } else {
            Serial.println("Error subscribing to topic");
          }
        } else {
          Serial.println("Error sending message");
        }

      }

    }
    state = "FULL";
  }
  else {
    state = "EMPTY";
  }


  client.loop();



}

float getSonar()
{
  unsigned long pingTime;
  float distance;
  // make trigPin output high level lasting for 10μs to triger HC_SR04
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(SONAR_DELAY);
  digitalWrite(trigPin, LOW);
  // Wait HC-SR04 returning to the high level and measure out this waitting time
  pingTime = pulseIn(echoPin, HIGH, timeOut);
  // calculate the distance according to the time
  distance = (float)pingTime * soundVelocity / 2 / 10000;
  return distance; // return the distance value
}

void connectWiFi()
{
  //  Serial.begin(BAUD);
  delay(2000);
  Serial.println("Setup start");
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");

  // WiFi.begin(ssid_Router, password_Router);
  WiFi.begin("Wokwi-GUEST", "", 6);
  Serial.println(String("Connecting to ") + ssid_Router);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("WL_CONNECTED:");
    Serial.print(WL_CONNECTED);
    Serial.print("\n");
    Serial.print("WiFi.status():");
    Serial.print(WiFi.status());
    Serial.print("\n.");
    Serial.print("Attempting to connect to WEP network, SSID: ");
    Serial.println(ssid_Router);
    //    status = WiFi.begin(ssid, keyIndex, key);
  }
  Serial.println("\nConnected, IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Setup End");
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String stMessage;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    stMessage += (char)message[i];
  }
  Serial.println();
}

void mqttReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection…");
    String clientId = "djflasfsadfasdfsad";


    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("connected");
      if (client.subscribe("testTopic") == true) {
        Serial.println("Success subscribing to topic");
        client.subscribe("testTopic");
      } else {
        Serial.println("Error subscribing to topic");
      }
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Connection Err");
    return;
  }

  Serial.println(&timeinfo, "%H:%M:%S");

  Serial.println(&timeinfo, "%d/%m/%Y   %Z");
}
