#include "web_server.h"
#include <string.h>
#include <stdlib.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "config_manager.h"
#include "telegram_bot.h"
#include "doorbell_logic.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "web_server";
static httpd_handle_t server = NULL;

const char *index_html = "<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"<title>Doorbell Admin</title>\n"
"<style>\n"
":root {\n"
"  --bg-color: #0f172a;\n"
"  --glass-bg: rgba(30, 41, 59, 0.7);\n"
"  --glass-border: rgba(255, 255, 255, 0.1);\n"
"  --primary: #3b82f6;\n"
"  --primary-hover: #2563eb;\n"
"  --success: #10b981;\n"
"  --danger: #ef4444;\n"
"  --text: #f8fafc;\n"
"  --text-muted: #94a3b8;\n"
"}\n"
"body {\n"
"  margin: 0;\n"
"  font-family: 'Inter', system-ui, sans-serif;\n"
"  background-color: var(--bg-color);\n"
"  background-image: radial-gradient(circle at 10% 20%, rgba(59, 130, 246, 0.15) 0%, transparent 20%), radial-gradient(circle at 90% 80%, rgba(16, 185, 129, 0.15) 0%, transparent 20%);\n"
"  color: var(--text);\n"
"  min-height: 100vh;\n"
"  display: flex;\n"
"  justify-content: center;\n"
"  align-items: center;\n"
"  padding: 20px;\n"
"}\n"
".container {\n"
"  width: 100%;\n"
"  max-width: 600px;\n"
"  background: var(--glass-bg);\n"
"  backdrop-filter: blur(12px);\n"
"  -webkit-backdrop-filter: blur(12px);\n"
"  border: 1px solid var(--glass-border);\n"
"  border-radius: 16px;\n"
"  padding: 32px;\n"
"  box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);\n"
"  animation: fadeIn 0.5s ease-out;\n"
"  box-sizing: border-box;\n"
"}\n"
"@keyframes fadeIn {\n"
"  from { opacity: 0; transform: translateY(20px); }\n"
"  to { opacity: 1; transform: translateY(0); }\n"
"}\n"
"h1 { margin-top: 0; font-size: 24px; font-weight: 600; text-align: center; margin-bottom: 24px; }\n"
".tabs { display: flex; gap: 8px; margin-bottom: 24px; border-bottom: 1px solid var(--glass-border); padding-bottom: 8px; }\n"
".tab { flex: 1; padding: 10px; text-align: center; cursor: pointer; border-radius: 8px; transition: all 0.2s; font-weight: 500; }\n"
".tab.active { background: var(--primary); color: white; }\n"
".tab:not(.active):hover { background: rgba(255,255,255,0.05); }\n"
".tab-content { display: none; }\n"
".tab-content.active { display: block; animation: fadeIn 0.3s ease-out; }\n"
".form-group { margin-bottom: 16px; }\n"
"label { display: block; margin-bottom: 6px; font-size: 14px; color: var(--text-muted); }\n"
"input { width: 100%; padding: 12px; background: rgba(0,0,0,0.2); border: 1px solid var(--glass-border); border-radius: 8px; color: white; font-size: 16px; box-sizing: border-box; transition: border-color 0.2s; }\n"
"input:focus { outline: none; border-color: var(--primary); }\n"
"button { width: 100%; padding: 14px; background: var(--primary); color: white; border: none; border-radius: 8px; font-size: 16px; font-weight: 600; cursor: pointer; transition: all 0.2s; margin-top: 8px; }\n"
"button:hover { background: var(--primary-hover); transform: translateY(-1px); }\n"
"button.danger { background: var(--danger); }\n"
"button.danger:hover { background: #dc2626; }\n"
"button.success { background: var(--success); }\n"
"button.success:hover { background: #059669; }\n"
".whitelist-item { display: flex; justify-content: space-between; align-items: center; padding: 12px; background: rgba(0,0,0,0.2); border-radius: 8px; margin-bottom: 8px; word-break: break-all; }\n"
".whitelist-item button { width: auto; padding: 6px 12px; margin: 0; font-size: 14px; flex-shrink: 0; margin-left: 10px; }\n"
".status-badge { display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 12px; font-weight: bold; background: var(--primary); }\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div class=\"container\">\n"
"  <h1>Doorbell Control Panel</h1>\n"
"  <div class=\"tabs\">\n"
"    <div class=\"tab active\" onclick=\"switchTab('config')\">Config</div>\n"
"    <div class=\"tab\" onclick=\"switchTab('whitelist')\">Whitelist</div>\n"
"    <div class=\"tab\" onclick=\"switchTab('control')\">Control</div>\n"
"  </div>\n"
"  \n"
"  <div id=\"config\" class=\"tab-content active\">\n"
"    <form id=\"configForm\">\n"
"      <div class=\"form-group\"><label>WiFi SSID</label><input type=\"text\" id=\"wifi_ssid\" required></div>\n"
"      <div class=\"form-group\"><label>WiFi Password</label><input type=\"password\" id=\"wifi_pass\"></div>\n"
"      <div class=\"form-group\"><label>Telegram Bot Token</label><input type=\"text\" id=\"tg_token\" required></div>\n"
"      <div class=\"form-group\"><label>Telegram Chat ID</label><input type=\"text\" id=\"tg_chat\" required></div>\n"
"      <div class=\"form-group\"><label>Telegram Admin ID</label><input type=\"text\" id=\"tg_admin\" required></div>\n"
"      <button type=\"submit\">Save & Restart</button>\n"
"    </form>\n"
"  </div>\n"
"\n"
"  <div id=\"whitelist\" class=\"tab-content\">\n"
"    <form id=\"addWhitelistForm\" style=\"display:flex;gap:8px;margin-bottom:16px;\">\n"
"      <input type=\"number\" id=\"new_user_id\" placeholder=\"User ID\" required style=\"flex:1;margin-bottom:0;\">\n"
"      <button type=\"submit\" style=\"width:auto;margin:0;\">Add</button>\n"
"    </form>\n"
"    <div id=\"whitelist_container\"></div>\n"
"  </div>\n"
"\n"
"  <div id=\"control\" class=\"tab-content\">\n"
"    <div class=\"form-group\">\n"
"      <label>Device Status</label>\n"
"      <div id=\"device_status\" class=\"status-badge\">Checking...</div>\n"
"    </div>\n"
"    <button class=\"success\" onclick=\"openDoor()\" style=\"padding: 24px; font-size: 20px;\">🚪 Open Door</button>\n"
"  </div>\n"
"</div>\n"
"\n"
"<script>\n"
"function switchTab(tabId) {\n"
"  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));\n"
"  document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));\n"
"  document.querySelector(`.tab[onclick=\"switchTab('${tabId}')\"]`).classList.add('active');\n"
"  document.getElementById(tabId).classList.add('active');\n"
"  if(tabId === 'whitelist') loadWhitelist();\n"
"  if(tabId === 'control') loadStatus();\n"
"}\n"
"\n"
"async function loadConfig() {\n"
"  try {\n"
"    let res = await fetch('/api/config');\n"
"    let data = await res.json();\n"
"    document.getElementById('wifi_ssid').value = data.wifi_ssid || '';\n"
"    document.getElementById('wifi_pass').value = data.wifi_password || '';\n"
"    document.getElementById('tg_token').value = data.tg_bot_token || '';\n"
"    document.getElementById('tg_chat').value = data.tg_chat_id || '';\n"
"    document.getElementById('tg_admin').value = data.tg_admin_id || '';\n"
"  } catch (e) { console.error(e); }\n"
"}\n"
"\n"
"document.getElementById('configForm').onsubmit = async (e) => {\n"
"  e.preventDefault();\n"
"  const btn = e.target.querySelector('button');\n"
"  btn.innerText = 'Saving...';\n"
"  const data = {\n"
"    wifi_ssid: document.getElementById('wifi_ssid').value,\n"
"    wifi_password: document.getElementById('wifi_pass').value,\n"
"    tg_bot_token: document.getElementById('tg_token').value,\n"
"    tg_chat_id: document.getElementById('tg_chat').value,\n"
"    tg_admin_id: document.getElementById('tg_admin').value\n"
"  };\n"
"  try {\n"
"    await fetch('/api/config', { method: 'POST', body: JSON.stringify(data) });\n"
"    alert('Config saved! Device will now restart to apply changes.');\n"
"    btn.innerText = 'Save & Restart';\n"
"  } catch (e) { alert('Error saving config'); btn.innerText = 'Save & Restart'; }\n"
"};\n"
"\n"
"async function loadWhitelist() {\n"
"  try {\n"
"    let res = await fetch('/api/whitelist');\n"
"    let users = await res.json();\n"
"    let html = '';\n"
"    users.forEach(u => {\n"
"      html += `<div class=\"whitelist-item\">\n"
"        <span>${u}</span>\n"
"        <button class=\"danger\" onclick=\"removeUser(${u})\">Remove</button>\n"
"      </div>`;\n"
"    });\n"
"    if(users.length === 0) html = '<div style=\"text-align:center;color:var(--text-muted)\">No users in whitelist.</div>';\n"
"    document.getElementById('whitelist_container').innerHTML = html;\n"
"  } catch (e) { console.error(e); }\n"
"}\n"
"\n"
"document.getElementById('addWhitelistForm').onsubmit = async (e) => {\n"
"  e.preventDefault();\n"
"  let id = document.getElementById('new_user_id').value;\n"
"  try {\n"
"    await fetch('/api/whitelist', { method: 'POST', body: JSON.stringify({id: parseInt(id)}) });\n"
"    document.getElementById('new_user_id').value = '';\n"
"    loadWhitelist();\n"
"  } catch (e) { alert('Error adding user'); }\n"
"};\n"
"\n"
"async function removeUser(id) {\n"
"  if(!confirm('Remove user '+id+'?')) return;\n"
"  try {\n"
"    await fetch('/api/whitelist?id='+id, { method: 'DELETE' });\n"
"    loadWhitelist();\n"
"  } catch (e) { alert('Error removing user'); }\n"
"}\n"
"\n"
"async function loadStatus() {\n"
"  try {\n"
"    let res = await fetch('/api/status');\n"
"    let data = await res.json();\n"
"    let badge = document.getElementById('device_status');\n"
"    badge.innerText = data.status;\n"
"    badge.style.background = data.status === 'Connected' ? 'var(--success)' : 'var(--danger)';\n"
"  } catch (e) { console.error(e); }\n"
"}\n"
"\n"
"async function openDoor() {\n"
"  try {\n"
"    let res = await fetch('/api/door/open', { method: 'POST' });\n"
"    if(res.ok) alert('Door open command sent!');\n"
"    else alert('Failed to open door');\n"
"  } catch (e) { alert('Error opening door'); }\n"
"}\n"
"\n"
"loadConfig();\n"
"setInterval(() => { if(document.getElementById('control').classList.contains('active')) loadStatus(); }, 5000);\n"
"</script>\n"
"</body>\n"
"</html>\n";

/* Handlers */

static esp_err_t index_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

static esp_err_t api_config_get_handler(httpd_req_t *req) {
    device_config_t cfg;
    config_manager_load(&cfg);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "wifi_ssid", cfg.wifi_ssid);
    cJSON_AddStringToObject(root, "wifi_password", cfg.wifi_password);
    cJSON_AddStringToObject(root, "tg_bot_token", cfg.tg_bot_token);
    cJSON_AddStringToObject(root, "tg_chat_id", cfg.tg_chat_id);
    cJSON_AddStringToObject(root, "tg_admin_id", cfg.tg_admin_id);
    
    const char *sys_info = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, sys_info, strlen(sys_info));
    free((void *)sys_info);
    cJSON_Delete(root);
    return ESP_OK;
}

static void restart_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

static esp_err_t api_config_post_handler(httpd_req_t *req) {
    char buf[1024];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
        return ESP_FAIL;
    }

    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    device_config_t cfg;
    memset(&cfg, 0, sizeof(device_config_t));
    cJSON *item;
    
    item = cJSON_GetObjectItem(root, "wifi_ssid");
    if (cJSON_IsString(item)) strncpy(cfg.wifi_ssid, item->valuestring, CONFIG_MAX_WIFI_SSID_LEN - 1);
    
    item = cJSON_GetObjectItem(root, "wifi_password");
    if (cJSON_IsString(item)) strncpy(cfg.wifi_password, item->valuestring, CONFIG_MAX_WIFI_PASS_LEN - 1);
    
    item = cJSON_GetObjectItem(root, "tg_bot_token");
    if (cJSON_IsString(item)) strncpy(cfg.tg_bot_token, item->valuestring, CONFIG_MAX_TG_TOKEN_LEN - 1);
    
    item = cJSON_GetObjectItem(root, "tg_chat_id");
    if (cJSON_IsString(item)) strncpy(cfg.tg_chat_id, item->valuestring, CONFIG_MAX_TG_CHAT_ID_LEN - 1);
    
    item = cJSON_GetObjectItem(root, "tg_admin_id");
    if (cJSON_IsString(item)) strncpy(cfg.tg_admin_id, item->valuestring, CONFIG_MAX_TG_ADMIN_ID_LEN - 1);

    config_manager_save(&cfg);
    cJSON_Delete(root);

    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");

    // Restart the device after saving config
    xTaskCreate(restart_task, "restart_task", 2048, NULL, 5, NULL);

    return ESP_OK;
}

static esp_err_t api_whitelist_get_handler(httpd_req_t *req) {
    int64_t wl[20];
    int count = telegram_bot_get_whitelist(wl, 20);
    
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(root, cJSON_CreateNumber((double)wl[i]));
    }
    
    const char *resp = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    free((void *)resp);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t api_whitelist_post_handler(httpd_req_t *req) {
    char buf[128];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) return ESP_FAIL;

    if ((ret = httpd_req_recv(req, buf, remaining)) <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) return ESP_FAIL;

    cJSON *item = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsNumber(item)) {
        telegram_bot_add_user((int64_t)item->valuedouble);
    }
    cJSON_Delete(root);
    
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t api_whitelist_delete_handler(httpd_req_t *req) {
    char query[64];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char val[32];
        if (httpd_query_key_value(query, "id", val, sizeof(val)) == ESP_OK) {
            int64_t id = atoll(val);
            telegram_bot_remove_user(id);
        }
    }
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t api_door_open_post_handler(httpd_req_t *req) {
    doorbell_logic_open_door();
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

static esp_err_t api_status_get_handler(httpd_req_t *req) {
    wifi_ap_record_t ap_info;
    bool connected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", connected ? "Connected" : "Disconnected");
    
    const char *resp = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    free((void *)resp);
    cJSON_Delete(root);
    return ESP_OK;
}

/* Server init/stop */

bool web_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    
    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get_index = { .uri = "/", .method = HTTP_GET, .handler = index_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_get_index);

        httpd_uri_t uri_get_config = { .uri = "/api/config", .method = HTTP_GET, .handler = api_config_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_get_config);

        httpd_uri_t uri_post_config = { .uri = "/api/config", .method = HTTP_POST, .handler = api_config_post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_post_config);

        httpd_uri_t uri_get_whitelist = { .uri = "/api/whitelist", .method = HTTP_GET, .handler = api_whitelist_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_get_whitelist);

        httpd_uri_t uri_post_whitelist = { .uri = "/api/whitelist", .method = HTTP_POST, .handler = api_whitelist_post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_post_whitelist);

        httpd_uri_t uri_delete_whitelist = { .uri = "/api/whitelist", .method = HTTP_DELETE, .handler = api_whitelist_delete_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_delete_whitelist);

        httpd_uri_t uri_post_door = { .uri = "/api/door/open", .method = HTTP_POST, .handler = api_door_open_post_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_post_door);

        httpd_uri_t uri_get_status = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_get_handler, .user_ctx = NULL };
        httpd_register_uri_handler(server, &uri_get_status);

        return true;
    }
    ESP_LOGI(TAG, "Error starting server!");
    return false;
}

void web_server_stop(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
}
