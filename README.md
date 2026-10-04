# DIY Personal Stream Deck 🎛️

A custom hardware remote controller / macro deck built with ESP32 to remotely trigger actions and manage my personal [homelab](https://github.com/Benjpp/homelab). Not really a stream deck, a remote orchestrator, but looks like a stream deck, kinda.

![Demo](building :V)

## 🚀 Features

- **Homelab Control:** Direct integration with MQTT endpoints. A listener runs on a docker container on the target machine, listens for a command, and executes the designated task
- **Custom Macros:** Trigger Docker actions, scripts, and server routines. Still adding more stuff
- **Fast Communication:** Low-latency Wi-Fi connection.  10 TX antenna power set on ESP32C3 for more precise communication. 

## 🛠️️ Hardware

- **Microcontroller:** ESP32 Board.
- **Display / Controls:** Status LEDs (Red, Green, Blue) / Push Buttons.
- **Case:** Work in progress.

## ⚙️️ Dev Setup
# Prod-Listener
The target machine listener runs on a docker container. Said container has access to executing systems commands in the host side (maybe not best practice but comfortable for a homemade project). 
The mqtt client in the host machine is developed using the PAHO Mqtt library, and included in the Dockerfile used to generate said container.

# Esp32-Client
An ESP32C3 super mini is in charge of sending the orchestration commands. It is programmed in C/C++ using the oficial Espressif framework. The complete toolkit must be installed following the oficial docs from the Espressif webpage
To correctly secure WiFi connection the TX power must be lowered to the minum
value, 10, via the command:
```bash
idf.py menuconfig
```
Also, all possible secrets used in the project, for example WiFi SSID and Password, have been written in a Kconfig.projbuild file in order to correctly define the constants via the sdkconfig.h include file, thus not needing to upload
potential security risks to a public repo.

I have nothing more to say for know, bye!

