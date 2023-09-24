#include "esp-meshed.h"
#include <Arduino.h>

struct transceivedata
{
    uint8_t temp;
    uint16_t hum;
    uint8_t light;
};

void receiveHandler(uint8_t *data, uint8_t len)
{
    transceivedata *received = (transceivedata *)data;
    Serial.print("RECEIVED data: ");
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

uint16_t peer_adr = 0b001000010011;
uint16_t self_adr = 0b111101010111;

ESPMeshedNode *node = nullptr;
void setup()
{
    Serial.begin(115200);
    Serial.println("Starting");
    node = GetESPMeshedNode(self_adr, receiveHandler);
    node->setCleanupTime(3);
    delay(3000);
#ifdef DEBUG_ESP_MESHED
    node->print_self();
#endif
    transceivedata data = GenDataNormal();
    node->sendMessage((uint8_t *)&data, sizeof(data), peer_adr);
    delay(3000);
}

void loop()
{
    delay(1000);
    Serial.println("Looping... w8ing for data");
    node->handleInLoop();
    transceivedata data = GenDataRand();
    node->sendMessage((uint8_t *)&data, sizeof(data), peer_adr);
}
