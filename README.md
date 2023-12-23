# openGRO-EnvironmentController 

<!-- ABOUT THE PROJECT -->
## About The Project

openGRO-EnvironmentController is device firmware for a custom piece of hardware consisting of an ESP32 microcontroller, which controls a set of relay outputs using an MCP20137 I2C GPIO expander.  It could easily be adapted for use with different outputs.  The firmware subscribes to configuration topics over MQTT, stores the config data in NVS (Non Voltatile Storage,) and executes and multi-stage control algorithm to decide the state of the outputs.

This project is build on top of the [ESP-IDF](https://github.com/espressif/esp-idf), and should be compiled using the tools found in that repository.  The MCP23017 driver comes from the excellent [ESP-IDF-LIB](https://github.com/UncleRus/esp-idf-lib)
