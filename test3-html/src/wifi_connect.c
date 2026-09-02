#include "wifi_connect.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "lwip/api_shell.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "wifi_device.h"

#define WIFI_SCAN_TIMEOUT_SEC 15
#define WIFI_CONNECT_TIMEOUT_SEC 15
#define WIFI_SCAN_RETRY_MAX 3
#define SELECT_WIFI_SECURITYTYPE WIFI_SEC_TYPE_PSK
#define SELECT_WLAN_PORT "wlan0"

static int g_staScanSuccess = 0;
static int g_connectSuccess = 0;
static int g_ssidCount = 0;
static WifiEvent g_wifiEventHandler = {0};

static void WifiInit(void);
static int WaitScanResult(void);
static int WaitConnectResult(void);
static void OnWifiScanStateChangedHandler(int state, int size);
static void OnWifiConnectionChangedHandler(int state, WifiLinkedInfo *info);
static void OnHotspotStaJoinHandler(StationInfo *info);
static void OnHotspotStateChangedHandler(int state);
static void OnHotspotStaLeaveHandler(StationInfo *info);

int WifiConnect(const char *ssid, const char *psk)
{
    WifiScanInfo *info = NULL;
    unsigned int size = WIFI_SCAN_HOTSPOT_LIMIT;
    static struct netif *lwipNetif = NULL;
    int configId = -1;
    int found = 0;

    if (ssid == NULL || psk == NULL || ssid[0] == '\0') {
        printf("WiFi config invalid.\r\n");
        return -1;
    }

    osDelay(200);
    printf("<-- WiFi system init -->\r\n");
    WifiInit();

    if (EnableWifi() != WIFI_SUCCESS) {
        printf("EnableWifi failed.\r\n");
        return -1;
    }

    if (IsWifiActive() == 0) {
        printf("WiFi station is not active.\r\n");
        return -1;
    }

    info = malloc(sizeof(WifiScanInfo) * WIFI_SCAN_HOTSPOT_LIMIT);
    if (info == NULL) {
        printf("WiFi scan buffer malloc failed.\r\n");
        return -1;
    }

    for (int retry = 0; retry < WIFI_SCAN_RETRY_MAX && !found; retry++) {
        size = WIFI_SCAN_HOTSPOT_LIMIT;
        g_ssidCount = 0;
        g_staScanSuccess = 0;
        (void)memset(info, 0, sizeof(WifiScanInfo) * WIFI_SCAN_HOTSPOT_LIMIT);

        printf("Task 3 running: WiFi scan, retry=%d\r\n", retry + 1);
        if (Scan() != WIFI_SUCCESS) {
            printf("WiFi Scan failed.\r\n");
            continue;
        }

        if (WaitScanResult() != 0) {
            continue;
        }

        if (GetScanInfoList(info, &size) != WIFI_SUCCESS) {
            printf("GetScanInfoList failed.\r\n");
            continue;
        }

        printf("WiFi scan result count=%d\r\n", g_ssidCount);
        for (int i = 0; i < g_ssidCount; i++) {
            printf("WiFi AP %02d: ssid=%s rssi=%d\r\n", i + 1, info[i].ssid, info[i].rssi / 100);
            if (strcmp(ssid, info[i].ssid) == 0) {
                WifiDeviceConfig selectApConfig = {0};

                printf("Task 3 running: WiFi target found, connecting to %s\r\n", ssid);
                (void)strcpy(selectApConfig.ssid, info[i].ssid);
                (void)strcpy(selectApConfig.preSharedKey, psk);
                selectApConfig.securityType = SELECT_WIFI_SECURITYTYPE;

                if (AddDeviceConfig(&selectApConfig, &configId) == WIFI_SUCCESS &&
                    ConnectTo(configId) == WIFI_SUCCESS &&
                    WaitConnectResult() == 1) {
                    found = 1;
                    break;
                }

                printf("WiFi connect to %s failed.\r\n", ssid);
                break;
            }
        }
    }

    free(info);
    info = NULL;

    if (!found) {
        printf("Task 3 running: WiFi connect failed, ssid=%s not connected\r\n", ssid);
        return -1;
    }

    lwipNetif = netifapi_netif_find(SELECT_WLAN_PORT);
    if (lwipNetif == NULL) {
        printf("WiFi netif %s not found.\r\n", SELECT_WLAN_PORT);
        return -1;
    }

    dhcp_start(lwipNetif);
    printf("Task 3 running: WiFi DHCP start\r\n");

    for (int retry = 0; retry < WIFI_CONNECT_TIMEOUT_SEC; retry++) {
        if (dhcp_is_bound(lwipNetif) == ERR_OK) {
            printf("Task 3 running: WiFi connected, DHCP OK\r\n");
            netifapi_netif_common(lwipNetif, dhcp_clients_info_show, NULL);
            return 0;
        }

        printf("Task 3 running: WiFi DHCP in progress\r\n");
        sleep(1);
    }

    printf("Task 3 running: WiFi DHCP timeout\r\n");
    return -1;
}

static void WifiInit(void)
{
    WifiErrorCode error;

    g_wifiEventHandler.OnWifiScanStateChanged = OnWifiScanStateChangedHandler;
    g_wifiEventHandler.OnWifiConnectionChanged = OnWifiConnectionChangedHandler;
    g_wifiEventHandler.OnHotspotStaJoin = OnHotspotStaJoinHandler;
    g_wifiEventHandler.OnHotspotStaLeave = OnHotspotStaLeaveHandler;
    g_wifiEventHandler.OnHotspotStateChanged = OnHotspotStateChangedHandler;

    error = RegisterWifiEvent(&g_wifiEventHandler);
    if (error != WIFI_SUCCESS) {
        printf("RegisterWifiEvent failed, error=%d\r\n", error);
    } else {
        printf("RegisterWifiEvent succeed.\r\n");
    }
}

static void OnWifiScanStateChangedHandler(int state, int size)
{
    if (size > 0) {
        g_ssidCount = size;
        g_staScanSuccess = 1;
    }
    printf("WiFi scan callback: state=%d size=%d\r\n", state, size);
}

static void OnWifiConnectionChangedHandler(int state, WifiLinkedInfo *info)
{
    (void)info;

    if (state == WIFI_STATE_AVALIABLE) {
        g_connectSuccess = 1;
    } else {
        g_connectSuccess = 0;
    }
    printf("WiFi connection callback: state=%d\r\n", state);
}

static void OnHotspotStaJoinHandler(StationInfo *info)
{
    (void)info;
}

static void OnHotspotStaLeaveHandler(StationInfo *info)
{
    (void)info;
}

static void OnHotspotStateChangedHandler(int state)
{
    printf("Hotspot state changed: state=%d\r\n", state);
}

static int WaitScanResult(void)
{
    for (int timeout = 0; timeout < WIFI_SCAN_TIMEOUT_SEC; timeout++) {
        sleep(1);
        if (g_staScanSuccess == 1) {
            printf("WaitScanResult success, used=%ds\r\n", timeout + 1);
            return 0;
        }
    }

    printf("WaitScanResult timeout.\r\n");
    return -1;
}

static int WaitConnectResult(void)
{
    for (int timeout = 0; timeout < WIFI_CONNECT_TIMEOUT_SEC; timeout++) {
        sleep(1);
        if (g_connectSuccess == 1) {
            printf("WaitConnectResult success, used=%ds\r\n", timeout + 1);
            return 1;
        }
    }

    printf("WaitConnectResult timeout.\r\n");
    return 0;
}
