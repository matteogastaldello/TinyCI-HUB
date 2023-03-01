#include <Arduino.h>
// MQTT Libraries
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <tcpUtils.hpp>

#define SA struct sockaddr

struct Device
{
  char ip[MAX_IPLEN];
  String deviceName;
  Device()
  {
    deviceName = "";
  }
  Device(const char *_ip, String _deviceName)
  {
    strcpy(ip, _ip);
    deviceName = _deviceName;
  }
};

char outBuf[50];
Device devicesList[MAX_DEVICES];
int nDevices = 0;

// DynamicJsonDocument doc(200);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

char mqttServer[20] = "18.197.96.236";
const char *ssid = "MspNet";
const char *password = "mspmatteo";
String MODE = "default";

void messageCallback(char *topic, byte *payload, unsigned int length);
void mqttReconnect();
void discoveryMode(String deviceName);
int setMode(String deviceName, char *params, int params_len);

// MQTT FUNCTIONS: message callback
void messageCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char strPayload[length + 1];
  for (int i = 0; i < length; i++)
  {
    strPayload[i] = (char)payload[i];
  }
  strPayload[length] = '\0';

  StaticJsonDocument<1024> doc;
  // memcpy(strPayload, payload, length);
  Serial.println(strPayload);
  deserializeJson(doc, strPayload);
  const char *mode = doc["mode"];
  const char *device = doc["device"];
  // const char* message = doc["msg"].as<const char*>();

  if (strcmp(mode, "discovery") == 0)
  {
    Serial.println("Discovery Mode...");
    mqttClient.unsubscribe("msp-outTopic");
    mqttClient.publish("msp-outTopic", "Entering Discovery Mode! - esp32");
    discoveryMode(device);
    mqttClient.subscribe("msp-outTopic");
  }
  else if (strcmp(mode, "set") == 0)
  {
    Serial.println("Set Mode...");
    mqttClient.unsubscribe("msp-outTopic");
    mqttClient.publish("msp-outTopic", "Entering Set Mode! - esp32");
    mqttClient.subscribe("msp-outTopic");
    if (setMode(device, strPayload, length + 1) < 0)
    {
      Serial.println("Impossible to set mode");
      mqttClient.unsubscribe("msp-outTopic");
      mqttClient.publish("msp-outTopic", "Impossible to set Mode - esp32");
      mqttClient.subscribe("msp-outTopic");
    }
  }
  else
  {
    Serial.println("No msg-mode selected!");
    Serial.println(strPayload);
  }
  // Serial.println("name = " + String(name) + "\ncar = " + String(car));
}
// MQTT FUNCTIONS: message reconnect on down
void mqttReconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("arduinoClient"))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("msp-outTopic", "hello world");
      Serial.print("State: ");
      Serial.println(mqttClient.state());
      // ... and resubscribe
      mqttClient.subscribe("msp-outTopic");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
// TCP DISCOVERY FUNCTION
void discoveryMode(String deviceName)
{
  char ip[MAX_IPLEN];
  String str_ip = deviceDiscovery(30, 40);
  str_ip.toCharArray(ip, MAX_IPLEN, 0);
  if (str_ip != "error")
  {
    devicesList[nDevices++] = Device(ip, deviceName);
  }
  for (int i = 0; i < nDevices; i++)
  {
    Serial.print(devicesList[i].ip);
    Serial.println(devicesList[i].deviceName);
  }
}
// SET MODE FUNCTION
int setMode(String deviceName, char *params, int params_len)
{
  int position;
  Serial.println(params_len);
  // find the position of "deviceName", if is not found position is "nDevices"
  for (position = 0; position < nDevices && devicesList[position].deviceName != deviceName; position++);
  if (position != nDevices)
  {
    if (params_len != 0)
    {
      // try to send message 3 times (on error, <0) then return error
      int times;
      for (times = 0; times < 3; times++)
      {
        if (sendTCPMessage(devicesList[position].ip, params, params_len) >= 0){
          Serial.println("Message Sent");
          return 0;
        }
      }
      return -2; // impossible to send message
    }
  }
  return -1; // device not found
}

void setup()
{
  Serial.begin(115200);
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(messageCallback);
  delay(1500);
}

void loop()
{
  if (mqttClient.state() != MQTT_CONNECTED)
  {
    // Serial.print("State: ");
    // Serial.println(mqttClient.state());
    mqttReconnect();
  }
  // Serial.print("State: ");
  // Serial.println(mqttClient.state());
  mqttClient.loop();
}
