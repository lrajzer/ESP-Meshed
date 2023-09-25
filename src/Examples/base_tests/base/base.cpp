// #define BASE_BASE

#ifdef BASE_BASE

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
    Serial.print(" data: ");
    Serial.print(received->temp);
    Serial.print(" ");
    Serial.print(received->hum);
    Serial.print(" ");
    Serial.println(received->light);
}

uint16_t base_addr = 0b101000010011;

ESPMeshedNode *node = nullptr;
void setup()
{
    Serial.begin(115200);
    Serial.println("Starting base station");
    node = GetESPMeshedNode(base_addr, receiveHandler);
    node->setCleanupTime(3);
    delay(3000);
#ifdef DEBUG_ESP_MESHED
    node->print_self();
#endif
    delay(3000);
}

uint32_t last = 0;

void loop()
{
    if (millis() - last >= 10000)
    {
        Serial.println("Looping... w8ing for data");
        last = millis();
    }
    node->handleInLoop();
}
#endif