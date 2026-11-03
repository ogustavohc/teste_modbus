/*
    Based on ModbusIP_ESP8266 by André Sarmento Barbosa (2015).
    Copyright (C) 2015 André Sarmento Barbosa
    Distributed under the BSD 3-Clause License; see LICENSE.
*/

#ifndef MODBUSIP_ESP8266_H
#define MODBUSIP_ESP8266_H

#include <Modbus.h>
#include <ESP8266WiFi.h>

#define MODBUSIP_PORT 502
#define MODBUSIP_MAXFRAME 200

class ModbusIP : public Modbus {
private:
    byte _MBAP[7];
    WiFiClient _client;
    bool _hasClient;
    uint8_t _rxBuffer[MODBUSIP_MAXFRAME + 7];
    size_t _rxLength;

    void closeClient(const char* reason);
    bool processRequest(size_t frameLength);

public:
    ModbusIP();
    void config(const char* ssid, const char* password);
    void task();
};

#endif
