#include <Arduino.h>
// MQTT Libraries
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
// include this file to include utils to connect to local websocket server.
#include <utils/tcpUtils.hpp>
// include this file to include utils to manage strings
#include <utils/stringUtils.hpp>
// import this file to include wifi SSID/password and mqtt server url/topic
#include <configuration.h>

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

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void messageCallback(char *topic, byte *payload, unsigned int length);
void handleConfigurationCallback(char *topic, byte *payload, unsigned int length);
void handleCommunicationCallback(char *topic, byte *payload, unsigned int length);
void mqttReconnect();
void discoveryMode(String deviceName);
void checkConfiguration();
int setMode(String deviceName, char *params, int params_len);

boolean checkValidJson(DynamicJsonDocument doc, const String props[])
{
  for (int i = 0; i < props->length(); i++)
  {
    if (doc.containsKey(props[i]) == false)
      return false;
  }
  return true;
}

// MQTT FUNCTIONS: message callback
void messageCallback(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, mqttTopicConfig) == 0)
  {
    handleConfigurationCallback(topic, payload, length);
    return;
  }
  else if (strcmp(topic, mqttTopicCommunication) == 0)
  {
    handleCommunicationCallback(topic, payload, length);
    return;
  }
}

void handleCommunicationCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Communication message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  char strPayload[length + 1];
  char strPayload_backup[length + 1];
  byteToString(payload, length, strPayload);
  strcpy(strPayload_backup, strPayload);
  StaticJsonDocument<1024> doc;
  deserializeJson(doc, strPayload);
  String validProps[] = {"mode", "device"};
  // check if json is valid for communication topic
  if (checkValidJson(doc, validProps))
  {
    const char *mode = doc["mode"];
    const char *device = doc["device"];

    if (strcmp(mode, "discovery") == 0)
    {
      Serial.println("Discovery Mode...");
      mqttClient.unsubscribe(mqttTopicCommunication);
      mqttClient.publish(mqttTopicCommunication, "Entering Discovery Mode! - esp32");
      discoveryMode(device);
      mqttClient.subscribe(mqttTopicCommunication);
    }
    else if (strcmp(mode, "set") == 0)
    {
      Serial.println("Set Mode...");
      mqttClient.unsubscribe(mqttTopicCommunication);
      mqttClient.publish(mqttTopicCommunication, "Entering Set Mode! - esp32");
      mqttClient.subscribe(mqttTopicCommunication);
      Serial.println(strPayload_backup);
      Serial.println(strPayload);
      if (setMode(device, strPayload_backup, length + 1) < 0)
      {
        Serial.println("Impossible to set mode");
        mqttClient.unsubscribe(mqttTopicCommunication);
        mqttClient.publish(mqttTopicCommunication, "Impossible to set Mode - esp32");
        mqttClient.subscribe(mqttTopicCommunication);
      }
    }
    else
    {
      Serial.println("No msg-mode selected!");
      Serial.println(strPayload);
    }
  }
  else
  {
    Serial.println("Json not valid for communication.");
  }
}
void handleConfigurationCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Configuration Message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  // prepare payload to deserialization. byte to string
  char strPayload[length + 1];
  char strPayload_backup[length + 1];
  byteToString(payload, length, strPayload);
  strcpy(strPayload_backup, strPayload);
  StaticJsonDocument<1024> doc;
  Serial.println(strPayload);
  deserializeJson(doc, strPayload);
  String validProps[] = {"id", "status"};
  if (checkValidJson(doc, validProps))
  {
    const char *deviceId = doc["id"];
    const char *status = doc["status"];

    if (strcmp(deviceId, WiFi.macAddress().c_str()) == 0)
    {
      Serial.println("Device registered");
      mqttClient.unsubscribe(mqttTopicConfig);
      mqttClient.subscribe(mqttTopicCommunication);
    }
  }
  else
  {
    Serial.println("Json not valid for configuration.");
  }
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
      // mqttClient.publish(mqttTopicCommunication, "hello world");
      Serial.print("State: ");
      Serial.println(mqttClient.state());
      // ... and resubscribe
      mqttClient.subscribe(mqttTopicCommunication);
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
  for (position = 0; position < nDevices && devicesList[position].deviceName != deviceName; position++)
    ;
  if (position != nDevices)
  {
    if (params_len != 0)
    {
      // try to send message 3 times (on error, <0) then return error
      int times;
      for (times = 0; times < 3; times++)
      {
        if (sendMessage(devicesList[position].ip, params, params_len) >= 0)
        {
          Serial.println("Message Sent: ");
          Serial.println(params);
          return 0;
        }
      }
      return -2; // impossible to send message
    }
  }
  return -1; // device not found
}

void checkConfiguration()
{
  Serial.println("checking configuration...");
  StaticJsonDocument<256> jsonResponse;
  jsonResponse["device-name"] = deviceName;
  jsonResponse["id"] = WiFi.macAddress();
  char stringJson[500];
  serializeJson(jsonResponse, stringJson, sizeof(stringJson));
  Serial.println(stringJson);
  mqttClient.publish(mqttTopicConfig, stringJson);
  mqttClient.subscribe(mqttTopicConfig);
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

  delay(500);
  while (mqttClient.state() != MQTT_CONNECTED)
  {
    mqttReconnect();
  }
  checkConfiguration();
}

void loop()
{
  if (mqttClient.state() != MQTT_CONNECTED)
  {
    mqttReconnect();
  }
  mqttClient.loop();
}