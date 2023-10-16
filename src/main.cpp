#include <Arduino.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

const char *serverAPI = "203.194.112.238:3550";
const char *resource = "/downlink/?gatewayId=MELGL0002";
const unsigned long HTTP_TIMEOUT = 10000; // max respone time from serverAPI
const size_t MAX_CONTENT_SIZE = 2000;

uint64_t chipID = ESP.getEfuseMac();
String dataTelnet;
String simpanData;
String stringValue1;
String stringValue2;
int hitungSimpan = 0;
int16_t the_int = 0;

bool kondisi = 0;
bool simpan = 0;

String long_chipID = "MEL_" + String(chipID);

const unsigned long periodStatus = 30000;
const unsigned long periodDownlink = 8000;

const char *mqtt_server = "203.194.112.238";
const char *mqtt_password = "mgi2022";
const char *mqtt_username = "das";
const char *id_gateway = "MELGL0002";

bool internet = false;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 100);
IPAddress subnet(255, 255, 255, 0);
IPAddress myDns(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
// #define MYIPADDR 192, 168, 1, 28
// #define MYIPMASK 255, 255, 255, 0
// #define MYDNS 192, 168, 1, 1
// #define MYGW 192, 168, 1, 1

const int configLora = 5;
const int pinM1 = 25;
String topicGateway;
String topicNode;

EthernetServer server(23);
boolean alreadyConnected = false;

EthernetClient espClient;
EthernetClient apiClient;

PubSubClient client(espClient);

void reconnect()
{
  static uint32_t mqttReconnect = millis();
  if (!client.connected())
  {
    if ((millis() - mqttReconnect) > 2000)
    {
      mqttReconnect = millis();

      client.connected();
      Serial.println("Connecting to MQTT...");

      if (client.connect(long_chipID.c_str(), mqtt_username, mqtt_password))
      {

        Serial.println("MQTT connected");
      }
      else
      {
        Serial.print("failed with state ");
        Serial.print(client.state());
      }
    }
  }
}

void connectmqtt()
{
  client.connect(long_chipID.c_str()); // ESP will connect to mqtt broker with clientID
  {
    Serial.println("connected to MQTT");
    internet = true;

    if (!client.connected())
    {
      reconnect();
      internet = false;
    }
  }
}

void setup()
{
  topicGateway = "i-DasWmeter/Gateway/" + String(id_gateway) + "/Event/Up";
  topicNode = "i-DasWmeter/Node/" + String(id_gateway) + "/Event/Up";

  Ethernet.init(4);
  server.begin();

  Serial.begin(19200);
  Serial1.begin(9600, SERIAL_8N1, 16, 17);
  Serial2.begin(19200, SERIAL_8N1, 16, 17);
  // Serial2.begin(19200);
  pinMode(configLora, OUTPUT);
  pinMode(pinM1, OUTPUT);
  digitalWrite(configLora, LOW);
  digitalWrite(pinM1, LOW);

  // delay(10000);
  Serial.print("Chip ID : ");
  Serial.println(long_chipID);

  if (Ethernet.begin(mac))
  { // Dynamic IP setup
    Serial.println("DHCP OK!");
  }
  else
  {
    Ethernet.begin(mac, ip, myDns, gateway, subnet);
    Serial.println("Failed to configure Ethernet using DHCP");
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware)
    {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      // while (true)
      // {
      //   delay(1); // do nothing, no point running without Ethernet hardware
      // }
    }
    if (Ethernet.linkStatus() == LinkOFF)
    {
      Serial.println("Ethernet cable is not connected.");
    }
  }
  delay(5000);
  Serial.print("Local IP : ");
  Serial.println(Ethernet.localIP());
  Serial.print("Subnet Mask : ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("Gateway IP : ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS serverAPI : ");
  Serial.println(Ethernet.dnsServerIP());
  Serial.println("Ethernet Successfully Initialized");

  client.setServer(mqtt_server, 1883);
  // connectmqtt();
}

void gatewayStatus()
{
  static uint32_t statusTime_now = millis();
  if ((millis() - statusTime_now) > periodStatus)
  {
    statusTime_now = millis();

    StaticJsonDocument<192> doc;
    doc["gatewayID"] = String(id_gateway);
    doc["statusLAN"] = Ethernet.localIP();

    if (internet)
    {
      doc["statusInternet"] = "Connected ";
    }
    else
    {
      doc["statusInternet"] = "Not Connected";
    }

    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    client.publish(topicGateway.c_str(), buffer, n);
  }
}

void getDownlink()
{
  static uint32_t downlinkTime_now = millis();
  if ((millis() - downlinkTime_now) > periodDownlink)
  {
    downlinkTime_now = millis();
    apiClient.setTimeout(10000);
    if (!apiClient.connect("203.194.112.238", 3550))
    {
      Serial.println(F("Connection failed"));
      return;
    }

    apiClient.println(F("GET /downlink/?gatewayId=MELGL0001 HTTP/1.0"));
    apiClient.println(F("Host: 203.194.112.238"));
    apiClient.println(F("Connection: close"));
    if (apiClient.println() == 0)
    {
      Serial.println(F("Failed to send request"));
      apiClient.stop();
      return;
    }

    char endOfHeaders[] = "\r\n\r\n";
    if (!apiClient.find(endOfHeaders))
    {
      Serial.println(F("Invalid response"));
      apiClient.stop();
      return;
    }

    DynamicJsonDocument apiDoc(1536);

    DeserializationError error = deserializeJson(apiDoc, apiClient);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      apiClient.stop();
      return;
    }

    int list = apiDoc["list"];
    if (list > 0)
    {
      serializeJson(apiDoc, Serial2);
    }

    apiClient.stop();
  }
}

void getUplink()
{

  if (Serial2.available() > 0)
  {
    StaticJsonDocument<350> dataUplink;
    DeserializationError errorUplink = deserializeJson(dataUplink, Serial2);

    char bufferUplink[350];
    size_t nUplink = serializeJson(dataUplink, bufferUplink);

    if (errorUplink)
    {

      Serial.print("deserializeJson() Uplink failed: ");
      Serial.println(errorUplink.c_str());
      // return;
    }
    else
    {
      Serial.println("Upload");
      // Serial.println(dataSerial2);
      const char *nodeID = dataUplink["nodeID"];
      String dataTopic = "i-DasWmeter/Node/" + String(nodeID) + "/Event/Up";
      client.publish(dataTopic.c_str(), bufferUplink, nUplink);
    }
  }
}

void telnetHandle()
{
  EthernetClient clientTelnet = server.available();
  if (clientTelnet)
  {
    if (!alreadyConnected)
    {
      // clear out the input buffer:
      clientTelnet.flush();
      server.println("Configurasi Gateway");
      server.print("EUI :");
      server.println(id_gateway);
      alreadyConnected = true;
    }

    if (clientTelnet.available() > 0)
    {
      // read the bytes incoming from the client:
      String thisChar = clientTelnet.readString();
      dataTelnet = thisChar;
      // thisChar.trim();

      // echo the bytes back to the client:
      // server.println(thisChar);
      if (thisChar == "amin\r\n")
      {
        // server.print(id_gateway);
        byte message[] = {0xC0, 0x00, 0x04, 0x12, 0x34, 0x00, 0x61};

        // server.prinf("%02X", "12648452");
        Serial1.write(message, sizeof(message));
      }
      // thisChar = "";
      // clientTelnet.flush();
    }
    if (Serial1.available())
    {
      Serial.printf("%02X", Serial2.read());
    }
  }
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }

  if (Ethernet.linkStatus() == LinkOFF)
  {
    Serial.println("Ethernet cable is not connected.");
  }

  client.loop();
  gatewayStatus();
  getUplink();
  getDownlink();
  telnetHandle();
}