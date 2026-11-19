/*
 * The settings page. See web.h.
 */

#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "net.h"
#include "stats.h"

static const char *TAG = "web";

static const char PAGE[] =
"<!DOCTYPE html><html lang=ru><head><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>Crack the Safe</title><style>"
":root{--bg:#0e1116;--card:#171b22;--txt:#e8eaed;--mut:#8a93a0;--acc:#e3b341;--line:#262c36}"
"*{box-sizing:border-box}body{margin:0;font-family:-apple-system,Segoe UI,Roboto,sans-serif;"
"background:var(--bg);color:var(--txt)}main{max-width:460px;margin:auto;padding:20px}"
"h1{font-size:20px;margin:4px 0 16px}h2{font-size:15px;margin:0 0 10px;color:var(--mut);font-weight:500}"
".c{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}"
".r{display:flex;justify-content:space-between;padding:5px 0}.r span:first-child{color:var(--mut)}"
"input,select{width:100%;padding:10px;margin:6px 0;border-radius:9px;border:1px solid var(--line);"
"background:#0b0e12;color:var(--txt);font-size:15px}"
"button{padding:10px 14px;border:0;border-radius:9px;background:var(--acc);color:#111;font-weight:600;"
"font-size:15px;cursor:pointer;margin:6px 6px 0 0}button.s{background:#2a313c;color:var(--txt)}"
"button:disabled{opacity:.4}progress{width:100%;height:10px}#msg{color:var(--acc);min-height:1.2em}"
"</style></head><body><main><h1>&#128274; Crack the Safe</h1>"
"<div class=c><h2>Прошивка</h2>"
"<div class=r><span>Версия</span><span id=ver>-</span></div>"
"<div class=r><span>Последний релиз</span><span id=lat>-</span></div>"
"<div class=r><span>Статус</span><span id=upd>-</span></div>"
"<progress id=pr max=100 value=0 hidden></progress>"
"<button id=chk class=s onclick=\"post('/api/check')\">Проверить</button>"
"<button id=ins onclick=\"post('/api/install')\" hidden>Установить</button>"
"<p style='margin:14px 0 4px;color:var(--mut)'>Или загрузить свой .bin:</p>"
"<input type=file id=f accept=.bin><button class=s onclick=up()>Загрузить</button></div>"
"<div class=c><h2>Wi-Fi</h2>"
"<div class=r><span>Сеть</span><span id=net>-</span></div>"
"<div class=r><span>Адрес</span><span id=ip>-</span></div>"
"<div class=r><span>Точка настройки</span><span id=ap>-</span></div>"
"<select id=ss onchange=\"q('#ssid').value=this.value\"><option value=''>Сети рядом...</option></select>"
"<input id=ssid placeholder='Имя сети'><input id=pw type=password placeholder='Пароль'>"
"<button onclick=wifi()>Подключить</button><button class=s onclick=scan()>Обновить список</button>"
"<button class=s id=apoff onclick=\"post('/api/ap',{on:false})\">Закрыть точку настройки</button></div>"
"<div class=c><h2>Игра</h2>"
"<div class=r><span>Уровень (продолжение)</span><span id=glv>-</span></div>"
"<div class=r><span>Победы / поражения</span><span id=gwl>-</span></div>"
"<div class=r><span>Из них: время / охранник</span><span id=gto>-</span></div>"
"<div class=r><span>Проигрыши на числе 1 / 2 / 3</span><span id=glo>-</span></div>"
"<div class=r><span>ao5 / ao12</span><span id=gao>-</span></div>"
"<div class=r><span>Сейф дня: лучшее / попыток</span><span id=gdy>-</span></div>"
"<svg id=spark width=100% height=60 viewBox='0 0 300 60' preserveAspectRatio=none></svg>"
"<p style='color:var(--mut);margin:8px 0 4px'>Лучшее время по уровням</p><div id=gbest></div>"
"<p style='color:var(--mut);margin:8px 0 4px'>Последние сборки (сплиты)</p><div id=grec></div></div>"
"<div class=c><h2>Вызов другу</h2>"
"<p style='color:var(--mut);margin:0 0 6px'>Задай код (0-39). Друг выбирает Mode \u2192 Challenge на крутилке.</p>"
"<div style='display:flex;gap:8px'><input id=c0 type=number min=0 max=39><input id=c1 type=number min=0 max=39>"
"<input id=c2 type=number min=0 max=39></div>"
"<div class=r><span>Сейчас</span><span id=gch>-</span></div>"
"<button onclick=chal()>Задать</button><button class=s onclick=\"post('/api/challenge',{clear:true})\">Убрать</button></div>"
"<div class=c><h2>Настройки игры</h2>"
"<div class=r><span>Охранник</span><input type=checkbox id=sg style='width:auto' onchange=sset()></div>"
"<div class=r><span>Охранник слушает микрофон</span><input type=checkbox id=sm style='width:auto' onchange=sset()></div>"
"<div class=r><span>Сон после (мин)</span><select id=ss2 style='width:120px' onchange=sset()>"
"<option value=0>никогда</option><option>2</option><option>5</option><option>10</option><option>30</option></select></div>"
"<button class=s onclick=\"if(confirm('Стереть рекорды и прогресс?'))post('/api/stats_reset')\">Сбросить статистику</button></div>"
"<p id=msg></p></main><script>"
"const q=s=>document.querySelector(s);"
"const U={0:'не проверялось',1:'проверяю...',2:'актуальная',3:'есть обновление',4:'устанавливаю',5:'готово, перезагрузка',6:'ошибка'};"
"const S={0:'не задана',1:'подключаюсь...',2:'подключено',3:'не подключается'};"
"function msg(t){q('#msg').textContent=t||''}"
"async function post(u,b){try{const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(b||{})});msg(r.ok?'':'Ошибка '+r.status)}catch(e){msg('Нет связи')}st()}"
"async function st(){try{const s=await(await fetch('/api/status')).json();"
"q('#ver').textContent=s.version;q('#lat').textContent=s.latest||'-';"
"q('#upd').textContent=U[s.upd]+(s.upd==6&&s.msg?': '+s.msg:'');"
"q('#ins').hidden=s.upd!=3;q('#pr').hidden=s.upd!=4;q('#pr').value=s.progress;"
"q('#net').textContent=s.ssid?s.ssid+' ('+S[s.sta]+')':'не задана';"
"q('#ip').textContent=s.ip||'-';q('#ap').textContent=s.ap?s.ap_ssid:'закрыта';q('#apoff').hidden=!s.ap||!s.ssid"
"}catch(e){}}"
"async function scan(){msg('Ищу сети...');try{const l=await(await fetch('/api/scan')).json();"
"const o=q('#ss');o.length=1;l.forEach(n=>{const e=document.createElement('option');e.value=n.ssid;"
"e.textContent=n.ssid+'  '+n.rssi+' dBm'+(n.open?'':' \\u{1F512}');o.add(e)});msg('')}catch(e){msg('Не получилось')}}"
"function wifi(){post('/api/wifi',{ssid:q('#ssid').value,pass:q('#pw').value});"
"msg('Подключаюсь. Если это точка настройки, страница пропадёт: открой http://knob.local в своей сети.')}"
"async function up(){const f=q('#f').files[0];if(!f)return;msg('Загружаю '+f.name+'...');"
"try{const r=await fetch('/api/upload',{method:'POST',body:f});msg(r.ok?'Записано, перезагрузка...':await r.text())}"
"catch(e){msg('Обрыв загрузки')}}"
"const sec=m=>m?(m/1000).toFixed(1)+'s':'-';"
"let G={};async function gs(){try{const g=await(await fetch('/api/stats')).json();G=g;"
"q('#glv').textContent=g.level+' / '+g.levels;q('#gwl').textContent=g.wins+' / '+g.losses;"
"q('#gto').textContent=g.timeouts+' / '+g.caught;q('#glo').textContent=g.lost_on.join(' / ');"
"q('#gao').textContent=sec(g.ao5)+' / '+sec(g.ao12);"
"q('#gdy').textContent=g.today?sec(g.daily_best)+' / '+g.daily_tries:'нет даты';"
"q('#gbest').innerHTML=g.best_ms.map((m,i)=>'<div class=r><span>Уровень '+(i+1)+'</span><span>'+sec(m)+'</span></div>').join('');"
"q('#grec').innerHTML=g.recent.slice().reverse().map(r=>{const s=r.split;"
"return '<div class=r><span>Ур. '+r.level+'</span><span>'+[s[0],s[1]-s[0],s[2]-s[1]].map(x=>(x/1000).toFixed(1)).join(' + ')+' = '+sec(r.ms)+'</span></div>'}).join('')||'<span style=\"color:var(--mut)\">пока нет</span>';"
"const t=g.recent.map(r=>r.ms),mx=Math.max(...t,1);q('#spark').innerHTML=t.length>1?'<polyline fill=none stroke=#e3b341 stroke-width=2 points=\"'+"
"t.map((v,i)=>(i*300/(t.length-1)).toFixed(0)+','+(58-v/mx*54).toFixed(0)).join(' ')+'\"/>':'';"
"q('#gch').textContent=g.challenge?'ждёт игрока':'нет';"
"if(document.activeElement.tagName!='INPUT'&&document.activeElement.tagName!='SELECT'){q('#sg').checked=g.guard;q('#sm').checked=g.mic;q('#ss2').value=g.sleep_min}"
"}catch(e){}}"
"function chal(){const c=[0,1,2].map(i=>parseInt(q('#c'+i).value));"
"if(c.some(v=>isNaN(v)||v<0||v>39)){msg('Три числа от 0 до 39');return}post('/api/challenge',{code:c});msg('Код задан');}"
"function sset(){post('/api/settings',{guard:q('#sg').checked,mic:q('#sm').checked,sleep_min:parseInt(q('#ss2').value)})}"
"st();scan();gs();setInterval(st,1500);setInterval(gs,5000)"
"</script></body></html>";

/* ---- Helpers ------------------------------------------------------------- */

static esp_err_t send_json(httpd_req_t *req, cJSON *j)
{
    char *s = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!s) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const esp_err_t err = httpd_resp_sendstr(req, s);
    free(s);
    return err;
}

/* Small JSON bodies only (settings, not firmware). */
static cJSON *recv_json(httpd_req_t *req)
{
    char buf[256];
    const int len = req->content_len;
    if (len <= 0) {
        return cJSON_CreateObject();
    }
    if (len >= (int)sizeof buf) {
        return NULL;
    }
    int got = 0;
    while (got < len) {
        const int r = httpd_req_recv(req, buf + got, len - got);
        if (r <= 0) {
            return NULL;
        }
        got += r;
    }
    buf[got] = 0;
    return cJSON_Parse(buf);
}

static esp_err_t ok(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

/* ---- Handlers ------------------------------------------------------------ */

static esp_err_t h_page(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGE, sizeof PAGE - 1);
}

static esp_err_t h_status(httpd_req_t *req)
{
    net_status_t s;
    net_get(&s);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "version", s.version);
    cJSON_AddStringToObject(j, "latest", s.latest);
    cJSON_AddNumberToObject(j, "upd", s.upd);
    cJSON_AddNumberToObject(j, "progress", s.progress);
    cJSON_AddStringToObject(j, "msg", s.msg);
    cJSON_AddStringToObject(j, "ssid", s.ssid);
    cJSON_AddNumberToObject(j, "sta", s.sta);
    cJSON_AddStringToObject(j, "ip", s.ip);
    cJSON_AddNumberToObject(j, "rssi", s.rssi);
    cJSON_AddBoolToObject(j, "ap", s.ap_on);
    cJSON_AddStringToObject(j, "ap_ssid", s.ap_ssid);
    return send_json(req, j);
}

static esp_err_t h_scan(httpd_req_t *req)
{
    wifi_scan_config_t sc = { .show_hidden = false };
    cJSON *list = cJSON_CreateArray();
    if (esp_wifi_scan_start(&sc, true) == ESP_OK) {
        uint16_t n = 20;
        wifi_ap_record_t *ap = calloc(n, sizeof *ap);
        if (ap && esp_wifi_scan_get_ap_records(&n, ap) == ESP_OK) {
            for (int i = 0; i < n; i++) {
                if (!ap[i].ssid[0]) {
                    continue;
                }
                bool dup = false;                 /* one row per name */
                for (int k = 0; k < i && !dup; k++) {
                    dup = strcmp((char *)ap[k].ssid, (char *)ap[i].ssid) == 0;
                }
                if (dup) {
                    continue;
                }
                cJSON *e = cJSON_CreateObject();
                cJSON_AddStringToObject(e, "ssid", (char *)ap[i].ssid);
                cJSON_AddNumberToObject(e, "rssi", ap[i].rssi);
                cJSON_AddBoolToObject(e, "open", ap[i].authmode == WIFI_AUTH_OPEN);
                cJSON_AddItemToArray(list, e);
            }
        }
        free(ap);
    }
    return send_json(req, list);
}

static esp_err_t h_wifi(httpd_req_t *req)
{
    cJSON *j = recv_json(req);
    const cJSON *ss = j ? cJSON_GetObjectItem(j, "ssid") : NULL;
    const cJSON *pw = j ? cJSON_GetObjectItem(j, "pass") : NULL;
    if (!cJSON_IsString(ss) || strlen(ss->valuestring) > 32 ||
        (cJSON_IsString(pw) && strlen(pw->valuestring) > 64)) {
        cJSON_Delete(j);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad ssid/pass");
    }
    char ssid[33], pass[65];
    strlcpy(ssid, ss->valuestring, sizeof ssid);
    strlcpy(pass, cJSON_IsString(pw) ? pw->valuestring : "", sizeof pass);
    cJSON_Delete(j);
    ok(req);
    /* After the answer: switching networks may cut this very connection. */
    vTaskDelay(pdMS_TO_TICKS(300));
    net_set_wifi(ssid, pass);
    return ESP_OK;
}

static esp_err_t h_ap(httpd_req_t *req)
{
    cJSON *j = recv_json(req);
    const cJSON *on = j ? cJSON_GetObjectItem(j, "on") : NULL;
    const bool want = cJSON_IsTrue(on);
    cJSON_Delete(j);
    ok(req);
    vTaskDelay(pdMS_TO_TICKS(300));
    net_setup_ap(want);
    return ESP_OK;
}

static esp_err_t h_check(httpd_req_t *req)
{
    net_check_update();
    return ok(req);
}

static esp_err_t h_install(httpd_req_t *req)
{
    net_install_update();
    return ok(req);
}

static void restart_later(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

static esp_err_t h_upload(httpd_req_t *req)
{
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    const int total = req->content_len;
    if (!part || total <= 0 || (size_t)total > part->size) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No room for this file");
    }
    esp_ota_handle_t ota;
    if (esp_ota_begin(part, OTA_WITH_SEQUENTIAL_WRITES, &ota) != ESP_OK) {
        return httpd_resp_send_500(req);
    }
    ESP_LOGI(TAG, "upload: %d bytes to %s", total, part->label);
    char *buf = malloc(4096);
    int got = 0, last_pct = -1;
    esp_err_t err = buf ? ESP_OK : ESP_ERR_NO_MEM;
    while (err == ESP_OK && got < total) {
        const int r = httpd_req_recv(req, buf, total - got < 4096 ? total - got : 4096);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            err = ESP_FAIL;
            break;
        }
        err = esp_ota_write(ota, buf, r);
        got += r;
        const int pct = (int)(got * 100LL / total);
        if (pct != last_pct && pct < 100) {
            net_upload_progress(pct, NULL);
            last_pct = pct;
        }
    }
    free(buf);
    if (err == ESP_OK) {
        err = esp_ota_end(ota);
    } else {
        esp_ota_abort(ota);
    }
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(part);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "upload failed: %s", esp_err_to_name(err));
        net_upload_progress(0, "Upload failed");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   err == ESP_ERR_OTA_VALIDATE_FAILED
                                       ? "Not a valid firmware image" : "Write failed");
    }
    net_upload_progress(100, NULL);
    httpd_resp_sendstr(req, "OK");
    xTaskCreate(restart_later, "restart", 2048, NULL, 5, NULL);
    return ESP_OK;
}

/* ---- The game ------------------------------------------------------------------ */

static esp_err_t h_stats(httpd_req_t *req)
{
    char *s = stats_json();
    if (!s) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const esp_err_t err = httpd_resp_sendstr(req, s);
    free(s);
    return err;
}

static esp_err_t h_challenge(httpd_req_t *req)
{
    cJSON *j = recv_json(req);
    if (!j) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    }
    if (cJSON_IsTrue(cJSON_GetObjectItem(j, "clear"))) {
        cJSON_Delete(j);
        stats_clear_challenge();
        return ok(req);
    }
    const cJSON *code = cJSON_GetObjectItem(j, "code");
    uint8_t c[GAME_CODE_LEN];
    bool good = cJSON_IsArray(code) && cJSON_GetArraySize(code) == GAME_CODE_LEN;
    for (int k = 0; good && k < GAME_CODE_LEN; k++) {
        const cJSON *v = cJSON_GetArrayItem(code, k);
        good = cJSON_IsNumber(v) && v->valueint >= 0 && v->valueint < GAME_POSITIONS;
        if (good) {
            c[k] = (uint8_t)v->valueint;
        }
    }
    cJSON_Delete(j);
    if (!good) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "three numbers 0-39");
    }
    stats_set_challenge(c);
    return ok(req);
}

static esp_err_t h_settings(httpd_req_t *req)
{
    cJSON *j = recv_json(req);
    if (!j) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad json");
    }
    const cJSON *g = cJSON_GetObjectItem(j, "guard");
    const cJSON *m = cJSON_GetObjectItem(j, "mic");
    const cJSON *sl = cJSON_GetObjectItem(j, "sleep_min");
    if (cJSON_IsBool(g)) {
        stats_set_guard(cJSON_IsTrue(g));
    }
    if (cJSON_IsBool(m)) {
        stats_set_mic(cJSON_IsTrue(m));
    }
    if (cJSON_IsNumber(sl) && sl->valueint >= 0 && sl->valueint <= 120) {
        stats_set_sleep_min((uint8_t)sl->valueint);
    }
    cJSON_Delete(j);
    return ok(req);
}

static esp_err_t h_stats_reset(httpd_req_t *req)
{
    stats_reset();
    return ok(req);
}

/* ---- Start --------------------------------------------------------------- */

void web_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 16;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = 20;
    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "http server did not start");
        return;
    }
    const httpd_uri_t uris[] = {
        { .uri = "/",            .method = HTTP_GET,  .handler = h_page    },
        { .uri = "/api/status",  .method = HTTP_GET,  .handler = h_status  },
        { .uri = "/api/scan",    .method = HTTP_GET,  .handler = h_scan    },
        { .uri = "/api/wifi",    .method = HTTP_POST, .handler = h_wifi    },
        { .uri = "/api/ap",      .method = HTTP_POST, .handler = h_ap      },
        { .uri = "/api/check",   .method = HTTP_POST, .handler = h_check   },
        { .uri = "/api/install", .method = HTTP_POST, .handler = h_install },
        { .uri = "/api/upload",  .method = HTTP_POST, .handler = h_upload  },
        { .uri = "/api/stats",   .method = HTTP_GET,  .handler = h_stats   },
        { .uri = "/api/challenge", .method = HTTP_POST, .handler = h_challenge },
        { .uri = "/api/settings",  .method = HTTP_POST, .handler = h_settings  },
        { .uri = "/api/stats_reset", .method = HTTP_POST, .handler = h_stats_reset },
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++) {
        httpd_register_uri_handler(srv, &uris[i]);
    }
    ESP_LOGI(TAG, "settings page up on port 80");
}
