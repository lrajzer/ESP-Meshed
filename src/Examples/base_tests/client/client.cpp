#define BASE_CLIENT

#ifdef BASE_CLIENT
#include "esp-meshed.h"
#include <Arduino.h>

struct transceivedata
{
    uint8_t temp;
    uint16_t hum;
    uint8_t light;
};

void receiveHandler(uint8_t *data, uint8_t len, uint16_t from)
{
    transceivedata *received = (transceivedata *)data;
    Serial.print("RECEIVED data from: ");
    Serial.print(from);
    Serial.print("data: ");
    Serial.print(received->temp);
    Serial.print(" ");
    Serial.print(received->hum);
    Serial.print(" ");
    Serial.println(received->light);
}

transceivedata GenDataNormal()
{
    transceivedata data;
    data.temp = 1;
    data.hum = 2;
    data.light = 3;
    return data;
}

transceivedata GenDataRand()
{
    transceivedata data;
    data.temp = random(0, 255);
    data.hum = random(0, 65535);
    data.light = random(0, 255);
    Serial.print("SENDING data: ");
    Serial.print(data.temp);
    Serial.print(" ");
    Serial.print(data.hum);
    Serial.print(" ");
    Serial.println(data.light);
    return data;
}

uint16_t base_addr = 0b101000010011;

ESPMeshedNode *node = nullptr;
void setup()
{
    Serial.begin(115200);
    Serial.println("Starting slave");
    uint16_t self_adr = WiFi.macAddress().toInt();
    node = GetESPMeshedNode(self_adr, receiveHandler);
    node->setCleanupTime(3);
    delay(3000);
#ifdef DEBUG_ESP_MESHED
    node->print_self();
#endif
    transceivedata data = GenDataNormal();
    delay(3000);
}

void loop()
{
    delay(10000);
    Serial.println("Looping...");
    transceivedata data = GenDataRand();
    node->sendMessage((uint8_t *)&data, sizeof(data), base_addr);
    node->handleInLoop();
}
#endif