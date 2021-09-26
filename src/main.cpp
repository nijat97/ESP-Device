
/*
One ESP sends temperature and humidity data to another ESP that holds webserver. 
User can send commands from webserver interface to the nodes.

Mesh network
Little components in React which gives info about the devices, table of devices, every new device shows
new box in web interface. 
Fix everything which was problem last semester. Everything should be working with at least min features. 

Create Similar msg structure in React(json,axios)
Use cypher 
*/

//uncomment if webserver
#define WEBSERVER

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#include "main.h"
#include <Arduino_JSON.h>

#if defined(WEBSERVER)

#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "FS.h"

#endif

// Replace with your network credentials (STATION)
const char *ssid = "Podmaniczky";
const char *password = "kiraly42";
//const char* ssid = "XperiaZ2";
//const char* password = "12345678z";

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //Trying to broadcast to all devices
uint8_t myAddress[] = {0x5C, 0xCF, 0x7F, 0xC2, 0x67, 0x6E};
//60:01:94:02:de:c7

//for data from webserver
String inputMessage1;
String inputMessage2;

//message: TA, Sa, 3, KeyValuesNum:3,
//0 Key:1-Value:21.4,
//1 Key:5-Value:1,
//2 Key:4-Value:2000

//mesh network
struct KeyValue_t
{
  uint32_t Key;
  uint32_t Value;
};

typedef struct Message_t
{

  uint8_t TargetAddress[4]; //use one byte for address
  uint8_t SenderAddress[4];
  uint16_t MessageID;
  uint16_t KeyValuesNum;
  KeyValue_t KeyValue[10];

} Message;

Message incomingMessage = {0}, outgoingMessage = {0};

#if defined(WEBSERVER)
const char *PARAM_INPUT_1 = "output";
const char *PARAM_INPUT_2 = "state";

AsyncWebServer server(80);
AsyncEventSource events("/events");

#endif

JSONVar board;
JSONVar packets;
bool newDataFromESP = 0; //if data is received from another device
bool newDataFromWeb = 0; //if user has clicked on buttons on web interface
int connectedDevices=0;
void printReceivedData();
void addBoardDataToPacket(JSONVar board);


void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus)
{
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0)
  {
    Serial.println("Delivery success");
  }
  else
  {
    Serial.println("Delivery fail");
  }
}

// callback function that will be executed when data is received
void OnDataRecv(uint8_t *mac_addr, uint8_t *incomingData, uint8_t len)
{
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
      
  printReceivedData();

  board["id"] = random(1,10);
  board["temperature"] = int(incomingMessage.KeyValue[0].Value);
  board["humidity"] = int(incomingMessage.KeyValue[1].Value);
  board["readingId"] = String(incomingMessage.MessageID);
  addBoardDataToPacket(board);
  Serial.println("Data received and added to packets!");
  Serial.println(packets);
  newDataFromESP = true;
}

void setup()
{
  // Initialize Serial Monitor
  Serial.begin(115200);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  randomSeed(77);

  // Set the device as a Station and Soft Access Point simultaneously
  WiFi.mode(WIFI_AP_STA);

#if defined(WEBSERVER)

  // Set device as a Wi-Fi Station
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

#endif

  // Init ESP-NOW
  if (esp_now_init() != 0)
  { //0 instead of ESP_OK
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 6, NULL, 0);

  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);

#if defined(WEBSERVER)

  if (!SPIFFS.begin())
  {
    Serial.println("Error occurred when mounting SPIFFS!");
    return;
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", String(), false);
  });

  /*
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/index.html", String(), false);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Max-Age", "10000");
    response->addHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "*");

    request->send(response);
  });
  
  server.serveStatic("/", SPIFFS, "/");
  //find a way to serve static files from the server,


  server.on("/demo.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/demo.js", "text/javascript");
  });

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2))
    {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();

      newDataFromWeb = true;
    }
    else
    {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }
    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
  });
  */
  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId())
    {
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 1000);
  });

  server.addHandler(&events);
  server.begin();

#endif
}

void printReceivedData()
{
  Serial.printf("Target address: %u \n", *incomingMessage.TargetAddress);
  Serial.printf("Sender address: %u \n", *incomingMessage.SenderAddress);
  Serial.printf("Message ID: %u \n", incomingMessage.MessageID);
  Serial.printf("Number of KeyValue pairs: %d \n", incomingMessage.KeyValuesNum);

  for (int i = 0; i < incomingMessage.KeyValuesNum; i++)
  {
    Serial.printf("%u: Key : %u , Value: %u \n", i, incomingMessage.KeyValue[i].Key, incomingMessage.KeyValue[i].Value);
  }

  Serial.println();
}

void addBoardDataToPacket(JSONVar board)
{
    for(int i=0;i<connectedDevices;i++)
    {
        if((int)packets[i]["id"] == (int)board["id"])
        {
          packets[i] = board;
          return;
        }
    }
    packets[connectedDevices] = board;
    connectedDevices++;
}


//divide this to two functions: sendToDevice(int devid), sendToServer()
void fillDataToStruct(bool isWebserver) 
{
  if (isWebserver == true)
  {
    memcpy(outgoingMessage.TargetAddress, broadcastAddress, sizeof(outgoingMessage.TargetAddress));
    memcpy(outgoingMessage.SenderAddress, myAddress, sizeof(outgoingMessage.SenderAddress));
    outgoingMessage.MessageID = 43; //just random number for testing
    outgoingMessage.KeyValuesNum = 2;
    outgoingMessage.KeyValue[0].Key = KEY_GPIO_NUM;
    outgoingMessage.KeyValue[0].Value = inputMessage1.toInt();
    outgoingMessage.KeyValue[1].Key = KEY_GPIO_STATE;
    outgoingMessage.KeyValue[1].Value = inputMessage2.toInt();
  }
}

void loop()
{
#if defined(WEBSERVER)

  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS)
  {
    String jsonString = JSON.stringify(packets);
    events.send(jsonString.c_str(), "new_readings", millis());
    lastEventTime = millis();
  }

  if (newDataFromWeb == true)
  {
    fillDataToStruct(true);
    esp_now_send(broadcastAddress, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage) - sizeof(outgoingMessage.KeyValue) + (outgoingMessage.KeyValuesNum) * sizeof(KeyValue_t));
    newDataFromWeb = false;
  }
#endif

#if !defined(WEBSERVER)

  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 3000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS)
  {
    fillDataToStruct(false);
    esp_now_send(broadcastAddress, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage) - sizeof(outgoingMessage.KeyValue) + (outgoingMessage.KeyValuesNum) * sizeof(KeyValue_t));
    lastEventTime = millis();
  }

#endif
}
