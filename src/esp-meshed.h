#include <Arduino.h>
#include <vector>
#include <esp_now.h>
#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

// TODO: ADD SLAVE BROADCAST

void _handlePackage(const uint8_t *mac, const uint8_t *incoming, int len)
{
#ifdef DEBUG_ESP_MESHED
    Serial.println("Handling package");
#endif
}

#ifdef DEBUG_ESP_MESHED

String bin(const uint16_t data)
{
    uint16_t data_copy = data;
    String out = "";
    for (uint8_t i = 0; i < 16; i++)
    {
        if (i % 8 == 0)
        {
            out += " ";
        }
        out += String((data_copy & 0b1000000000000000) >> 15, BIN);
        data_copy = data_copy << 1;
    }
    return out;
}

String bin(const uint8_t data)
{
    String out = "";
    uint8_t data_copy = data;
    for (uint8_t i = 0; i < 8; i++)
    {
        out += String((data_copy & 0b10000000) >> 7, BIN);
        data_copy = data_copy << 1;
    }
    return out;
}

String bin(const uint8_t *data, uint8_t len)
{
    String out = "";
    for (uint8_t i = 0; i < len; i++)
    {
        out += bin(data[i]) + "|";
    }
    return out;
}

String bin(const uint16_t *data, uint8_t len)
{
    String out = "";
    for (uint8_t i = 0; i < len; i++)
    {
        out += bin(data[i]) + "|";
    }
    return out;
}

#endif

class ESPMeshedNode
{
private:
    static ESPMeshedNode *ESPMeshedNodeInstance;
    uint16_t _nodeId;
    std::vector<std::pair<uint16_t, uint32_t>> _message_id_buffer;
    uint8_t _cleanup_time = 5; // Cleanup time in seconds
    esp_now_peer_info_t _peerInfo = {};
    uint8_t _broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    void (*_receiveHandler)(uint8_t *data, uint8_t len, uint16_t sender) = NULL;
    uint16_t getIDfromHeader(const uint8_t *header)
    {
        return uint16_t(header[2] << 4 | (header[3] >> 4));
    }

    uint16_t getAdressfromHeader(const uint8_t *header)
    {
        return uint16_t((header[3] & 0b00001111) << 8 | header[4]);
    }

    uint16_t getMessageIdfromHeader(const uint8_t *header)
    {
        return uint16_t(header[0] << 8 | header[1]);
    }

    uint16_t getNodeIdfromHeader(const uint8_t *header)
    {
        return uint16_t(header[2] << 4 | (header[3] >> 4));
    }
    uint8_t *constructHeader(const uint16_t adress, bool ack)
    {
        uint8_t *header = new uint8_t[5];
        uint16_t message_id = 0b1100110010101011;
        if (ack)
        {
            if (message_id % 2 == 0)
            {
                message_id++;
            }
            else
            {
                message_id--;
            }
        }
        else
        {
            if (message_id % 2 == 0)
            {
                message_id--;
            }
            else
            {
                message_id++;
            }
        }
        // bits 0-14: semi random message id
        // bits 16-28: node id
        // bit 29-40: adress

        header[0] = (message_id >> 8) & 0xFF;
        header[1] = message_id & 0xFF;
        header[2] = (adress >> 4) & 0xFF;
        header[3] = ((adress << 4) & 0b11110000) | ((this->_nodeId >> 8) & 0b00001111);
        header[4] = (this->_nodeId >> 8) & 0xFF;
#ifdef DEBUG_ESP_MESHED
        Serial.print("Message id: ");
        Serial.println(bin(message_id));
        Serial.print("Node id: ");
        Serial.println(bin(this->_nodeId));
        Serial.print("Adress: ");
        Serial.println(bin(adress));
        Serial.print("Header: ");
        Serial.print(bin(header, 5));

#endif

        return header;
    }
    uint8_t _retransmit(const uint8_t *data, uint8_t len)
    {
#ifdef DEBUG_ESP_MESHED
        Serial.print("Message id: ");
        Serial.println(bin(getIDfromHeader((uint8_t *)data)));
        Serial.print("Node id: ");
        Serial.println(bin(getNodeIdfromHeader((uint8_t *)data)));
        Serial.print("Adress: ");
        Serial.println(bin(getAdressfromHeader((uint8_t *)data)));
        Serial.print("Header: ");
        Serial.println(bin((uint8_t *)data, 5));

#endif

        switch (esp_now_send(this->_broadcastAddress, data, len))
        {
        case ESP_OK:
            return 0;

        default:
            return 1;
        }
    }

    bool _wasRetransmited(const uint8_t *data, uint8_t len)
    {
        uint16_t message_id = uint16_t(data[0] << 8 | data[1]);
        for (uint8_t i = 0; i < this->_message_id_buffer.size(); i++) // Would use mutex but since this is only called from the receive callback it should be fine
        {
            if (this->_message_id_buffer[i].first == message_id)
            {
                return true;
            }
        }
        this->_message_id_buffer.push_back({message_id, ESP.getCycleCount()});
        return false;
    }

    void _vectCleaner()
    {
#ifdef DEBUG_ESP_MESHED
        Serial.println("Cleaning vector");
#endif
        if (this->_message_id_buffer.size() == 0)
        {
#ifdef DEBUG_ESP_MESHED
            Serial.println("Vector empty");
#endif
            return;
        }
        uint32_t current_time = ESP.getCycleCount();
        // get rough time
        uint32_t timeToClean = this->_cleanup_time * ESP.getCpuFreqMHz() * 1000;

        // #ifdef DEBUG_ESP_MESHED
        //         Serial.print("Current time: ");
        //         Serial.println(current_time);
        //         Serial.print("Time to clean: ");
        //         Serial.println(timeToClean);
        // #endif
        for (uint8_t i = 0; i < this->_message_id_buffer.size(); i++)
        {
            // #ifdef DEBUG_ESP_MESHED
            //             Serial.print("Message id: ");
            //             Serial.println(bin(this->_message_id_buffer[i].first));
            //             Serial.print("Message time: ");
            //             Serial.println(this->_message_id_buffer[i].second);
            // #endif

            if (current_time - this->_message_id_buffer[i].second > timeToClean)
            {
                this->_message_id_buffer.erase(this->_message_id_buffer.begin() + i);
            }
            else
            {
                return;
            }
        }

        return;
    }

    void _commonSetup();

    ESPMeshedNode()
    {
        this->_commonSetup();
        // get esp mac addr
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        this->_nodeId = (mac[0] << 11 | mac[4] << 7 | mac[2] >> 3) & 0x0FFF;
        this->_cleanup_time = 1;
        this->_receiveHandler = nullptr;
    };

public:
    static ESPMeshedNode *GetInstance();

    /**
     * @brief Use this to send messages through the mesh network
     *
     * @param data buffer containing the data to be sent
     * @param len length of the data buffer (max 45)
     * @param peer adress of the peer to send the message to (lower 12 bits are used)
     * @return uint8_t 0 if successfull, 1 if len > 45 or 2-8 if esp_now_send() returns an error:
     * 2: ESP_ERR_ESPNOW_NOT_INIT
     * 3: ESP_ERR_ESPNOW_ARG
     * 4: ESP_ERR_ESPNOW_INTERNAL
     * 5: ESP_ERR_ESPNOW_NO_MEM
     * 6: ESP_ERR_ESPNOW_NOT_FOUND
     * 7: ESP_ERR_ESPNOW_IF
     * 8: ESP_ERR_ESPNOW_NOT_SUPPORT
     *
     */
    uint8_t sendMessage(uint8_t *data, uint8_t len, uint16_t peer)
    {
        if (len > 245)
        {
            return 1;
        }
        uint8_t *header = this->constructHeader(peer, false);
        uint8_t *message = new uint8_t[len + 5];
        memcpy(message, header, 5);
        memcpy(message + 5, data, len);
        esp_err_t msg_sent = esp_now_send(this->_broadcastAddress, message, len + 5);
        delete[] header;
        delete[] message;
        switch (msg_sent)
        {
        case ESP_OK:
            return 0;
        case ESP_ERR_ESPNOW_NOT_INIT:
            return 2;
        case ESP_ERR_ESPNOW_ARG:
            return 3;
        case ESP_ERR_ESPNOW_INTERNAL:
            return 4;
        case ESP_ERR_ESPNOW_NO_MEM:
            return 5;
        case ESP_ERR_ESPNOW_NOT_FOUND:
            return 6;
        case ESP_ERR_ESPNOW_IF:
            return 7;
        default:
            return 8;
        }
    }

    void setReceiveHandler(void (*handler)(uint8_t *data, uint8_t len, uint16_t sender))
    {
        this->_receiveHandler = handler;
    }

    void setCleanupTime(uint8_t time)
    {
        this->_cleanup_time = time;
    }

    void setNodeId(uint16_t id)
    {
        this->_nodeId = id;
    }

    void handleInLoop()
    {
        this->_vectCleaner();
    }

#ifdef DEBUG_ESP_MESHED
    void print_self()
    {
        Serial.print("Node id: ");
        Serial.println(bin(this->_nodeId));
        Serial.print("Cleanup time: ");
        Serial.println(this->_cleanup_time);
    }
#endif

    ESPMeshedNode(ESPMeshedNode const &) = delete;
    void operator=(ESPMeshedNode const &) = delete;

    void _handlePackage(const uint8_t *mac, const uint8_t *incoming, int len)
    {
#ifdef DEBUG_ESP_MESHED
        Serial.println("Handling package");
#endif
        if (len < 5)
        {
#ifdef DEBUG_ESP_MESHED
            Serial.println("Package too short");
#endif
            return;
        }
        // Check if message was already received
        if (_wasRetransmited(incoming, len))
        {
#ifdef DEBUG_ESP_MESHED
            Serial.println("Message already received, ignoring");
#endif
            return;
        }
        // Check if message is for this node
        if (this->getIDfromHeader(incoming) != this->_nodeId)
        {
#ifdef DEBUG_ESP_MESHED
            Serial.println("Message not for this node, retransmitting");
#endif
            _retransmit(incoming, len);
            return;
        }
        this->_receiveHandler((uint8_t *)incoming + 5, len - 5, this->getNodeIdfromHeader(incoming));
    }
};
ESPMeshedNode *ESPMeshedNode::ESPMeshedNodeInstance = nullptr;

ESPMeshedNode *ESPMeshedNode::GetInstance()
{
    if (!ESPMeshedNodeInstance)
    {
        ESPMeshedNodeInstance = new ESPMeshedNode();
    }
    return ESPMeshedNodeInstance;
}

static void _handlePackageStatic(const uint8_t *mac, const uint8_t *incoming, int len)
{
    ESPMeshedNode::GetInstance()->_handlePackage(mac, incoming, len);
}

void ESPMeshedNode::_commonSetup()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK)
    {
#ifdef DEBUG_ESP_MESHED
        Serial.println("Error initializing ESP-NOW");
#endif
        return;
    }
    esp_now_register_recv_cb(_handlePackageStatic);
    memcpy(this->_peerInfo.peer_addr, this->_broadcastAddress, 6);
    this->_peerInfo.channel = 0;
    this->_peerInfo.encrypt = false;
    esp_now_add_peer(&this->_peerInfo);
}

ESPMeshedNode *GetESPMeshedNode(int ID, void (*handler)(uint8_t *data, uint8_t len, uint16_t sender))
{
    ESPMeshedNode *node = ESPMeshedNode::GetInstance();
    node->setNodeId(ID);
    node->setReceiveHandler(handler);
    return node;
}