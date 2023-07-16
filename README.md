# ESP32-HomeHub

"TIC" Hub is middleware hub for IoT is a transformative solution that bridges the gap between IoT devices and web platforms. By efficiently collecting data from sensors on IoT devices and transmitting it to a web platform for processing and analysis. Our solution permit to edge devices, with a simple and standard implementation of a web socket to communicate with our hub. 

## Requirements

### Hardware Requirements
 - 1 ESP32 (our solution is implemented on ESP32-WROOM-32 but the code is not board dependent)

   <img src="https://i.ibb.co/7gSXKgT/photo-2023-07-16-11-47-14.jpg" height="300">

### Software Requirements
For the development, in particular the compilation and the linting of the code are performed by [Plaform.IO](https://platformio.org/) (generation toolset for embedded C/C++ development, that can be used as a VSCode extension or with their IDE).
To run the software for the hub you need to install low number of libraries:
- **PubSubClient.h**: This library provides a client for doing simple publish/subscribe messaging with a server that supports MQTT. We use it to handle the communication (in both direction) between the hub and the web platform.
- **ArduinoJson.h**: JSON library, optimized for embdeed boards based on the Arduino platform

You can install the following libraries launching these commands:
```console
pio pkg install --library "knolleary/PubSubClient@^2.8" # MQTT Library
pio pkg install --library "bblanchon/ArduinoJson@^6.20.1" # To handle the Serialization and Deserialization of json
```

## Project Layout

```
├── README.md
├── include
│   └── README
├── lib
│   └── README
├── platformio.ini
├── src
│   ├── configuration.h
│   ├── debug.cfg
│   ├── debug_custom.json
│   ├── esp32.svd
│   ├── main.cpp
│   └── utils
│       ├── stringUtils.cpp
│       ├── stringUtils.hpp
│       ├── tcpUtils.cpp
│       └── tcpUtils.hpp
└── test
    └── README
```

## Getting Started

The only thing you need to do is to connect to your Wifi network setting the global variables specified here in "configuration.h"
```
const char *ssid = "MspNet";
const char *password = "mspmatteo";
```

Other parts of "configuration.h" refers to MQTT server configuration. This lines can be edited only if also the configuration of the web platform are edited accordingly!

## Code Highlights 

| Function | Description |
|   ---    |     ---     |
|`void messageCallback(char *topic, byte *payload, unsigned int length)`| Handle all MQTT payload that are published and redirect the request to the appropriate handler (`void handleConfigurationCallback`, `void handleCommunicationCallback`) |
|`void handleConfigurationCallback(char *topic, byte *payload, unsigned int length)`| Handle response when a payload is published on the defined MQTT configuration topic |
|`void handleCommunicationCallback(char *topic, byte *payload, unsigned int length)` | Handle response when a payload is published on the defined MQTT configuration topic |
|`int setMode(String deviceName, char *params, int params_len)`| If a json is published in the communication topic with `{"mode": "set"}` the hub redirect the request to `deviceName` (if the device was previously configured) that handle the request. |
|`int getMode(String deviceName, char *responseBuf, int responseLen)`| If a json is published in the communication topic with `{"mode": "get"}` the hub redirect the request to `deviceName` (if the device was previously configured) that handle the request (eg. reading sensonrs) and sends back to the response to the hub that immediately share the data with the web platform |
|`String deviceDiscovery(int ip_start, int ip_end, JsonDocument &doc, char *responseBuf, int responseLen)`| See code below. |
|`void discoveryMode(String deviceName, JsonDocument &doc, char *responseBuf, int responseLen)` | See code below |

- **Discovery Mode**: the code provided below is used to scan the entire ip subnet to auto register edge devices in to the hub to communicate with them later.

```C++
void discoveryMode(String deviceName, JsonDocument &doc, char *responseBuf, int responseLen)
{
  char ip[MAX_IPLEN];
  String str_out = deviceDiscovery(2, 254, doc, responseBuf, responseLen);
  Serial.println(str_out);
  deserializeJson(doc, str_out);
  String ipS = doc["ip"];
  if (str_out != "error")
  {
    devicesList[nDevices++] = Device(ipS.c_str(), doc["device-name"]);
  }
 ...
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
... logging ...
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port = htons(PORT);

    // connect the client socket to server socket in non blocking mode with timeout
    int ra = connect_with_timeout(sockfd, (SA *)&servaddr, sizeof(servaddr), 2000);
    if (ra >= 0)
    {
      Serial.println("connected to the server...");
      std::string mqttMessage;
      serializeJson(doc, mqttMessage);
      char responseBuf[MAX];
... logging ...
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
      ... logging error ...
    Serial.println("closing socket...\n");
    close(sockfd);
  }
  return "error";
}
```

The project also contains custom made libraries to handle web socket communication and string manipulation. The functions are documented in the relative files (`tcpUtils.cpp` and `stringUtils.cpp` in `src/utils/`)

| Function | Description |
|   ---    |     ---     |
|`int connect_with_timeout(int sockfd, const struct sockaddr *addr, socklen_t addrlen, unsigned int timeout_ms)`| Function used to connect to a server specified at addr. The operation abort after the time specified in timeout_ms |
|`int sendAndReceiveMessage(int sockfd, const char *msg, int msgSize, char * responseBuf, int responseLen)`| Function used to communicate with a server by websocket. The response is set in responseBuf|
|`sendAndReceiveMessage(const char *ip, const char *message, int message_len, char * responseBuf, int responseLen)`| the result is equals to "int sendAndReceiveMessage" but you can specify ip instead of sockfd. note: the sockfd is created in the function |

