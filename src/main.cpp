
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

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //Trying to broadcast to all devices
//uint8_t myAddress[] = {0x5C, 0xCF, 0x7F, 0xC2, 0x67, 0x6E};
//60:01:94:02:de:c7


// Structures to hold messages
struct KeyValue_t
{
  uint32_t Key;
  uint32_t Value;
};

typedef struct Message_t
{
  uint16_t TargetAddress; //use one byte for address
  uint16_t SenderAddress;
  uint16_t readingID;
  uint16_t KeyValuesNum;
  KeyValue_t KeyValue[10];

} Message;

Message incomingMessage = {0}, outgoingMessage = {0};

// For data from webserver
String inputMessage1;
String inputMessage2;

#if defined(WEBSERVER)
const char *PARAM_INPUT_1 = "output";
const char *PARAM_INPUT_2 = "state";

AsyncWebServer server(80);
AsyncEventSource events("/events");

#endif

JSONVar board;
JSONVar packets;
//bool newDataFromESP = 0; //if data is received from another device
bool newDataFromWeb = 0; //if user has clicked on buttons on web interface
int connected_devices=0;
int msg_count=0;

/* Function prototypes */
void printReceivedData();
void addBoardDataToPacket(JSONVar);
void OnDataSent(uint8_t*, uint8_t);
void OnDataRecv(uint8_t*, uint8_t*, uint8_t);

void setup()
{
  // Initialize Serial Monitor
  Serial.begin(115200);

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  Serial.println();
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  // Set the sender and target addresses in message packet
  String addr = WiFi.macAddress().substring(15);
  memcpy(&outgoingMessage.SenderAddress, &addr, sizeof(addr));
  memcpy(&outgoingMessage.TargetAddress, &broadcastAddress[5], sizeof(broadcastAddress[5]));

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

/* Prints the result of sending a message. It does not check if the
message is delivered */
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus)
{
  Serial.print("Last Packet Send Status: ");
  if (sendStatus == 0)
  {
    Serial.println("Send success");
  }
  else
  {
    Serial.println("Send fail");
  }
}

/* Callback function that will be executed when data is received on Webserver device */
void OnDataRecv(uint8_t *mac_addr, uint8_t *incomingData, uint8_t len)
{
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
      
  printReceivedData();
  board["sender"] = int(incomingMessage.SenderAddress);
  board["target"] = int(incomingMessage.TargetAddress);
  board["readingId"] = int(incomingMessage.readingID);
  board["numOfPairs"] = int(incomingMessage.KeyValuesNum);

  for(int i=0;i<incomingMessage.KeyValuesNum;i++)
  {
    board["data"][i] = String("key: " + String(incomingMessage.KeyValue[i].Key) + " value: " + String(incomingMessage.KeyValue[i].Value));
    //board["value" + String(i)] = int(incomingMessage.KeyValue[i].Value);
  }
  
  addBoardDataToPacket(board);
  Serial.println("Data received and added to packets!");
  Serial.println(packets);
  //newDataFromESP = true;
}

/* Prints the incoming message from a node to the Serial port */ 
void printReceivedData()
{
  Serial.printf("Target address: %u \n", incomingMessage.TargetAddress);
  Serial.printf("Sender address: %u \n", incomingMessage.SenderAddress);
  Serial.printf("Reading ID: %u \n", incomingMessage.readingID);
  Serial.printf("Number of KeyValue pairs: %d \n", incomingMessage.KeyValuesNum);

  for (int i = 0; i < incomingMessage.KeyValuesNum; i++)
  {
    Serial.printf("%u: Key : %u , Value: %u \n", i, incomingMessage.KeyValue[i].Key, incomingMessage.KeyValue[i].Value);
  }

  Serial.println();
}

/* Appends the JSON packet from the node to an array of packets 
to send to the server. If there is already a packet with the same ID,
it will be replaced with the new packet*/
void addBoardDataToPacket(JSONVar board)
{
    for(int i=0;i<connected_devices;i++)
    {
        if((int)packets[i]["sender"] == (int)board["sender"])
        {
          packets[i] = board;
          return;
        }
    }
    packets[connected_devices] = board;
    connected_devices++;
}


/* If there is an update from the server, this function is called to send update to a node */
void sendToNode() 
{
    outgoingMessage.readingID = msg_count;
    msg_count++;

    #ifdef WEBSERVER
    outgoingMessage.KeyValuesNum = 2;
    outgoingMessage.KeyValue[0].Key = KEY_GPIO_NUM;
    outgoingMessage.KeyValue[0].Value = inputMessage1.toInt();
    outgoingMessage.KeyValue[1].Key = KEY_GPIO_STATE;
    outgoingMessage.KeyValue[1].Value = inputMessage2.toInt();
    newDataFromWeb = false;
    #else
    outgoingMessage.KeyValuesNum = 2;
    outgoingMessage.KeyValue[0].Key = KEY_TEMPERATURE_F_CEL;
    outgoingMessage.KeyValue[0].Value = 26; //example sensor data
    outgoingMessage.KeyValue[1].Key = KEY_HUMIDITY_F_PER;
    outgoingMessage.KeyValue[1].Value = 35; //example sensor data
    #endif
    esp_now_send(broadcastAddress, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage) -
    sizeof(outgoingMessage.KeyValue) + (outgoingMessage.KeyValuesNum) * sizeof(KeyValue_t));

}

void loop()
{
#if defined(WEBSERVER)

  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;

  /* Packets are sent to the server in interval of EVENT_INTERVAL_MS */
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS)
  {
    String jsonString = JSON.stringify(packets);
    events.send(jsonString.c_str(), "new_readings", millis());
    Serial.println("New readings sent!");
    lastEventTime = millis();
  }

  /* If there is an update from the server, send it to the node */
  if (newDataFromWeb == true)
  {
    sendToNode();
  }

#else

  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 3000;
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS)
  {
    sendToNode();
    lastEventTime = millis();
  }

#endif
}
