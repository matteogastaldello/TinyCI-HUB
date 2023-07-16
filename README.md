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
