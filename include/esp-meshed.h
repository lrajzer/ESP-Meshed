#include <Arduino.h>
#include <vector>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <unordered_map>
#ifdef ESP32
#include <esp_now.h>
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <espnow.h>
#endif

// TODO: ADD SLAVE BROADCAST

void _handlePackage(const uint8_t *mac, const uint8_t *incoming, int len)
{
#ifdef DEBUG_ESP_MESHED
    Serial.println("Handling package");
#endif
}

/**
 * @brief Control packets used for internal communication
 *
 */
enum ControlPacket
{
    NORMAL = 0b0000,
    PRIVATE_KEY_EXCHANGE = 0b001,
    PING_REQUEST = 0b010,
    PING_RESPONSE = 0b011,
    POSSIBLE_SPEEDS_REQUEST = 0b100,
    POSSIBLE_SPEEDS_RESPONSE = 0b101,
    PUBLIC_KEY_REQUEST = 0b110,
    PUBLIC_KEY_RESPONSE = 0b111,
};

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
    /**
     * @brief Adress of instance (singleton)
     *
     */
    static ESPMeshedNode *ESPMeshedNodeInstance;
    /**
     * @brief Adress of this node
     *
     */
    uint16_t _nodeId;
    /**
     * @brief Buffor of message IDs and their time of arrival for duplicate avoidance
     *
     */
    std::vector<std::pair<uint16_t, uint32_t>> _message_id_buffer;
    /**
     * @brief Already seen peers
     *
     */
    std::vector<uint16_t> _peers;
    /**
     * @brief Preffered speed for communication
     *
     */
    wifi_phy_rate_t _preffered_speed = WIFI_PHY_RATE_MAX;
    /**
     * @brief Time between cleaning the message id buffer
     *
     */
    uint8_t _cleanup_time = 5;
#ifdef ESP32
    /**
     * @brief Peer info for broadcast in esp_now
     *
     */
    esp_now_peer_info_t _peerInfo = {};
#endif
    /**
     * @brief Public key for encryption
     *
     */
    uint8_t _public_key[128];
    /**
     * @brief Private key for encryption
     *
     */
    std::unordered_map<uint16_t, uint8_t> _private_keys;
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
    uint8_t *constructHeader(const uint16_t adress, bool ack, ControlPacket packet_type)
    {
        uint8_t *header = new uint8_t[5];
        uint16_t message_id = 0b1100110010101;
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
        // bits 0-11: semi random message id
        // bits 12-14: packet type
        // bits 15-27: node id
        // bit 28-40: adress

        header[0] = (message_id >> 8) & 0xFF;
        header[1] = message_id & 0b11111000;
        header[1] = header[1] | (packet_type & 0b00000111);
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
#ifdef ESP32
    void _deconstructHeader(const uint8_t *data, uint16_t &message_id, uint16_t &node_id, uint16_t &adress, ControlPacket &packet_type)
#elif defined(ESP8266)
    void _deconstructHeader(uint8_t *data, uint16_t &message_id, uint16_t &node_id, uint16_t &adress, ControlPacket &packet_type)
#endif
    {
        message_id = uint16_t(data[0] << 4 | data[1] >> 4);
        packet_type = ControlPacket(data[1] & 0b00001111);
        node_id = uint16_t(data[2] << 4 | data[3] >> 4);
        adress = uint16_t((data[3] & 0b00001111) << 8 | data[4]);
    }

#ifdef ESP32
    uint8_t _retransmit(const uint8_t *data, uint8_t len)
#elif defined(ESP8266)
    uint8_t _retransmit(uint8_t *data, uint8_t len)
#endif
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
#ifdef ESP32
        switch (esp_now_send(this->_broadcastAddress, data, len))
        {
        case ESP_OK:
            return 0;

        default:
            return 1;
        }
#elif defined(ESP8266)
        return esp_now_send(this->_broadcastAddress, data, len);
#endif
    }

    bool _wasRetransmited(const uint8_t *data, uint8_t len)
    {
        uint16_t message_id = uint16_t(data[0] << 8 | data[1]);
        for (uint8_t i = 0; i < this->_message_id_buffer.size(); i++)
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
#ifdef ESP32
        esp_efuse_mac_get_default(mac);
#elif defined(ESP8266)
        WiFi.macAddress(mac);
#endif
        this->_nodeId = (mac[0] << 11 | mac[4] << 7 | mac[2] >> 3) & 0x0FFF;
        this->_cleanup_time = 1;
        this->_receiveHandler = nullptr;
    };

    void _handlePingRequest(uint16_t adress, const uint8_t *incoming, uint8_t len)
    {
        if (adress != 0 && adress != this->_nodeId)
        {
            if (!_wasRetransmited(incoming, len))
            {
                this->_retransmit(incoming, len);
            }
            return;
        }
        uint8_t data[2];
        data[0] = this->_nodeId >> 8;
        data[1] = this->_nodeId & 0xFF;
        this->_sendMessage(data, 2, adress, ControlPacket::PING_RESPONSE);
    }

    void _handlePingResponse(uint16_t adress, const uint8_t *incoming, uint8_t len)
    {
        if (adress != 0 && adress != this->_nodeId)
        {
            if (!_wasRetransmited(incoming, len))
            {
                this->_retransmit(incoming, len);
            }
            return;
        }
        if (adress != incoming[0] << 8 | incoming[1])
        {
            return;
        }
        if (std::find(this->_peers.begin(), this->_peers.end(), adress) != this->_peers.end())
        {
            return;
        }
        _peers.push_back(adress);
    }

    void _handlePossibleSpeedsRequest(uint16_t adress, const uint8_t *incoming, uint8_t len)
    {
        wifi_phy_rate_t preffered_speed = (wifi_phy_rate_t)incoming[0];
        if (preffered_speed < this->_preffered_speed)
        {
            this->_preffered_speed = preffered_speed;
        }
        uint8_t data[1];
        data[0] = this->_preffered_speed;
        this->_sendMessage(data, 1, adress, ControlPacket::POSSIBLE_SPEEDS_RESPONSE);
    }

    void _handlePossibleSpeedsResponse(uint16_t adress, const uint8_t *incoming, uint8_t len)
    {
        wifi_phy_rate_t preffered_speed = (wifi_phy_rate_t)incoming[0];
#ifdef DEBUG_ESP_MESHED
        Serial.print("Preffered speed: ");
        Serial.println(preffered_speed);
#endif
        if (preffered_speed < this->_preffered_speed)
        {
            this->_preffered_speed = preffered_speed;
#ifdef DEBUG_ESP_MESHED
            Serial.print("New preffered speed: ");
            Serial.println(this->_preffered_speed);
#endif
        }
    }

    void _handlePublicKeyRequest(uint16_t adress)
    {
        this->_sendMessage(this->_public_key, 128, adress, ControlPacket::PUBLIC_KEY_RESPONSE);
    }

    void _handlePublicKeyResponse(uint16_t adress, const uint8_t *incoming, uint8_t len)
    {
        uint8_t public_key[128];
        uint8_t private_key[32];
        memcpy(public_key, incoming, 128);
        // Generate private key
        mbedtls_entropy_context entropy;
        mbedtls_ctr_drbg_context ctr_drbg;
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
        mbedtls_ctr_drbg_random(&ctr_drbg, private_key, 32);
        this->_private_keys[adress] = *private_key;
        // Encrypt private key
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, public_key, 128);
        uint8_t encrypted_private_key[32];
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, private_key, encrypted_private_key);
        // Send encrypted private key
        this->_sendMessage(encrypted_private_key, 32, adress, ControlPacket::PRIVATE_KEY_EXCHANGE);
    }

#ifdef ESP32
    uint8_t _sendMessage(const uint8_t *data, const uint8_t len, uint16_t peer, ControlPacket packet_type)
#elif defined(ESP8266)
    uint8_t _sendMessage(uint8_t *data, uint8_t len, uint16_t peer, ControlPacket packet_type)
#endif
    {
        uint8_t *header = this->constructHeader(peer, false, packet_type);
        uint8_t *message = new uint8_t[len + 5];
        memcpy(message, header, 5);
        memcpy(message + 5, data, len);
#ifdef ESP32
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
#elif defined(ESP8266)
        uint8_t msg_sent = esp_now_send(this->_broadcastAddress, message, len + 5);
        delete[] header;
        delete[] message;
        return msg_sent;
#endif
    }

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
     */
    uint8_t sendMessage(uint8_t *data, uint8_t len, uint16_t peer)
    {
        if (len > 245)
        {
            return 1;
        }
        uint8_t msg_sent = this->_sendMessage(data, len, peer, ControlPacket::NORMAL);
        return msg_sent ? msg_sent + 1 : 0;
    }

    /**
     * @brief Set the function invoked after a normal message is received
     *
     * @param handler
     */
    void setReceiveHandler(void (*handler)(uint8_t *data, uint8_t len, uint16_t sender))
    {
        this->_receiveHandler = handler;
    }

    /**
     * @brief Set the time in seconds after which a message id is removed from the buffer
     *
     * @param time
     */
    void setCleanupTime(uint8_t time)
    {
        this->_cleanup_time = time;
    }

    /**
     * @brief Set the adress of this node
     *
     * @param id
     */
    void setNodeId(uint16_t id)
    {
        this->_nodeId = id;
    }

    /**
     * @brief Function to be called in the main loop, used for housekeeping tasks
     *
     */
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
#ifdef ESP32
    /**
     * @brief Internal function to handle incoming packages DO NOT CALL
     *
     */
    void _handlePackage(const uint8_t *mac, const uint8_t *incoming, int len)
#elif defined(ESP8266)
    /**
     * @brief Internal function to handle incoming packages DO NOT CALL
     *
     */
    void _handlePackage(uint8_t *mac, uint8_t *incoming, int len)
#endif
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

        uint16_t message_id, node_id, adress;
        ControlPacket packet_type;
        this->_deconstructHeader(incoming, message_id, node_id, adress, packet_type);
        if (packet_type != ControlPacket::NORMAL)
        {
            switch (packet_type)
            {
            case ControlPacket::PING_REQUEST:
                this->_handlePingRequest(adress, incoming, len);
                return;
            case ControlPacket::PING_RESPONSE:
                this->_handlePingResponse(adress, incoming, len);
                return;
            case ControlPacket::POSSIBLE_SPEEDS_REQUEST:
                this->_handlePossibleSpeedsRequest(adress, incoming, len);
                return;
            case ControlPacket::POSSIBLE_SPEEDS_RESPONSE:
                this->_handlePossibleSpeedsResponse(adress, incoming, len);
                return;
            case ControlPacket::PUBLIC_KEY_REQUEST:
                this->_handlePublicKeyRequest(adress);
                return;
            case ControlPacket::PUBLIC_KEY_RESPONSE:
                this->_handlePublicKeyResponse(adress, incoming, len);
                return;
            case ControlPacket::PRIVATE_KEY_EXCHANGE:
                //  TODO: IMPLEMENT
                return;
            default:
                break;
            }
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

#ifdef ESP32
/**
 * @brief Internal function to handle incoming packages DO NOT CALL
 *
 */
static void _handlePackageStatic(const uint8_t *mac, const uint8_t *incoming, int len)
#elif defined(ESP8266)
/**
 * @brief Internal function to handle incoming packages DO NOT CALL
 *
 */
static void _handlePackageStatic(uint8_t *mac, uint8_t *incoming, uint8_t len)
#endif
{
    ESPMeshedNode::GetInstance()->_handlePackage(mac, incoming, len);
}

/**
 * @brief Get the ESPMeshedNode object (singleton) with the given ID and receive handler
 *
 * @param ID Adress of the node
 * @param handler Function to be called when a normal message is received
 * @return ESPMeshedNode* Adress of the ESPMeshedNode object
 */
ESPMeshedNode *GetESPMeshedNode(int ID, void (*handler)(uint8_t *data, uint8_t len, uint16_t sender))
{
    ESPMeshedNode *node = ESPMeshedNode::GetInstance();
    node->setNodeId(ID);
    node->setReceiveHandler(handler);
    return node;
}

void ESPMeshedNode::_commonSetup()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
#ifdef ESP32
    if (esp_now_init() != ESP_OK)
#elif defined(ESP8266)
    if (esp_now_init() != 0)
#endif
    {
#ifdef DEBUG_ESP_MESHED
        Serial.println("Error initializing ESP-NOW");
#endif
        return;
    }
    esp_now_register_recv_cb(_handlePackageStatic);
#ifdef ESP32
    memcpy(this->_peerInfo.peer_addr, this->_broadcastAddress, 6);
    this->_peerInfo.channel = 0;
    this->_peerInfo.encrypt = false;
    esp_now_add_peer(&this->_peerInfo);
#endif
}