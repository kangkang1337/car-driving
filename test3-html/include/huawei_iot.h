#ifndef HUAWEI_IOT_H
#define HUAWEI_IOT_H

typedef struct {
    const char *host;
    int port;
    const char *deviceId;
    const char *clientId;
    const char *username;
    const char *password;
} HuaweiIotConfig;

typedef struct {
    float temperature;
    float humidity;
    int apIr;
    int apAls;
    int apPs;
    int edgeLeft;
    int edgeRight;
    float distanceCm;
    int ledOn;
} HuaweiCarData;

int HuaweiIotConnect(const HuaweiIotConfig *config);
int HuaweiIotPublishCarData(const HuaweiIotConfig *config, const HuaweiCarData *data);
void HuaweiIotClose(void);

#endif
