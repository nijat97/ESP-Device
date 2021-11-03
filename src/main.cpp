
/*

Use cypher 
*/

//uncomment if webserver
//#define WEBSERVER

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

/* Structure for ESPNOW messaging */
typedef struct Message_t
{
    uint16_t SenderAddress;
    uint16_t TargetAddress;
    uint16_t readingID;
    uint16_t KeyValuesNum;
    KeyValue_t KeyValue[5];
} Message;

typedef struct Packet_t
{
    uint32_t ID;
    uint16_t SenderAddress;
    uint16_t readingId;
    uint32_t Key;
    uint32_t Value;
} Packet;

Message incomingMessage = {0}, outgoingMessage = {0};
Packet packets[5] = {0, 0, 0, 0, 0};
// For data from webserver
//reserve required memory for Strings in setup , try to use local variables, and pass reference for Strings
//String inputMessage1;
//String inputMessage2;

#if defined(WEBSERVER)

AsyncWebServer server(80);
AsyncEventSource events("/events");

#endif

String json_string;
bool newDataFromESP = 0; //if data is received from another device
bool newDataFromWeb = 0; //if user has clicked on buttons on web interface
int msg_count_rcv = 0;
int msg_count_snt = 0;
int index_packet = 0;

/* Function prototypes */
void printReceivedData();
void OnDataSent(uint8_t *, uint8_t);
void OnDataRecv(uint8_t *, uint8_t *, uint8_t);

void setup()
{
    // Initialize Serial Monitor
    Serial.begin(115200);

    pinMode(2, OUTPUT);
    digitalWrite(2, LOW);

    Serial.println();
    Serial.print(F("ESP Board MAC Address:  "));
    Serial.println(WiFi.macAddress());

    // Set the device as a Station and Soft Access Point simultaneously
    WiFi.mode(WIFI_AP_STA);

#if defined(WEBSERVER)

    // Set device as a Wi-Fi Station
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println(F("Setting as a Wi-Fi Station.."));
    }
    Serial.print(F("Station IP Address: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("Wi-Fi Channel: "));
    Serial.println(WiFi.channel());

#endif

    // Init ESP-NOW
    if (esp_now_init() != 0)
    { //0 instead of ESP_OK
        Serial.println(F("Error initializing ESP-NOW"));
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
        Serial.println(F("Error occurred when mounting SPIFFS!"));
        return;
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", String(), false); });

    server.on(
        "/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            String input = "";
            JSONVar input_packet;
            for (size_t i = 0; i < len; i++)
            {
                Serial.write(data[i]);
                input += (char)data[i];
            }
            input_packet = JSON.parse(input);

            if (JSON.typeof(input_packet) == "undefined")
            {
                Serial.println("Parsing input failed!");
                return;
            }
            outgoingMessage.SenderAddress = 255;
            outgoingMessage.TargetAddress = (int)input_packet["target"];
            outgoingMessage.KeyValuesNum = 1;
            outgoingMessage.KeyValue[0].Key = (int)input_packet["key"];
            outgoingMessage.KeyValue[0].Value = (int)input_packet["value"];
            newDataFromWeb = true;

            request->send(200);
        });

    events.onConnect([](AsyncEventSourceClient *client)
                     {
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
    newDataFromESP = 1;
}

/* Prints the incoming message from a node to the Serial port */
void printReceivedData()
{
    Serial.printf("Target address: %u \n", incomingMessage.TargetAddress);
    //Serial.printf("Sender address: %u \n", incomingMessage.SenderAddress);
    //Serial.printf("Reading ID: %u \n", incomingMessage.readingID);
    //Serial.printf("Number of KeyValue pairs: %d \n", incomingMessage.KeyValuesNum);

    for (int i = 0; i < incomingMessage.KeyValuesNum; i++)
    {
        Serial.printf("%u: Key : %u , Value: %u \n", i, incomingMessage.KeyValue[i].Key, incomingMessage.KeyValue[i].Value);
    }

    Serial.println();
}

void loop()
{
#if defined(WEBSERVER)

    static unsigned long lastEventTime = millis();
    static const unsigned long EVENT_INTERVAL_MS = 5000;

    /* Packets are sent to the server in interval of EVENT_INTERVAL_MS */
    if ((millis() - lastEventTime) > EVENT_INTERVAL_MS)
    { 
        if (newDataFromESP)
        {
            JSONVar json_packet;
            /* Search the structure for the sender address of incoming message.
           If device is already in structure, just update values */
            for (int i = 0; i < incomingMessage.KeyValuesNum; i++)
            {
                bool is_found = false;
                for (unsigned int j = 0; j < sizeof(packets) / sizeof(Packet); j++)
                {
                    if (incomingMessage.SenderAddress == packets[j].SenderAddress &&
                        incomingMessage.KeyValue[i].Key == packets[j].Key)
                    {
                        packets[j].ID = String(String(incomingMessage.SenderAddress) + String(incomingMessage.KeyValue[i].Key)).toInt();
                        packets[j].readingId = incomingMessage.readingID;
                        packets[j].Value = incomingMessage.KeyValue[i].Value;
                        is_found = true;
                    }
                }
                if(!is_found)
                {
                    Serial.println(incomingMessage.SenderAddress);
                    Serial.println(incomingMessage.TargetAddress);
                    packets[index_packet].ID = String(String(incomingMessage.SenderAddress) + String(incomingMessage.KeyValue[i].Key)).toInt();
                    packets[index_packet].SenderAddress = incomingMessage.SenderAddress;
                    packets[index_packet].readingId = incomingMessage.readingID;
                    packets[index_packet].Key = incomingMessage.KeyValue[i].Key;
                    packets[index_packet].Value = incomingMessage.KeyValue[i].Value;
                    index_packet == 4 ? index_packet = 0 : index_packet++;
                }
            }

            int json_index = 0;
            for (unsigned int i = 0; i < sizeof(packets) / sizeof(Packet); i++)
            {
                if (packets[i].SenderAddress != 0)
                {
                    json_packet[json_index]["ID"] = (int)packets[i].ID;
                    json_packet[json_index]["sender"] = (int)packets[i].SenderAddress;
                    json_packet[json_index]["readingId"] = (int)packets[i].readingId;
                    json_packet[json_index]["key"] = (int)packets[i].Key;
                    json_packet[json_index]["value"] = (int)packets[i].Value;
                    json_index++;
                }
            }
            json_string = JSON.stringify(json_packet);
            Serial.println(F("Data received and added to packets!"));
            newDataFromESP = 0;
            Serial.println(json_packet);
        }

        events.send(json_string.c_str(), "new_readings", millis());
        Serial.println(F("New readings sent!"));
        lastEventTime = millis();
        Serial.println(ESP.getFreeHeap());
    }

    /* If there is an update from the server, send it to the node */
    if (newDataFromWeb == true)
    {
        outgoingMessage.readingID = msg_count_snt;
        msg_count_snt++;
        esp_now_send(broadcastAddress, (uint8_t *)&outgoingMessage,
        sizeof(outgoingMessage) - sizeof(outgoingMessage.KeyValue) + (outgoingMessage.KeyValuesNum) * sizeof(KeyValue_t));
        newDataFromWeb = false;
    }

#else

    static unsigned long lastEventTime = millis();
    static const unsigned long EVENT_INTERVAL_MS = 3000;
    if ((millis() - lastEventTime) > EVENT_INTERVAL_MS)
    {
        // Set the sender and target addresses in message packet
        String addr = WiFi.macAddress().substring(15);
        memcpy(&outgoingMessage.SenderAddress, &addr, sizeof(addr));
        memcpy(&outgoingMessage.TargetAddress, &broadcastAddress[5], sizeof(broadcastAddress[5]));
        outgoingMessage.readingID = msg_count_snt;
        msg_count_snt++;
        outgoingMessage.KeyValuesNum = 2;
        outgoingMessage.KeyValue[0].Key = KEY_LIGHTSTATE_I;
        outgoingMessage.KeyValue[0].Value = 1; //example sensor data
        outgoingMessage.KeyValue[1].Key = KEY_AIR_COND_SWITCH_I;
        outgoingMessage.KeyValue[1].Value = 0; //example sensor data

        esp_now_send(broadcastAddress, (uint8_t *)&outgoingMessage,
                     sizeof(outgoingMessage) - sizeof(outgoingMessage.KeyValue) + (outgoingMessage.KeyValuesNum) * sizeof(KeyValue_t));
        lastEventTime = millis();
    }
    
    if (newDataFromESP)
    {
        Serial.println("Data received from webserver:");
        Serial.println(incomingMessage.KeyValue[0].Key);
        Serial.println(incomingMessage.KeyValue[0].Value);
        newDataFromESP = false;
    }
#endif
}
