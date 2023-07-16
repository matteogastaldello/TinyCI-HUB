#include <Arduino.h>
// MQTT Libraries
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
void discoveryMode(String deviceName, JsonDocument &doc, char *responseBuf, int responseLen);
void checkConfiguration();
int setMode(String deviceName, char *params, int params_len);
int getMode(String deviceName, char *responseBuf, int responseLen);
String deviceDiscovery(int ip_start, int ip_end, JsonDocument &doc, char *responseBuf, int responseLen);

boolean checkValidJson(JsonDocument &doc, const String props[], int props_len)
{
  Serial.println("Checking json...");
  serializeJson(doc, Serial);
  for (int i = 0; i < props_len; i++)
  {
    Serial.println(props[i]);
    if (doc.containsKey(props[i]) == false)
      return false;
  }
  return true;
}
// MQTT FUNCTIONS: message callback
void messageCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.println("Callback");
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
  Serial.println(strPayload_backup);
  // check if json is valid for communication topic
  if (checkValidJson(doc, validProps, 2))
  {
    const char *mode = doc["mode"];
    const char *device = doc["device"];

    if (strcmp(mode, "discovery") == 0)
    {
      Serial.println("Discovery Mode...");
      mqttClient.unsubscribe(mqttTopicCommunication);
      char responseBuf[MAX];
      discoveryMode(device, doc, responseBuf, sizeof(responseBuf));
      mqttClient.subscribe(mqttTopicCommunication);
    }
    else if (strcmp(mode, "set") == 0)
    {
      Serial.println("Set Mode...");
      mqttClient.unsubscribe(mqttTopicCommunication);
      mqttClient.subscribe(mqttTopicCommunication);
      Serial.println(strPayload_backup);
      Serial.println(strPayload);
      if (setMode(device, strPayload_backup, length + 1) < 0)
      {
        Serial.println("Impossible to set mode");
        mqttClient.unsubscribe(mqttTopicCommunication);
        mqttClient.subscribe(mqttTopicCommunication);
      }
    }
    else if (strcmp(mode, "get") == 0)
    {
      Serial.println("Get Mode...");
      char responseBuf[MAX];
      if (getMode(device, responseBuf, sizeof(responseBuf)) < 0)
      {
        Serial.println("Impossible to get mode");
        mqttClient.unsubscribe(mqttTopicCommunication);
        doc["success"] = false;
        char stringJson[500];
        serializeJson(doc, stringJson, sizeof(stringJson));
        mqttClient.publish(mqttTopicCommunication, stringJson);
        mqttClient.subscribe(mqttTopicCommunication);
      }
      else{
        Serial.println("Impossible to get mode");
        mqttClient.unsubscribe(mqttTopicCommunication);
        mqttClient.publish(mqttTopicCommunication, responseBuf);
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
  if (checkValidJson(doc, validProps, 2))
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
      mqttClient.unsubscribe(mqttTopicCommunication);
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
void discoveryMode(String deviceName, JsonDocument &doc, char *responseBuf, int responseLen)
{
  String str_out = deviceDiscovery(100, 105, doc, responseBuf, responseLen);
  Serial.println(str_out);
  deserializeJson(doc, str_out);
  String ipS = doc["ip"];
  if (str_out != "error")
  {
    devicesList[nDevices++] = Device(ipS.c_str(), doc["device-name"]);
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
        char responseBuf[MAX];
        if (sendAndReceiveMessage(devicesList[position].ip, params, 1024, responseBuf, sizeof(responseBuf)) >= 0)
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

// GET MODE FUNCTION
int getMode(String deviceName, char *responseBuf, int responseLen)
{
  int position;
  // find the position of "deviceName", if is not found position is "nDevices"
  for (position = 0; position < nDevices && devicesList[position].deviceName != deviceName; position++)
    ;
  if (position != nDevices)
  {
    // try to send message 3 times (on error, <0) then return error
    int times;
    for (times = 0; times < 3; times++)
    {
      if (sendAndReceiveMessage(devicesList[position].ip, "get", 3, responseBuf, sizeof(responseBuf)) >= 0)
      {
        Serial.println("Message Sent: ");
        Serial.println("get");
        return 0;
      }
      Serial.println("impossible to send message");
      return -2; // impossible to send message
    }
  }
  Serial.println("device not found");
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
// Discovery function used to poll the network to find a device that is listening.
// This function make connection request to all ip from ip_start to ip_end.
String deviceDiscovery(int ip_start, int ip_end, JsonDocument &doc, char *responseBuf, int responseLen)
{
  char outBuf[50];
  int sockfd, connfd;
  struct sockaddr_in servaddr, cli;
  String subIP;
  subIP = getSubnetString(WiFi.localIP().toString(), subIP);

  for (int i = ip_start; i < ip_end; i++)
  {
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
      Serial.println("socket creation failed...\n");
      exit(0);
    }
    bzero(&servaddr, sizeof(servaddr));
    char ip[MAX_IPLEN] = "";
    sprintf(ip, "%s.%d", subIP, i);
    sprintf(outBuf, "ip: %s\t", ip);
    Serial.print(outBuf);
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket
    int ra = connect_with_timeout(sockfd, (SA *)&servaddr, sizeof(servaddr), 2000);
    if (ra >= 0)
    {
      Serial.println("connected to the server...");
      // function for chat
      // const char msg[] = "HANDSHAKE_REQ";
      std::string mqttMessage;
      serializeJson(doc, mqttMessage);
      char responseBuf[MAX];
      Serial.print("Message:");
      Serial.println(mqttMessage.c_str());
      int rm = sendAndReceiveMessage(sockfd, mqttMessage.c_str(), 1024, responseBuf, sizeof(responseBuf));
      // close the socket
      close(sockfd);
      if (rm == 0)
      {
        deserializeJson(doc, responseBuf, responseLen);
        doc["ip"] = ip;
        String out; 
        serializeJson(doc, out);
        return out;
      }
    }
    else
    {
      sprintf(outBuf, "Addr not working... %d", ra);
      Serial.println(outBuf);
    }
    Serial.println("closing socket...\n");
    close(sockfd);
  }
  return "error";
}
void setup()
{
  sprintf(mqttTopicCommunication, "esp-%s", WiFi.macAddress().c_str());
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
  // mqttReconnect();
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