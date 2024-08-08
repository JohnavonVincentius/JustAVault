#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecureBearSSL.h>

// Replace with your network credentials
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWD";
#define AuthorizationCode "b1803ac0de6603954e74ba6196e40149"

#define ServerURL "SERVER_ENDPOINT"    // + authorization code


// #define DEBUG

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Variable to save current epoch time
unsigned long epochTime; 

// Function that gets current epoch time
void sendTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  Serial.print(now);
}

bool sendServer(const String &args){

  if(WiFi.status()!= WL_CONNECTED){
    return false;
  }

  String mainURL = String(ServerURL) + String(AuthorizationCode);
  mainURL += "/inform?data=";
  mainURL += args;


  #ifdef DEBUG
    Serial.println("INIT SEND: ");
    Serial.println(mainURL);
  #endif

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  HTTPClient https;
  client->setInsecure();

  https.begin(*client,mainURL.c_str());

  int respCode = https.GET();

  #ifdef DEBUG
    Serial.println(respCode);
    Serial.println(https.getString());
  #endif


  if(respCode != 205){
    return false;
  }

  return true;
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  #ifdef DEBUG
    Serial.print("Connecting to WiFi ..");
  #endif

  while (WiFi.status() != WL_CONNECTED) {
    #ifdef DEBUG
      Serial.print('.');
    #endif
    delay(1000);
  }

  #ifdef DEBUG
    Serial.println(WiFi.localIP());
  #endif

  Serial.print("n7qcpqc34nu0");
}

void setup()
{
  Serial.begin(9600);

  #ifdef DEBUG
    Serial.println("WARNING: DEBUG ENABLED");
  #endif

  initWiFi();
  timeClient.begin();
}

void loop(){
  if(Serial.available()>0){
    String data = Serial.readString();

    #ifdef DEBUG
      Serial.println(data);
    #endif

    if(data == "EPOCH"){
      sendTime();
    }else{
      Serial.print(sendServer(data));    
    }
  }
}