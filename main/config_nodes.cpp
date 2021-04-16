/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNIT_TEST
#include "config_nodes.h"
#include "esp_system.h"
#include "esp_log.h"
#include "../lib/thingset/src/thingset.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include "string.h"

// assumption that config data is smaller than 1024 bytes
#define BUFFER_SIZE 1024;

static const char *TAG = "config_nodes";

EmoncmsConfig emon_config;
MqttConfig mqtt_config;
GeneralConfig general_config;

char device_id[9];
const char manufacturer[] = "Libre Solar";

static DataNode data_nodes[] = {
    TS_NODE_PATH(ID_INFO, "info", 0, NULL),

    TS_NODE_STRING(0x19, "DeviceID", device_id, sizeof(device_id),
        ID_INFO, TS_ANY_R | TS_MKR_W, 0),

    TS_NODE_STRING(0x1A, "Manufacturer", manufacturer, sizeof(manufacturer),
        ID_INFO, TS_ANY_R | TS_MKR_W, 0),

    TS_NODE_PATH(ID_CONF, CONFIG_NODE_NAME, 0, &config_nodes_save),

    TS_NODE_PATH(ID_CONF_GENERAL, CONFIG_NODE_GENERAL, ID_CONF, NULL),

    TS_NODE_STRING(0x32, "WifiSSID", general_config.wifi_ssid, STRING_LEN,
        ID_CONF_GENERAL, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x33, "WifiPassword", general_config.wifi_password, STRING_LEN,
        ID_CONF_GENERAL, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x34, "MdnsHostname", general_config.mdns_hostname, STRING_LEN,
        ID_CONF_GENERAL, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_BOOL(0x35, "TsUseCan", &(general_config.ts_can_active),
        ID_CONF_GENERAL, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_BOOL(0x36, "TsUseSerial", &(general_config.ts_serial_active),
        ID_CONF_GENERAL, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_PATH(ID_CONF_EMONCMS, CONFIG_NODE_EMONCMS, ID_CONF, NULL),

    TS_NODE_BOOL(0x38, "Activate", &(emon_config.active),
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x39, "Hostname", emon_config.emoncms_hostname, STRING_LEN,
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x3A, "Apikey", emon_config.api_key, STRING_LEN,
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x3B, "Url", emon_config.url, STRING_LEN,
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x3C, "SerialNode", emon_config.serial_node, STRING_LEN,
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x3D, "MPPT", emon_config.mppt, STRING_LEN,
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x3E, "BMS", emon_config.bms, STRING_LEN,
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x3F, "Port", emon_config.port, STRING_LEN,
        ID_CONF_EMONCMS, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_PATH(ID_CONF_MQTT, CONFIG_NODE_MQTT, ID_CONF, NULL),

    TS_NODE_BOOL(0x41, "Activate", &(mqtt_config.active),
        ID_CONF_MQTT, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x42, "BrokerHostname", mqtt_config.broker_hostname, STRING_LEN,
        ID_CONF_MQTT, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_BOOL(0x43, "UseSSL", &(mqtt_config.use_ssl),
        ID_CONF_MQTT, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_BOOL(0x44, "UseBrokerAuth", &(mqtt_config.use_broker_auth),
        ID_CONF_MQTT, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x45, "Username", mqtt_config.username, STRING_LEN,
        ID_CONF_MQTT, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_STRING(0x46, "Password", mqtt_config.password, STRING_LEN,
        ID_CONF_MQTT, TS_ANY_R | TS_ANY_W, PUB_NVM),

    TS_NODE_UINT32(0x47, "PubInterval", &(mqtt_config.pub_interval),
        ID_CONF_MQTT, TS_ANY_R | TS_ANY_W, PUB_NVM)
};

ThingSet ts(data_nodes, sizeof(data_nodes)/sizeof(DataNode));

/*
* String array to loop over
*/
const char *nodes[] = {CONFIG_NODE_GENERAL, CONFIG_NODE_EMONCMS, CONFIG_NODE_MQTT, NULL};

void data_nodes_init()
{
    uint64_t id64 = 0;
    // MAC Address of WiFi Station equals base adress
    esp_read_mac(((uint8_t *) &id64) + 2, ESP_MAC_WIFI_STA);
    id64 &= ~((uint64_t) 0xFFFFFFFF << 32);
    id64 += ((uint64_t)CONFIG_LIBRE_SOLAR_TYPE_ID) << 32;
    uint64_to_base32(id64, device_id, sizeof(device_id));

    esp_err_t ret = nvs_flash_init_partition(PARTITION);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to init config partition");
        //continueing makes no sense at this point
        esp_restart();
    }

    nvs_handle_t handle;
    ret = nvs_open_from_partition(PARTITION, NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to init config partition");
        //continueing makes no sense at this point
        esp_restart();
    }

    nvs_iterator_t it =  nvs_entry_find(PARTITION, NAMESPACE, NVS_TYPE_BLOB);
    if (it == NULL) {
        // Never written a blob to NVS after last flash erase
        config_nodes_load_kconfig();
        config_nodes_save();
    } else {
        config_nodes_load();
    }
}

char *process_ts_request(char *req, uint8_t CAN_Address)
{
    if (req[strlen(req) - 1] == '\n') {
        req[strlen(req) - 1] = '\0';
    }
    size_t res_len = BUFFER_SIZE;
    char buf[res_len];
    int len = ts.process((uint8_t *) req, strlen(req), (uint8_t *) buf, res_len);
    if (len == 0) {
        return NULL;
    }
    char *resp = (char *) malloc(len + 1);
    if (resp == NULL) {
        ESP_LOGE(TAG, "Unable to allocate memory for TSResponse");
        return NULL;
    }
    memcpy(resp, buf, len);
    resp[len] = '\0';
    return resp;
}

TSResponse *process_local_request(char *req, uint8_t CAN_Address)
{
    if (req[strlen(req) - 1] == '\n') {
        req[strlen(req) - 1] = '\0';
    }
    TSResponse *res = (TSResponse *) malloc(sizeof(TSResponse));

    res->block = process_ts_request(req, 0);
    res->data = ts_resp_data(res->block);

    if (res->block == NULL) {
        ESP_LOGE(TAG, "ThingsetError, unable to process request");
        res->ts_status_code = TS_STATUS_INTERNAL_SERVER_ERR;
        return res;
    }
    res->ts_status_code = ts_resp_status(res->block);
    return res;
}

void config_nodes_load()
{
    nvs_handle_t handle;
    uint8_t *buf;
    size_t len = 0;
    esp_err_t ret = nvs_open_from_partition(PARTITION, NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to open nvs from partition");
        return;
    }
    for (const char **node = nodes; *node != NULL; node++) {
        nvs_get_blob(handle, *node, NULL, &len);
        buf = (uint8_t *) malloc(len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Unable to allocate buffer for config");
            ret = ESP_FAIL;
            break;
        }
        nvs_get_blob(handle, *node, buf, &len);
        buf[len] = '\0';
        ESP_LOGD(TAG, "Content of NVS blob: %s", buf);

        char *ts_request = build_query(TS_PATCH, (char *) *node, (char *) buf);

        size_t res_len = 16;
        uint8_t response[res_len];
        ts.process((uint8_t *) ts_request, strlen(ts_request), response, res_len);
        ESP_LOGD(TAG, "TS Response: %s", response);
        free(buf);
        free(ts_request);
    }
}

void config_nodes_save()
{
    size_t res_len = BUFFER_SIZE;
    char response[res_len];
    int len;
    nvs_handle_t handle;
    esp_err_t ret = nvs_open_from_partition(PARTITION, NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Unable to open nvs from partition");
        return;
    }
    for (const char **ptr = nodes; *ptr != NULL; ptr++) {
        char *ts_request = build_query(TS_GET, (char *) *ptr, NULL);
        len = ts.process((uint8_t *) ts_request, strlen(ts_request), (uint8_t *) response, res_len);
        ESP_LOGD(TAG, "Got response to query: %s", response);
        char *json_start = ts_resp_data(response);
        len = len - (json_start - response);
        ret = nvs_set_blob(handle, *ptr, json_start, len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Unable to write to NVS, Error: %d", ret);
            break;
        } else {
            nvs_commit(handle);
        }
        free(ts_request);
    }
    nvs_close(handle);
}

void config_nodes_load_kconfig()
{
    strncpy(general_config.wifi_ssid, CONFIG_WIFI_SSID, sizeof(general_config.wifi_ssid));
    strncpy(general_config.wifi_password, CONFIG_WIFI_PASSWORD, sizeof(general_config.wifi_password));
    strncpy(general_config.mdns_hostname, CONFIG_DEVICE_HOSTNAME, sizeof(general_config.mdns_hostname));
    general_config.ts_can_active = CONFIG_THINGSET_CAN;
    general_config.ts_serial_active = CONFIG_THINGSET_SERIAL;

    mqtt_config.active = CONFIG_THINGSET_MQTT;
    strncpy(mqtt_config.broker_hostname, CONFIG_THINGSET_MQTT_BROKER_URI, sizeof(mqtt_config.broker_hostname));
    mqtt_config.use_ssl = CONFIG_THINGSET_MQTT_TLS;
    mqtt_config.use_broker_auth = CONFIG_THINGSET_MQTT_AUTHENTICATION;
    strncpy(mqtt_config.username, CONFIG_THINGSET_MQTT_USER, sizeof(mqtt_config.username));
    strncpy(mqtt_config.password, CONFIG_THINGSET_MQTT_PASS, sizeof(mqtt_config.password));
    mqtt_config.pub_interval = CONFIG_THINGSET_MQTT_PUBLISH_INTERVAL;

    emon_config.active = CONFIG_EMONCMS;
    strncpy(emon_config.emoncms_hostname, CONFIG_EMONCMS_HOST, sizeof(emon_config.emoncms_hostname));
    strncpy(emon_config.port, CONFIG_EMONCMS_PORT, sizeof(emon_config.port));
    strncpy(emon_config.url, CONFIG_EMONCMS_URL, sizeof(emon_config.url));
    strncpy(emon_config.api_key, CONFIG_EMONCMS_APIKEY, sizeof(emon_config.api_key));
    strncpy(emon_config.serial_node, CONFIG_EMONCMS_NODE_SERIAL, sizeof(emon_config.serial_node));
    strncpy(emon_config.mppt, CONFIG_EMONCMS_NODE_MPPT, sizeof(emon_config.mppt));
    strncpy(emon_config.bms, CONFIG_EMONCMS_NODE_BMS, sizeof(emon_config.bms));
}

char *build_query(uint8_t method, char *node, char *payload)
{
    TSUriElems params;
    char base_node[32] = {"conf/"};
    params.ts_payload = payload;
    params.ts_target_node = strcat(base_node, node);
    char *ts_request = ts_build_query(method, &params);
    //eliminate \n termination used for serial requests
    ts_request[strlen(ts_request) -1] = '\0';
    ESP_LOGD(TAG, "Build query to patch node values: %s", ts_request);
    return ts_request;
}

void uint64_to_base32(uint64_t in, char *out, size_t size)
{
    const char alphabet[] = "0123456789abcdefghjkmnpqrstvwxyz";
    // 13 is the maximum number of characters needed to encode 64-bit variable to base32
    int len = (size > 13) ? 13 : size;

    // find out actual length of output string
    for (int i = 0; i < len; i++) {
        if ((in >> (i * 5)) == 0) {
            len = i;
            break;
        }
    }

    for (int i = 0; i < len; i++) {
        out[len-i-1] = alphabet[(in >> (i * 5)) % 32];
    }
    out[len] = '\0';
}

#endif