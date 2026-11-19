/*
 * Wi-Fi and firmware updates. See net.h.
 */

#include "net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"

#include "web.h"

static const char *TAG = "net";

#define NVS_NS        "net"
#define NVS_SSID      "ssid"
#define NVS_PASS      "pass"

#define USER_AGENT    "crack-the-safe-knob"
#define RELEASES_URL  "https://github.com/" NET_GITHUB_REPO "/releases"
#define VERSION_ASSET "version.json"

/* Worker task requests. */
#define REQ_CHECK     (1u << 0)
#define REQ_INSTALL   (1u << 1)

static portMUX_TYPE  s_mux = portMUX_INITIALIZER_UNLOCKED;
static net_status_t  s_st;
static char          s_pass[65];
static char          s_asset_url[512];
static esp_netif_t  *s_sta_netif;
static esp_netif_t  *s_ap_netif;
static TaskHandle_t  s_worker;
static bool          s_mdns_up;
static bool          s_checked_once;
static int           s_retries;
static esp_timer_handle_t s_retry_timer;
static esp_timer_handle_t s_check_timer;
static esp_timer_handle_t s_ap_close_timer;

/* Setup stays open this long after the knob joins its network, so the phone
 * that set it up can still see the result. */
#define AP_CLOSE_AFTER_US (60ull * 1000000ull)

/* ---- Status ------------------------------------------------------------- */

#define ST_EDIT(code) do { taskENTER_CRITICAL(&s_mux); { code; } s_st.serial++; \
                           taskEXIT_CRITICAL(&s_mux); } while (0)

void net_get(net_status_t *out)
{
    taskENTER_CRITICAL(&s_mux);
    *out = s_st;
    taskEXIT_CRITICAL(&s_mux);
}

static void set_upd(upd_state_t u, const char *msg)
{
    ST_EDIT(s_st.upd = u;
            if (msg) { strlcpy(s_st.msg, msg, sizeof s_st.msg); });
}

/* ---- Versions ------------------------------------------------------------ */

/* "v1.2.3", "1.2.3" or "v1.2.3-4-gabcdef-dirty" -> 1002003; -1 if no number. */
static long ver_num(const char *v)
{
    if (!v) {
        return -1;
    }
    if (*v == 'v' || *v == 'V') {
        v++;
    }
    long part[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) {
        char *end;
        part[i] = strtol(v, &end, 10);
        if (end == v) {
            return i ? part[0] * 1000000 + part[1] * 1000 : -1;
        }
        v = end;
        if (*v != '.') {
            break;
        }
        v++;
    }
    return part[0] * 1000000 + part[1] * 1000 + part[2];
}

/* ---- Wi-Fi --------------------------------------------------------------- */

static void load_creds(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    size_t n = sizeof s_st.ssid;
    if (nvs_get_str(h, NVS_SSID, s_st.ssid, &n) != ESP_OK) {
        s_st.ssid[0] = 0;
    }
    n = sizeof s_pass;
    if (nvs_get_str(h, NVS_PASS, s_pass, &n) != ESP_OK) {
        s_pass[0] = 0;
    }
    nvs_close(h);
}

static void save_creds(const char *ssid, const char *pass)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    if (ssid[0]) {
        nvs_set_str(h, NVS_SSID, ssid);
        nvs_set_str(h, NVS_PASS, pass);
    } else {
        nvs_erase_key(h, NVS_SSID);
        nvs_erase_key(h, NVS_PASS);
    }
    nvs_commit(h);
    nvs_close(h);
}

/* Mode for the current wishes: STA when a network is saved, AP+STA while
 * setup is open (the STA side is what scans for networks, even with none
 * saved). */
static void apply_mode(void)
{
    net_status_t st;
    net_get(&st);
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (st.ap_on) {
        mode = WIFI_MODE_APSTA;
    } else if (st.ssid[0]) {
        mode = WIFI_MODE_STA;
    }
    if (mode == WIFI_MODE_NULL) {
        esp_wifi_stop();
        return;
    }
    esp_wifi_set_mode(mode);
    if (mode == WIFI_MODE_APSTA) {
        wifi_config_t ap = { 0 };
        strlcpy((char *)ap.ap.ssid, st.ap_ssid, sizeof ap.ap.ssid);
        ap.ap.ssid_len = strlen(st.ap_ssid);
        ap.ap.channel = 1;
        ap.ap.max_connection = 2;
        ap.ap.authmode = WIFI_AUTH_OPEN;
        esp_wifi_set_config(WIFI_IF_AP, &ap);
    }
    if (st.ssid[0]) {
        wifi_config_t sta = { 0 };
        strlcpy((char *)sta.sta.ssid, st.ssid, sizeof sta.sta.ssid);
        strlcpy((char *)sta.sta.password, s_pass, sizeof sta.sta.password);
        sta.sta.threshold.authmode = s_pass[0] ? WIFI_AUTH_WPA_PSK : WIFI_AUTH_OPEN;
        sta.sta.pmf_cfg.capable = true;
        esp_wifi_set_config(WIFI_IF_STA, &sta);
    }
    esp_wifi_start();
    if (st.ssid[0]) {
        s_retries = 0;
        ST_EDIT(s_st.sta = NET_CONNECTING; s_st.ip[0] = 0);
        esp_wifi_disconnect();
        esp_wifi_connect();
    } else {
        ST_EDIT(s_st.sta = NET_OFF; s_st.ip[0] = 0);
    }
}

static bool st_ap_on(void)
{
    net_status_t st;
    net_get(&st);
    return st.ap_on;
}

static void ap_close_cb(void *arg)
{
    (void)arg;
    net_status_t st;
    net_get(&st);
    if (st.ap_on && st.ssid[0] && st.sta == NET_CONNECTED) {
        ESP_LOGI(TAG, "on the network: closing setup");
        net_setup_ap(false);
    }
}

static void retry_cb(void *arg)
{
    (void)arg;
    esp_wifi_connect();
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
        net_status_t st;
        net_get(&st);
        if (!st.ssid[0]) {
            return;                         /* forgotten on purpose */
        }
        s_retries++;
        ESP_LOGW(TAG, "disconnected (reason %d), retry %d", d->reason, s_retries);
        ST_EDIT(s_st.ip[0] = 0;
                s_st.sta = s_retries >= 3 ? NET_FAILED : NET_CONNECTING);
        /* Quick at first, then every 15 s. */
        const uint64_t us = s_retries < 3 ? 1000000ull : 15000000ull;
        esp_timer_stop(s_retry_timer);
        esp_timer_start_once(s_retry_timer, us);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        wifi_ap_record_t ap;
        const int8_t rssi = esp_wifi_sta_get_ap_info(&ap) == ESP_OK ? ap.rssi : 0;
        s_retries = 0;
        ST_EDIT(s_st.sta = NET_CONNECTED;
                snprintf(s_st.ip, sizeof s_st.ip, IPSTR, IP2STR(&e->ip_info.ip));
                s_st.rssi = rssi);
        ESP_LOGI(TAG, "connected, http://" NET_HOSTNAME ".local  http://" IPSTR,
                 IP2STR(&e->ip_info.ip));
        if (!s_mdns_up && mdns_init() == ESP_OK) {
            mdns_hostname_set(NET_HOSTNAME);
            mdns_instance_name_set("Crack the Safe knob");
            mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
            s_mdns_up = true;
        }
        if (st_ap_on()) {
            esp_timer_stop(s_ap_close_timer);
            esp_timer_start_once(s_ap_close_timer, AP_CLOSE_AFTER_US);
        }
        if (!s_checked_once) {
            s_checked_once = true;
            net_check_update();
        }
    }
}

void net_set_wifi(const char *ssid, const char *pass)
{
    char ss[33], pw[65];
    strlcpy(ss, ssid ? ssid : "", sizeof ss);
    strlcpy(pw, pass ? pass : "", sizeof pw);
    save_creds(ss, pw);
    strlcpy(s_pass, pw, sizeof s_pass);
    ST_EDIT(strlcpy(s_st.ssid, ss, sizeof s_st.ssid);
            /* Joining a new network closes setup, unless nothing is saved. */
            if (!ss[0]) { s_st.ap_on = true; });
    ESP_LOGI(TAG, "network set to \"%s\"", ss);
    apply_mode();
}

void net_setup_ap(bool on)
{
    net_status_t st;
    net_get(&st);
    if (!on && !st.ssid[0]) {
        on = true;                          /* nothing else to fall back on */
    }
    ST_EDIT(s_st.ap_on = on);
    apply_mode();
}

/* ---- GitHub ------------------------------------------------------------- */

static void http_common(esp_http_client_config_t *c, const char *url)
{
    c->url = url;
    c->crt_bundle_attach = esp_crt_bundle_attach;
    c->user_agent = USER_AGENT;
    c->timeout_ms = 15000;
    c->buffer_size = 4096;
    c->buffer_size_tx = 2048;               /* GitHub's asset redirects are long */
    c->max_redirection_count = 5;
}

/* The latest release, without GitHub's API (60 requests an hour per public
 * IP, shared with every other device behind the same router): each release
 * carries a tiny version.json, and github.com/.../releases/latest/download/
 * redirects to the newest one. Fills s_st.latest and s_asset_url. */

typedef struct {
    char *buf;
    int   len;
    int   cap;
} body_t;

static esp_err_t on_http(esp_http_client_event_t *e)
{
    body_t *b = e->user_data;
    /* Only the final answer's body: the redirects before it carry their own. */
    if (e->event_id == HTTP_EVENT_ON_DATA && b &&
        esp_http_client_get_status_code(e->client) == 200) {
        const int room = b->cap - b->len;
        const int n = e->data_len < room ? e->data_len : room;
        memcpy(b->buf + b->len, e->data, n);
        b->len += n;
    }
    return ESP_OK;
}

static esp_err_t fetch_latest(void)
{
    char buf[512];
    body_t body = { .buf = buf, .cap = sizeof buf - 1 };
    esp_http_client_config_t cfg = { 0 };
    http_common(&cfg, RELEASES_URL "/latest/download/" VERSION_ASSET);
    cfg.event_handler = on_http;
    cfg.user_data = &body;
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = esp_http_client_perform(c);   /* follows the redirects */
    const int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);
    if (err != ESP_OK) {
        return err;
    }
    if (status != 200) {
        ESP_LOGW(TAG, "version.json: HTTP %d", status);
        return status == 404 ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }
    buf[body.len] = 0;

    cJSON *root = cJSON_Parse(buf);
    const cJSON *ver = root ? cJSON_GetObjectItem(root, "version") : NULL;
    if (!cJSON_IsString(ver) || ver->valuestring[0] == 0 ||
        strlen(ver->valuestring) >= sizeof s_st.latest) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_RESPONSE;
    }
    snprintf(s_asset_url, sizeof s_asset_url, RELEASES_URL "/download/%s/" NET_RELEASE_ASSET,
             ver->valuestring);
    ST_EDIT(strlcpy(s_st.latest, ver->valuestring, sizeof s_st.latest));
    cJSON_Delete(root);
    return ESP_OK;
}

static void do_check(void)
{
    net_status_t st;
    net_get(&st);
    if (st.sta != NET_CONNECTED) {
        set_upd(UPD_ERROR, "No Wi-Fi");
        return;
    }
    if (st.upd == UPD_INSTALLING || st.upd == UPD_DONE) {
        return;
    }
    set_upd(UPD_CHECKING, NULL);
    const esp_err_t err = fetch_latest();
    if (err == ESP_ERR_NOT_FOUND) {
        set_upd(UPD_ERROR, "No release yet");
        return;
    }
    if (err != ESP_OK) {
        set_upd(UPD_ERROR, "Check failed");
        return;
    }
    net_get(&st);
    const bool newer = ver_num(st.latest) > ver_num(st.version);
    ESP_LOGI(TAG, "running %s, latest %s%s", st.version, st.latest,
             newer ? " -- update available" : "");
    set_upd(newer ? UPD_AVAILABLE : UPD_UP_TO_DATE, NULL);
}

static void do_install(void)
{
    net_status_t st;
    net_get(&st);
    if (st.upd != UPD_AVAILABLE || !s_asset_url[0]) {
        return;
    }
    ESP_LOGI(TAG, "installing %s from %s", st.latest, s_asset_url);
    ST_EDIT(s_st.upd = UPD_INSTALLING; s_st.progress = 0);

    esp_http_client_config_t http = { 0 };
    http_common(&http, s_asset_url);
    http.keep_alive_enable = true;
    esp_https_ota_config_t cfg = { .http_config = &http };
    esp_https_ota_handle_t h = NULL;
    esp_err_t err = esp_https_ota_begin(&cfg, &h);
    if (err == ESP_OK) {
        esp_app_desc_t desc;
        if (esp_https_ota_get_img_desc(h, &desc) == ESP_OK &&
            strcmp(desc.project_name, esp_app_get_description()->project_name) != 0) {
            ESP_LOGE(TAG, "image is \"%s\", not this firmware", desc.project_name);
            err = ESP_ERR_INVALID_VERSION;
        }
    }
    while (err == ESP_OK) {
        err = esp_https_ota_perform(h);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        err = ESP_OK;
        const int total = esp_https_ota_get_image_size(h);
        const int done = esp_https_ota_get_image_len_read(h);
        if (total > 0) {
            const uint8_t p = (uint8_t)(done * 100LL / total);
            if (p != s_st.progress) {
                ST_EDIT(s_st.progress = p);
            }
        }
    }
    if (err == ESP_OK && !esp_https_ota_is_complete_data_received(h)) {
        err = ESP_FAIL;
    }
    if (err == ESP_OK) {
        err = esp_https_ota_finish(h);
    } else if (h) {
        esp_https_ota_abort(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "update failed: %s", esp_err_to_name(err));
        set_upd(UPD_ERROR, err == ESP_ERR_INVALID_VERSION ? "Wrong image"
                                                          : "Download failed");
        return;
    }
    ST_EDIT(s_st.upd = UPD_DONE; s_st.progress = 100);
    ESP_LOGI(TAG, "update written, restarting");
    vTaskDelay(pdMS_TO_TICKS(1500));        /* let the screen say so */
    esp_restart();
}

static void worker(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t req = 0;
        xTaskNotifyWait(0, UINT32_MAX, &req, portMAX_DELAY);
        if (req & REQ_INSTALL) {
            do_install();
        } else if (req & REQ_CHECK) {
            do_check();
        }
    }
}

void net_check_update(void)
{
    if (s_worker) {
        xTaskNotify(s_worker, REQ_CHECK, eSetBits);
    }
}

void net_install_update(void)
{
    if (s_worker) {
        xTaskNotify(s_worker, REQ_INSTALL, eSetBits);
    }
}

static void check_timer_cb(void *arg)
{
    (void)arg;
    net_check_update();
}

void net_upload_progress(int percent, const char *error)
{
    if (error) {
        set_upd(UPD_ERROR, error);
    } else if (percent >= 100) {
        ST_EDIT(s_st.upd = UPD_DONE; s_st.progress = 100);
    } else {
        ST_EDIT(s_st.upd = UPD_INSTALLING; s_st.progress = (uint8_t)percent);
    }
}

/* ---- Start -------------------------------------------------------------- */

void net_start(void)
{
    strlcpy(s_st.version, esp_app_get_description()->version, sizeof s_st.version);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_st.ap_ssid, sizeof s_st.ap_ssid, "Knob-%02X%02X", mac[4], mac[5]);
    load_creds();
    s_st.ap_on = s_st.ssid[0] == 0;         /* nothing saved: open setup */

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    esp_netif_set_hostname(s_sta_netif, NET_HOSTNAME);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_storage(WIFI_STORAGE_RAM); /* our own NVS keys are the truth */
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi, NULL));

    const esp_timer_create_args_t rt = { .callback = retry_cb, .name = "wifi_retry" };
    esp_timer_create(&rt, &s_retry_timer);
    const esp_timer_create_args_t ct = { .callback = check_timer_cb, .name = "upd_check" };
    esp_timer_create(&ct, &s_check_timer);
    const esp_timer_create_args_t at = { .callback = ap_close_cb, .name = "ap_close" };
    esp_timer_create(&at, &s_ap_close_timer);
    esp_timer_start_periodic(s_check_timer, NET_CHECK_PERIOD_H * 3600ull * 1000000ull);

    xTaskCreatePinnedToCore(worker, "net_upd", 8192, NULL, 3, &s_worker, 0);

    apply_mode();
    web_start();
    ESP_LOGI(TAG, "firmware %s, network \"%s\"%s", s_st.version, s_st.ssid,
             s_st.ap_on ? ", setup AP open" : "");
}
