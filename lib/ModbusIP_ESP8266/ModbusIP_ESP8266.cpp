/*
    Based on ModbusIP_ESP8266 by André Sarmento Barbosa (2015).
    Copyright (C) 2015 André Sarmento Barbosa
    Distributed under the BSD 3-Clause License; see LICENSE.
*/

#include "ModbusIP_ESP8266.h"
#include <string.h>

namespace {
WiFiServer server(MODBUSIP_PORT);
}

ModbusIP::ModbusIP() : _hasClient(false), _rxLength(0) {
    _frame = nullptr;
}

void ModbusIP::config(const char* ssid, const char* password) {
    WiFi.begin(ssid, password);
    server.begin();
}

void ModbusIP::closeClient(const char* reason) {
    if (_hasClient) {
        Serial.print(F("[Modbus] Conexão TCP encerrada: "));
        Serial.println(reason);
    }

    _client.stop();
    _hasClient = false;
    _rxLength = 0;
}

bool ModbusIP::processRequest(size_t frameLength) {
    const uint16_t transactionId =
        (static_cast<uint16_t>(_rxBuffer[0]) << 8) | _rxBuffer[1];
    const uint8_t unitId = _rxBuffer[6];
    const uint8_t functionCode = _rxBuffer[7];
    const uint16_t pduLength =
        (static_cast<uint16_t>(_rxBuffer[4]) << 8) | _rxBuffer[5];

    if (_rxBuffer[2] != 0 || _rxBuffer[3] != 0) {
        Serial.println(F("[Modbus] Requisição descartada: protocolo MBAP inválido"));
        closeClient("protocolo MBAP inválido");
        return false;
    }

    const size_t requestPduLength = pduLength - 1;
    if (requestPduLength < 5 ||
        ((functionCode == MB_FC_WRITE_COILS || functionCode == MB_FC_WRITE_REGS) &&
         requestPduLength < 6)) {
        Serial.printf(
            "[Modbus] Requisição descartada: PDU muito curto (%u bytes)\n",
            static_cast<unsigned int>(requestPduLength));
        closeClient("PDU muito curto");
        return false;
    }

    memcpy(_MBAP, _rxBuffer, sizeof(_MBAP));
    _len = static_cast<byte>(requestPduLength);
    _frame = static_cast<byte*>(malloc(_len));
    if (_frame == nullptr) {
        Serial.println(F("[Modbus] Falha ao alocar memória para a requisição"));
        closeClient("sem memória para a requisição");
        return false;
    }
    memcpy(_frame, _rxBuffer + sizeof(_MBAP), _len);

    Serial.printf(
        "[Modbus] Requisição: tx=%u unit=%u função=0x%02X PDU=%u bytes\n",
        static_cast<unsigned int>(transactionId),
        static_cast<unsigned int>(unitId),
        static_cast<unsigned int>(functionCode),
        static_cast<unsigned int>(_len));

    receivePDU(_frame);

    if (_reply != MB_REPLY_OFF) {
        const uint16_t responseLength = static_cast<uint16_t>(_len) + 1;
        _MBAP[4] = static_cast<byte>(responseLength >> 8);
        _MBAP[5] = static_cast<byte>(responseLength & 0xFF);

        uint8_t txBuffer[MODBUSIP_MAXFRAME + 7];
        const size_t txLength = static_cast<size_t>(_len) + sizeof(_MBAP);
        memcpy(txBuffer, _MBAP, sizeof(_MBAP));
        memcpy(txBuffer + sizeof(_MBAP), _frame, _len);

        size_t bytesSent = 0;
        while (bytesSent < txLength && _client.connected()) {
            const size_t written = _client.write(txBuffer + bytesSent, txLength - bytesSent);
            if (written == 0) {
                break;
            }
            bytesSent += written;
        }

        if (bytesSent != txLength) {
            Serial.printf(
                "[Modbus] Falha ao enviar resposta: %u de %u bytes\n",
                static_cast<unsigned int>(bytesSent),
                static_cast<unsigned int>(txLength));
            free(_frame);
            _frame = nullptr;
            closeClient("falha no envio");
            return false;
        }

        Serial.printf(
            "[Modbus] Resposta enviada: tx=%u função=0x%02X (%u bytes)\n",
            static_cast<unsigned int>(transactionId),
            static_cast<unsigned int>(functionCode),
            static_cast<unsigned int>(txLength));
    } else {
        Serial.printf(
            "[Modbus] Requisição sem resposta: tx=%u função=0x%02X\n",
            static_cast<unsigned int>(transactionId),
            static_cast<unsigned int>(functionCode));
    }

    free(_frame);
    _frame = nullptr;
    _len = 0;

    const size_t remaining = _rxLength - frameLength;
    if (remaining > 0) {
        memmove(_rxBuffer, _rxBuffer + frameLength, remaining);
    }
    _rxLength = remaining;
    return true;
}

void ModbusIP::task() {
    if (WiFi.status() != WL_CONNECTED) {
        if (_hasClient) {
            closeClient("Wi-Fi desconectado");
        }
        return;
    }

    if (_hasClient && !_client.connected()) {
        closeClient("cliente desconectado");
    }

    if (!_hasClient) {
        WiFiClient incomingClient = server.accept();
        if (!incomingClient) {
            return;
        }

        _client = incomingClient;
        _hasClient = true;
        Serial.print(F("[Modbus] Cliente TCP conectado: "));
        Serial.print(_client.remoteIP());
        Serial.print(':');
        Serial.println(_client.remotePort());
    }

    while (_client.available() > 0) {
        if (_rxLength >= sizeof(_rxBuffer)) {
            Serial.println(F("[Modbus] Requisição descartada: buffer cheio"));
            closeClient("buffer cheio");
            return;
        }

        const int nextByte = _client.read();
        if (nextByte < 0) {
            break;
        }
        _rxBuffer[_rxLength++] = static_cast<uint8_t>(nextByte);
    }

    while (_rxLength >= 7) {
        const uint16_t mbapLength =
            (static_cast<uint16_t>(_rxBuffer[4]) << 8) | _rxBuffer[5];
        if (mbapLength < 2 || mbapLength > MODBUSIP_MAXFRAME + 1) {
            Serial.printf(
                "[Modbus] Requisição descartada: tamanho MBAP inválido (%u)\n",
                static_cast<unsigned int>(mbapLength));
            closeClient("tamanho MBAP inválido");
            return;
        }

        const size_t frameLength = 6 + mbapLength;
        if (_rxLength < frameLength) {
            break;
        }

        if (!processRequest(frameLength)) {
            return;
        }
    }
}
