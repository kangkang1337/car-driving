#include "huawei_iot.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include "lwip/sockets.h"

#define MQTT_RECV_TIMEOUT_SEC 5
#define MQTT_KEEP_ALIVE_SEC 60
#define MQTT_TX_BUFFER_SIZE 768
#define MQTT_JSON_BUFFER_SIZE 512
#define MQTT_TOPIC_BUFFER_SIZE 128
#define MQTT_FIXED_HEADER_MAX 5

static int g_mqttSocket = -1;

static int WriteAll(const uint8_t *data, int len)
{
    int sent = 0;

    while (sent < len) {
        int ret = send(g_mqttSocket, data + sent, len - sent, 0);
        if (ret <= 0) {
            return -1;
        }
        sent += ret;
    }
    return 0;
}

static int ReadAll(uint8_t *data, int len)
{
    int received = 0;

    while (received < len) {
        int ret = recv(g_mqttSocket, data + received, len - received, 0);
        if (ret <= 0) {
            return -1;
        }
        received += ret;
    }
    return received;
}

static int EncodeRemainLength(uint8_t *buf, int len)
{
    int index = 0;

    do {
        uint8_t byte = (uint8_t)(len % 128);
        len /= 128;
        if (len > 0) {
            byte |= 0x80;
        }
        buf[index++] = byte;
    } while (len > 0 && index < MQTT_FIXED_HEADER_MAX);

    return index;
}

static int PutString(uint8_t *buf, int pos, const char *str)
{
    uint16_t len = (uint16_t)strlen(str);

    buf[pos++] = (uint8_t)(len >> 8);
    buf[pos++] = (uint8_t)(len & 0xff);
    (void)memcpy(&buf[pos], str, len);
    return pos + len;
}

static int MqttStringLen(const char *str)
{
    if (str == NULL) {
        return -1;
    }
    return 2 + (int)strlen(str);
}

static int TcpConnect(const char *host, int port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *item = NULL;
    int sock = -1;
    int ret;

    (void)memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(host, NULL, &hints, &result);
    if (ret != 0 || result == NULL) {
        printf("Task 4 running: Huawei Cloud DNS failed, host=%s\r\n", host);
        return -1;
    }

    for (item = result; item != NULL; item = item->ai_next) {
        struct sockaddr_in addr;
        struct timeval timeout = {MQTT_RECV_TIMEOUT_SEC, 0};

        if (item->ai_family != AF_INET) {
            continue;
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            continue;
        }

        (void)memcpy(&addr, item->ai_addr, sizeof(addr));
        addr.sin_port = htons((uint16_t)port);
        (void)setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        (void)setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

        ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
        if (ret == 0) {
            break;
        }

        (void)lwip_close(sock);
        sock = -1;
    }

    freeaddrinfo(result);
    return sock;
}

int HuaweiIotConnect(const HuaweiIotConfig *config)
{
    uint8_t packet[MQTT_TX_BUFFER_SIZE];
    uint8_t remain[MQTT_FIXED_HEADER_MAX];
    uint8_t ack[4] = {0};
    int pos = 0;
    int vhPayloadLen;
    int remainLen;
    int remainBytes;
    int ret;

    if (config == NULL || config->host == NULL || config->clientId == NULL ||
        config->username == NULL || config->password == NULL) {
        return -1;
    }

    remainLen = MqttStringLen("MQTT") + 4 + MqttStringLen(config->clientId) +
        MqttStringLen(config->username) + MqttStringLen(config->password);
    remainBytes = EncodeRemainLength(remain, remainLen);
    if (remainLen <= 0 || 1 + remainBytes + remainLen > MQTT_TX_BUFFER_SIZE) {
        printf("Task 4 running: MQTT CONNECT packet too long\r\n");
        return -1;
    }

    HuaweiIotClose();
    g_mqttSocket = TcpConnect(config->host, config->port);
    if (g_mqttSocket < 0) {
        printf("Task 4 running: Huawei Cloud TCP connect failed\r\n");
        return -1;
    }

    pos = 0;
    pos = PutString(packet, pos, "MQTT");
    packet[pos++] = 4;
    packet[pos++] = 0xc2;
    packet[pos++] = 0;
    packet[pos++] = MQTT_KEEP_ALIVE_SEC;
    pos = PutString(packet, pos, config->clientId);
    pos = PutString(packet, pos, config->username);
    pos = PutString(packet, pos, config->password);
    vhPayloadLen = pos;

    (void)memmove(packet + 1 + remainBytes, packet, vhPayloadLen);
    packet[0] = 0x10;
    (void)memcpy(packet + 1, remain, remainBytes);

    ret = WriteAll(packet, 1 + remainBytes + vhPayloadLen);
    if (ret != 0) {
        printf("Task 4 running: MQTT CONNECT send failed\r\n");
        HuaweiIotClose();
        return -1;
    }

    ret = ReadAll(ack, sizeof(ack));
    if (ret < 4 || ack[0] != 0x20 || ack[3] != 0) {
        printf("Task 4 running: MQTT CONNACK failed, ret=%d code=%u\r\n", ret, (unsigned int)ack[3]);
        HuaweiIotClose();
        return -1;
    }

    printf("Task 4 running: Huawei Cloud MQTT connected\r\n");
    return 0;
}

int HuaweiIotPublishCarData(const HuaweiIotConfig *config, const HuaweiCarData *data)
{
    char topic[MQTT_TOPIC_BUFFER_SIZE];
    char json[MQTT_JSON_BUFFER_SIZE];
    uint8_t packet[MQTT_TX_BUFFER_SIZE];
    uint8_t remain[MQTT_FIXED_HEADER_MAX];
    int topicLen;
    int jsonLen;
    int pos = 0;
    int remainLen;
    int remainBytes;

    if (config == NULL || data == NULL || config->deviceId == NULL || g_mqttSocket < 0) {
        return -1;
    }

    topicLen = snprintf(topic, sizeof(topic), "$oc/devices/%s/sys/properties/report", config->deviceId);
    jsonLen = snprintf(json, sizeof(json),
        "{\"services\":[{\"service_id\":\"qstcar\",\"properties\":{"
        "\"temp\":%d,\"humi\":%d,\"lumi\":%d,"
        "\"mode_led\":\"SEN\",\"car_mode\":\"ULE\","
        "\"temperature\":%.1f,\"humidity\":%.1f,"
        "\"ap_ir\":%d,\"ap_als\":%d,\"ap_ps\":%d,"
        "\"edge_left\":%d,\"edge_right\":%d,"
        "\"distance_cm\":%.1f,\"led_on\":%d"
        "}}]}",
        (int)data->temperature, (int)data->humidity, data->apAls,
        data->temperature, data->humidity,
        data->apIr, data->apAls, data->apPs,
        data->edgeLeft, data->edgeRight,
        data->distanceCm, data->ledOn);

    if (topicLen <= 0 || topicLen >= (int)sizeof(topic) ||
        jsonLen <= 0 || jsonLen >= (int)sizeof(json)) {
        printf("Task 4 running: MQTT payload too long\r\n");
        return -1;
    }

    remainLen = 2 + topicLen + jsonLen;
    remainBytes = EncodeRemainLength(remain, remainLen);
    if (1 + remainBytes + remainLen > MQTT_TX_BUFFER_SIZE) {
        printf("Task 4 running: MQTT packet too long\r\n");
        return -1;
    }

    packet[pos++] = 0x30;
    (void)memcpy(packet + pos, remain, remainBytes);
    pos += remainBytes;
    packet[pos++] = (uint8_t)(topicLen >> 8);
    packet[pos++] = (uint8_t)(topicLen & 0xff);
    (void)memcpy(packet + pos, topic, topicLen);
    pos += topicLen;
    (void)memcpy(packet + pos, json, jsonLen);
    pos += jsonLen;

    if (WriteAll(packet, pos) != 0) {
        printf("Task 4 running: MQTT publish send failed\r\n");
        HuaweiIotClose();
        return -1;
    }

    printf("Task 4 running: Huawei Cloud data upload OK\r\n");
    return 0;
}

void HuaweiIotClose(void)
{
    if (g_mqttSocket >= 0) {
        (void)lwip_close(g_mqttSocket);
        g_mqttSocket = -1;
    }
}
