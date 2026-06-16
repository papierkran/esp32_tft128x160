#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "cn_font.h"

TFT_eSPI tft = TFT_eSPI();
Preferences preferences;
WiFiServer server(80);  // 使用WiFiServer代替WebServer

// WiFi配置（从Preferences读取）
String wifiSsid = "";
String wifiPassword = "";

// 按钮
#define BTN_UP    32
#define BTN_DOWN  33
#define BTN_OK    34
#define BTN_BACK  35
#define TFT_BLK   22

const int btnPins[] = {BTN_UP, BTN_DOWN, BTN_OK, BTN_BACK};

// 菜单状态
enum AppState {
  STATE_MENU,
  STATE_WEATHER,
  STATE_WIFI_INFO,
  STATE_AP_CONFIG
};

AppState currentState = STATE_MENU;
int menuIndex = 0;
const int menuCount = 3;

// 天气相关
const char* cities[] = {"城市1", "城市2", "城市3", "城市4"};
const int cityCount = 4;
int currentCity = 0;

struct WeatherData {
  String weather;
  String temperature;
  String humidity;
  String windDir;
  String windPower;
  bool valid;
};

WeatherData weatherCache[4];
unsigned long lastRefresh = 0;
const unsigned long REFRESH_INTERVAL = 600000;

// 时间刷新
unsigned long lastTimeRefresh = 0;
const unsigned long TIME_REFRESH_INTERVAL = 1000;
unsigned long lastWifiRefresh = 0;
const unsigned long WIFI_REFRESH_INTERVAL = 1000;

// NTP
const char* ntpServer = "ntp.aliyun.com";
const long gmtOffset_sec = 8 * 3600;
const int daylightOffset_sec = 0;

// 菜单项
const char* menuItems[] = {"天气", "WiFi信息", "WiFi配网"};

// ========== 中文16x16绘制（支持缩放） ==========
void drawCN(int x, int y, const char* str, uint16_t fg, uint16_t bg, float scale = 1.0) {
  while (*str) {
    uint8_t c = *str;
    if (c < 0x80) {
      tft.setTextColor(fg, bg);
      tft.setTextSize(1);
      tft.drawChar(x, y, c, fg, bg, 1);
      x += 8 * scale;
      str++;
    } else {
      uint16_t unicode;
      if ((c & 0xE0) == 0xC0) {
        unicode = ((c & 0x1F) << 6) | (str[1] & 0x3F);
        str += 2;
      } else if ((c & 0xF0) == 0xE0) {
        unicode = ((c & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        str += 3;
      } else { str++; continue; }

      int idx = -1;
      for (int i = 0; ; i++) {
        uint16_t val = pgm_read_word(&cn_font_chars[i]);
        if (val == 0) break;
        if (val == unicode) { idx = i; break; }
      }

      if (idx >= 0) {
        for (int row = 0; row < 16; row++) {
          uint8_t byte1 = pgm_read_byte(&cn_font_data[idx][row * 2]);
          uint8_t byte2 = pgm_read_byte(&cn_font_data[idx][row * 2 + 1]);
          for (int col = 0; col < 8; col++) {
            if (byte1 & (0x80 >> col)) {
              tft.fillRect(x + col * scale, y + row * scale, scale, scale, fg);
            }
          }
          for (int col = 0; col < 8; col++) {
            if (byte2 & (0x80 >> col)) {
              tft.fillRect(x + (col + 8) * scale, y + row * scale, scale, scale, fg);
            }
          }
        }
      }
      x += 16 * scale;
    }
  }
}

// ========== WiFi配置管理 ==========
void loadWifiConfig() {
  preferences.begin("wifi-config", true);
  wifiSsid = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  preferences.end();

// 默认配置（首次使用）
  if (wifiSsid.length() == 0) {
    wifiSsid = "YOUR_WIFI_SSID";
    wifiPassword = "YOUR_WIFI_PASSWORD";
    saveWifiConfig(wifiSsid, wifiPassword);
  }

  Serial.println("Loaded: SSID=" + wifiSsid);
}

void saveWifiConfig(String ssid, String pass) {
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", pass);
  preferences.end();
  Serial.println("Saved: SSID=" + ssid);
}

void clearWifiConfig() {
  preferences.begin("wifi-config", false);
  preferences.clear();
  preferences.end();
  Serial.println("Config cleared");
}

// ========== AP配网网页 ==========
String scanNetworksHtml() {
  String html = "";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
  }
  return html;
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  Serial.println("New client connected");
  
  String request = "";
  String body = "";
  bool isBody = false;
  int contentLength = 0;
  
  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 10000) {
    if (client.available()) {
      char c = client.read();
      timeout = millis();
      
      if (!isBody) {
        request += c;
        // 检查是否到达header末尾
        if (request.endsWith("\r\n\r\n")) {
          // 提取Content-Length
          int clIdx = request.indexOf("Content-Length:");
          if (clIdx >= 0) {
            int start = clIdx + 15;
            int end = request.indexOf("\r\n", start);
            contentLength = request.substring(start, end).toInt();
            Serial.println("Content-Length: " + String(contentLength));
          }
          isBody = true;
        }
      } else {
        body += c;
        if (body.length() >= contentLength) {
          break;  // 已经读取完所有数据
        }
      }
    }
  }
  
  Serial.println("Request: " + request.substring(0, 50));
  Serial.println("Body: " + body);
  
  // 处理GET请求（主页）
  if (request.indexOf("GET / ") >= 0 && request.indexOf("GET / ") < 10) {
    String networks = scanNetworksHtml();
    String html = R"(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>ESP32 WiFi配置</title>
<style>
body{font-family:Arial;margin:20px;background:#f0f0f0}
.container{max-width:400px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h2{color:#333;text-align:center}
.form-group{margin:15px 0}
label{display:block;margin-bottom:5px;font-weight:bold}
select,input{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}
button{width:100%;padding:12px;background:#4CAF50;color:white;border:none;border-radius:5px;font-size:16px;cursor:pointer;margin-top:10px}
button:hover{background:#45a049}
.btn-danger{background:#f44336}
.btn-danger:hover{background:#d32f2f}
.info{background:#e3f2fd;padding:10px;border-radius:5px;margin:10px 0;font-size:14px}
</style>
</head>
<body>
<div class='container'>
<h2>ESP32 WiFi配置</h2>
<div class='info'>当前IP: )" + WiFi.softAPIP().toString() + R"(</div>
<form action='/save' method='POST'>
<div class='form-group'>
<label>选择WiFi:</label>
<select name='ssid' id='ssid' onchange='document.getElementById("manual_ssid").value=this.value'>
<option value=''>-- 选择网络 --</option>)" + networks + R"(
<option value='__manual__'>手动输入...</option>
</select>
</div>
<div class='form-group'>
<label>或手动输入WiFi名称:</label>
<input type='text' id='manual_ssid' name='manual_ssid' placeholder='WiFi名称'>
</div>
<div class='form-group'>
<label>密码:</label>
<input type='password' name='password' placeholder='WiFi密码'>
</div>
<button type='submit'>保存并重启</button>
</form>
<form action='/clear' method='POST'>
<button type='submit' class='btn-danger'>清除配置</button>
</form>
</div>
</body>
</html>
)";
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection:close");
    client.println();
    client.print(html);
  }
  // 处理POST /save请求
  else if (request.indexOf("POST /save") >= 0 && body.length() > 0) {
    Serial.println("=== Processing POST /save ===");
    Serial.println("Body: [" + body + "]");
    
    String ssid = "";
    String password = "";
    
    // 解析参数
    // 格式: ssid=xxx&manual_ssid=xxx&password=xxx
    
    // 提取ssid
    int ssidStart = body.indexOf("ssid=");
    if (ssidStart >= 0) {
      ssidStart += 5;
      int ssidEnd = body.indexOf("&", ssidStart);
      if (ssidEnd < 0) ssidEnd = body.length();
      ssid = body.substring(ssidStart, ssidEnd);
    }
    
    // 提取manual_ssid
    int manualStart = body.indexOf("manual_ssid=");
    if (manualStart >= 0) {
      manualStart += 12;
      int manualEnd = body.indexOf("&", manualStart);
      if (manualEnd < 0) manualEnd = body.length();
      String manual = body.substring(manualStart, manualEnd);
      if (ssid == "__manual__" || ssid.length() == 0) {
        ssid = manual;
      }
    }
    
    // 提取password
    int passStart = body.indexOf("password=");
    if (passStart >= 0) {
      passStart += 9;
      int passEnd = body.indexOf("&", passStart);
      if (passEnd < 0) passEnd = body.length();
      password = body.substring(passStart, passEnd);
    }
    
    // URL解码
    urlDecode(ssid);
    urlDecode(password);
    
    Serial.println("Parsed SSID: [" + ssid + "]");
    Serial.println("Parsed Pass: [" + password + "]");
    
    if (ssid.length() > 0) {
      saveWifiConfig(ssid, password);
      
      String html = R"(
<!DOCTYPE html>
<html><head><meta charset='UTF-8'>
<title>保存成功</title>
<style>body{font-family:Arial;text-align:center;padding:50px}
h2{color:#4CAF50}p{font-size:18px}</style>
</head><body>
<h2>配置已保存!</h2>
<p>SSID: )" + ssid + R"(</p>
<p>ESP32正在重启...</p>
</body></html>
)";
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/html");
      client.println("Connection:close");
      client.println();
      client.print(html);
      delay(1000);
      ESP.restart();
    } else {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/html");
      client.println("Connection:close");
      client.println();
      client.print("<html><body><h2>错误: WiFi名称为空</h2><a href='/'>返回</a></body></html>");
    }
  }
  // 处理POST /clear请求
  else if (request.indexOf("POST /clear") >= 0) {
    clearWifiConfig();
    String html = R"(
<!DOCTYPE html>
<html><head><meta charset='UTF-8'>
<title>已清除</title>
<style>body{font-family:Arial;text-align:center;padding:50px}
h2{color:#f44336}</style>
</head><body>
<h2>配置已清除!</h2>
<p>ESP32正在重启...</p>
</body></html>
)";
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection:close");
    client.println();
    client.print(html);
    delay(1000);
    ESP.restart();
  }
  
  delay(10);
  client.stop();
  Serial.println("Client disconnected");
}

// URL解码函数
void urlDecode(String &str) {
  str.replace("+", " ");
  str.replace("%20", " ");
  str.replace("%21", "!");
  str.replace("%22", "\"");
  str.replace("%23", "#");
  str.replace("%24", "$");
  str.replace("%25", "%");
  str.replace("%26", "&");
  str.replace("%27", "'");
  str.replace("%28", "(");
  str.replace("%29", ")");
  str.replace("%2A", "*");
  str.replace("%2B", "+");
  str.replace("%2C", ",");
  str.replace("%2F", "/");
  str.replace("%3A", ":");
  str.replace("%3B", ";");
  str.replace("%3C", "<");
  str.replace("%3D", "=");
  str.replace("%3E", ">");
  str.replace("%3F", "?");
  str.replace("%40", "@");
  str.trim();
}

// ========== 主菜单 ==========
void showMenu() {
  tft.fillScreen(TFT_BLACK);
  drawCN(30, 5, "功能", TFT_CYAN, TFT_BLACK);

  for (int i = 0; i < menuCount; i++) {
    int y = 35 + i * 30;
    if (i == menuIndex) {
      tft.fillRect(0, y - 2, 128, 20, TFT_DARKGREY);
      drawCN(20, y, menuItems[i], TFT_YELLOW, TFT_DARKGREY);
    } else {
      drawCN(20, y, menuItems[i], TFT_WHITE, TFT_BLACK);
    }
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, 148);
  tft.print("K1:Up K2:Down K3:OK K4:Back");
}

bool fetchWeather(const char* city, int index) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(5000);
  http.begin("https://uapis.cn/api/v1/misc/weather?city=" + String(city));
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    http.end();
    StaticJsonDocument<1024> doc;
    if (!deserializeJson(doc, payload)) {
      weatherCache[index].weather = doc["weather"].as<String>();
      weatherCache[index].temperature = doc["temperature"].as<String>();
      weatherCache[index].humidity = doc["humidity"].as<String>();
      weatherCache[index].windDir = doc["wind_direction"].as<String>();
      weatherCache[index].windPower = doc["wind_power"].as<String>();
      weatherCache[index].valid = true;
      return true;
    }
  } else { http.end(); }
  weatherCache[index].valid = false;
  return false;
}

void fetchAllWeather() {
  tft.fillScreen(TFT_BLACK);
  drawCN(5, 5, "正在获取", TFT_YELLOW, TFT_BLACK);

  for (int i = 0; i < cityCount; i++) {
    drawCN(5, 25 + i * 16, cities[i], TFT_WHITE, TFT_BLACK);
    if (fetchWeather(cities[i], i)) {
      tft.setCursor(40, 27 + i * 16);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setTextSize(1);
      tft.print("OK");
    }
    delay(300);
  }
  lastRefresh = millis();
}

void showWeather() {
  tft.fillScreen(TFT_BLACK);

  if (!weatherCache[currentCity].valid) {
    drawCN(30, 50, "获取失败", TFT_RED, TFT_BLACK);
    return;
  }

  // === 顶部标题栏 ===
  tft.fillRect(0, 0, 160, 38, TFT_BLACK);
  drawCN(4, 5, cities[currentCity], TFT_CYAN, TFT_BLACK, 2);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    // 时间（上行）
    tft.setCursor(130, 5);
    tft.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    // 日期（下行）
    tft.setCursor(130, 17);
    tft.printf("%02d/%02d", timeinfo.tm_mon + 1, timeinfo.tm_mday);
  }

  // === 分隔线 ===
  tft.drawFastHLine(0, 38, 160, TFT_WHITE);

  // === 左侧区域：天气描述(放大) + 风力 ===
  drawCN(4, 44, weatherCache[currentCity].weather.c_str(), TFT_WHITE, TFT_BLACK, 2);

// 风力
  drawCN(4, 84, "风力", TFT_WHITE, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(36, 86);
  tft.print(weatherCache[currentCity].windPower);

  // 风向
  drawCN(4, 100, "风向", TFT_WHITE, TFT_BLACK);
  drawCN(36, 100, weatherCache[currentCity].windDir.c_str(), TFT_GREEN, TFT_BLACK);

  // === 右侧区域：温度大字 + 湿度(温度下方) ===
  tft.drawFastVLine(85, 40, 80, TFT_WHITE);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(90, 50);
  tft.print(weatherCache[currentCity].temperature);
  int tempX = tft.getCursorX();
  tft.drawCircle(tempX + 2, 52, 3, TFT_YELLOW);
  tft.setCursor(tempX + 8, 50);
  tft.print("C");

// 湿度（温度下方）
  tft.drawFastHLine(90, 80, 60, TFT_WHITE);
  drawCN(90, 84, "湿度", TFT_WHITE, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(90, 102);
  tft.print(weatherCache[currentCity].humidity);
  tft.print("%");

  }

void showWifiInfo() {
  tft.fillScreen(TFT_BLACK);

  drawCN(20, 3, "WiFi", TFT_CYAN, TFT_BLACK);
  drawCN(68, 3, "信息", TFT_CYAN, TFT_BLACK);
  tft.drawFastHLine(0, 22, 128, TFT_DARKGREY);

  drawCN(2, 28, "名称", TFT_WHITE, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(36, 30);
  tft.print(WiFi.SSID().c_str());

  drawCN(2, 48, "地址", TFT_WHITE, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(36, 50);
  tft.print(WiFi.localIP().toString());

  drawCN(2, 68, "信号", TFT_WHITE, TFT_BLACK);
  int rssi = WiFi.RSSI();
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(36, 70);
  tft.print(rssi);
  tft.print("dBm ");
  if (rssi > -50) { drawCN(90, 68, "强", TFT_GREEN, TFT_BLACK); }
  else if (rssi > -70) { drawCN(90, 68, "中", TFT_YELLOW, TFT_BLACK); }
  else { drawCN(90, 68, "弱", TFT_RED, TFT_BLACK); }

  tft.drawFastHLine(0, 90, 128, TFT_DARKGREY);

  drawCN(2, 96, "时间", TFT_WHITE, TFT_BLACK);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(36, 96);
    tft.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }

  if (getLocalTime(&timeinfo)) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(36, 118);
    tft.printf("%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  }

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, 148);
  tft.print("K4:Back");
}

// ========== AP配网页面 ==========
void startAPConfig() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP32", "");
  Serial.println("AP started: ESP32");
  Serial.println("IP: " + WiFi.softAPIP().toString());
  server.begin();
  Serial.println("Web server started");
}

void showAPConfigPage() {
  tft.fillScreen(TFT_BLACK);
  drawCN(20, 3, "WiFi", TFT_CYAN, TFT_BLACK);
  drawCN(68, 3, "配网", TFT_CYAN, TFT_BLACK);
  tft.drawFastHLine(0, 22, 128, TFT_DARKGREY);

  drawCN(2, 28, "连接", TFT_WHITE, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(36, 30);
  tft.print("ESP32");

  drawCN(2, 48, "地址", TFT_WHITE, TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(36, 50);
  tft.print(WiFi.softAPIP().toString());

  tft.drawFastHLine(0, 70, 128, TFT_DARKGREY);

  drawCN(2, 76, "功能", TFT_WHITE, TFT_BLACK);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, 92);
  tft.print("1. Phone connect ESP32");
  tft.setCursor(2, 104);
  tft.print("2. Open browser, visit:");
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(2, 116);
  tft.print("http://192.168.4.1");

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setCursor(2, 148);
  tft.print("K4:Back");
}

// ========== setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 ===");

  for (int i = 0; i < 4; i++) pinMode(btnPins[i], INPUT_PULLUP);
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);

  pinMode(17, OUTPUT);
  digitalWrite(17, HIGH); delay(50);
  digitalWrite(17, LOW);  delay(50);
  digitalWrite(17, HIGH); delay(150);

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  loadWifiConfig();

  if (wifiSsid.length() > 0) {
    drawCN(5, 10, "正在连接", TFT_WHITE, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(5, 30);
    tft.print(wifiSsid);

    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
    Serial.println("Connecting to: " + wifiSsid);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    Serial.println("WiFi status: " + String(WiFi.status()));

    if (WiFi.status() == WL_CONNECTED) {
      drawCN(5, 50, "WiFi", TFT_GREEN, TFT_BLACK);
      tft.setCursor(37, 52);
      tft.print("OK!");
      delay(500);

      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      currentState = STATE_WEATHER;
      fetchAllWeather();
      showWeather();
    } else {
      drawCN(5, 50, "连接", TFT_RED, TFT_BLACK);
      drawCN(37, 50, "失败", TFT_RED, TFT_BLACK);
      delay(1000);

      currentState = STATE_AP_CONFIG;
      startAPConfig();
      showAPConfigPage();
    }
  } else {
    drawCN(5, 20, "首次", TFT_YELLOW, TFT_BLACK);
    drawCN(37, 20, "使用", TFT_YELLOW, TFT_BLACK);
    drawCN(5, 40, "请先", TFT_WHITE, TFT_BLACK);
    drawCN(37, 40, "配网", TFT_WHITE, TFT_BLACK);
    delay(1500);

    currentState = STATE_AP_CONFIG;
    startAPConfig();
    showAPConfigPage();
  }
}

// ========== loop ==========
void loop() {
  handleButtons();

  if (currentState == STATE_AP_CONFIG) {
    handleClient();
  }

  if (currentState == STATE_WIFI_INFO && millis() - lastWifiRefresh > WIFI_REFRESH_INTERVAL) {
    showWifiInfo();
    lastWifiRefresh = millis();
  }

  if (currentState == STATE_WEATHER && millis() - lastTimeRefresh > TIME_REFRESH_INTERVAL) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
      // 时间（上行）
      tft.setCursor(130, 5);
      tft.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      // 日期（下行）
      tft.setCursor(130, 17);
      tft.printf("%02d/%02d", timeinfo.tm_mon + 1, timeinfo.tm_mday);
    }
    lastTimeRefresh = millis();
  }
}

// ========== 按钮处理 ==========
void handleButtons() {
  // K1 上
  if (digitalRead(BTN_UP) == LOW) {
    delay(50);
    if (digitalRead(BTN_UP) == LOW) {
      if (currentState == STATE_MENU) {
        menuIndex = (menuIndex - 1 + menuCount) % menuCount;
        showMenu();
      } else if (currentState == STATE_WEATHER) {
        currentCity = (currentCity - 1 + cityCount) % cityCount;
        showWeather();
      }
      while (digitalRead(BTN_UP) == LOW) delay(10);
    }
  }

  // K2 下
  if (digitalRead(BTN_DOWN) == LOW) {
    delay(50);
    if (digitalRead(BTN_DOWN) == LOW) {
      if (currentState == STATE_MENU) {
        menuIndex = (menuIndex + 1) % menuCount;
        showMenu();
      } else if (currentState == STATE_WEATHER) {
        currentCity = (currentCity + 1) % cityCount;
        showWeather();
      }
      while (digitalRead(BTN_DOWN) == LOW) delay(10);
    }
  }

  // K3 确定
  if (digitalRead(BTN_OK) == LOW) {
    delay(50);
    if (digitalRead(BTN_OK) == LOW) {
      if (currentState == STATE_MENU) {
        if (menuIndex == 0) {
          currentState = STATE_WEATHER;
          fetchAllWeather();
          showWeather();
        } else if (menuIndex == 1) {
          currentState = STATE_WIFI_INFO;
          showWifiInfo();
        } else if (menuIndex == 2) {
          currentState = STATE_AP_CONFIG;
          startAPConfig();
          showAPConfigPage();
        }
      }
      while (digitalRead(BTN_OK) == LOW) delay(10);
    }
  }

  // K4 返回
  if (digitalRead(BTN_BACK) == LOW) {
    delay(50);
    if (digitalRead(BTN_BACK) == LOW) {
      if (currentState == STATE_AP_CONFIG) {
        unsigned long pressStart = millis();
        while (digitalRead(BTN_BACK) == LOW) {
          if (millis() - pressStart > 2000) {
            clearWifiConfig();
            tft.fillScreen(TFT_RED);
            drawCN(10, 50, "清除", TFT_WHITE, TFT_RED);
            drawCN(42, 50, "成功", TFT_WHITE, TFT_RED);
            delay(1000);
            ESP.restart();
          }
          delay(10);
        }
        currentState = STATE_MENU;
        server.stop();
        WiFi.mode(WIFI_STA);
        showMenu();
      } else if (currentState != STATE_MENU) {
        currentState = STATE_MENU;
        showMenu();
      } else {
        unsigned long pressStart = millis();
        while (digitalRead(BTN_BACK) == LOW) {
          if (millis() - pressStart > 2000) {
            tft.fillScreen(TFT_RED);
            drawCN(20, 50, "重启中", TFT_WHITE, TFT_RED);
            delay(500);
            ESP.restart();
          }
          delay(10);
        }
      }
      while (digitalRead(BTN_BACK) == LOW) delay(10);
    }
  }
}