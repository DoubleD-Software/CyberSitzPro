#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Arduino_JSON.h>
#include <SPIFFS.h>
#include <DHT.h>

// Enable debugging by defining DEBUG
// #define DEBUG

#ifdef DEBUG
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif

#define PIN_R 2
#define PIN_G 1
#define PIN_B 0
#define PIN_FAN 10
#define PIN_DHT 9

#define CHAN_FAN 0

#define ASS_THRESHOLD_TEMP 27.0

#define SSID "CyberSitz Pro"
#define PASSWORD "Baumgartenstrasse"

#define DEBUG_SSID "Pommes und Schnitzel"
#define DEBUG_PASSWORD "MeinPasswortKriegstDuNicht"

IPAddress localIP(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

DNSServer dnsServer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DHT dht(PIN_DHT, DHT11);

uint8_t valR = 0, valG = 0, valB = 0, valFan = 255, lastValFan = 255, lastR = 0, lastG = 0, lastB = 0;
ulong lastSensorUpdate = 0;
float temperature = 0, humidity = 0, h = 0, lastTemperature = 0, lastHumidity = 0;
bool fanOverride = false, fanState = false, lastFanSate = false, rainbow = false, lastFanOverride = false;

void handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void startWebServer();
void handleDataEvent(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len);
void handleSetColors(JSONVar data);
void handleSetFan(JSONVar data);
void sendChangedSensorValues();
void handleSetRainbow(JSONVar data);
void handleSetAssOverride(JSONVar data);
void rainbowFan();
void sendSensorValues(AsyncWebSocketClient *client);

void setup() {
    Serial.begin(115200);
    pinMode(PIN_R, OUTPUT);
    pinMode(PIN_G, OUTPUT);
    pinMode(PIN_B, OUTPUT);
    digitalWrite(PIN_R, LOW);
    digitalWrite(PIN_G, LOW);
    digitalWrite(PIN_B, LOW);
    ledcSetup(CHAN_FAN, 25000, 8);
    ledcAttachPin(PIN_FAN, CHAN_FAN);

    DEBUG_PRINTF("Starting WiFi AP...\n");

    dht.begin();

#ifdef DEBUG
    WiFi.mode(WIFI_STA);
    WiFi.begin(DEBUG_SSID, DEBUG_PASSWORD);
    while(WiFi.status() != WL_CONNECTED){
        Serial.print(WiFi.status());
        delay(100);
    }
#else
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(localIP, gateway, subnet);
    WiFi.softAP(SSID, PASSWORD);
    WiFi.setTxPower(WIFI_POWER_13dBm);
    dnsServer.start(53, "*", localIP);
#endif

    DEBUG_PRINTF("Started WiFi AP, IP address: %s\n", WiFi.localIP().toString().c_str());
    if (!SPIFFS.begin()) {
        DEBUG_PRINTF("An Error has occurred while mounting SPIFFS\n");
        while (1);
    }
    startWebServer();
}

void loop() {
    dnsServer.processNextRequest();
    if (lastSensorUpdate + 500 <= millis()) {
        ws.cleanupClients();
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();
        lastSensorUpdate = millis();
        sendChangedSensorValues();
        fanState = temperature >= ASS_THRESHOLD_TEMP;
    }
    if (fanState != lastFanSate || fanOverride != lastFanOverride || valFan != lastValFan
        || valR != lastR || valG != lastG || valB != lastB) {
        DEBUG_PRINTF("Fan state: %d, Fan override: %d, Fan val: %d\n", fanState, fanOverride, valFan);
        if (fanState || fanOverride) {
            ledcWrite(CHAN_FAN, valFan);
            analogWrite(PIN_R, valR);
            analogWrite(PIN_G, valG);
            analogWrite(PIN_B, valB);
        } else {
            ledcWrite(CHAN_FAN, 0);
            analogWrite(PIN_R, 0);
            analogWrite(PIN_G, 0);
            analogWrite(PIN_B, 0);
        }
        lastValFan = valFan;
        lastFanOverride = fanOverride;
        lastFanSate = fanState;
        lastR = valR;
        lastG = valG;
        lastB = valB;
    }
    if (rainbow) {
        rainbowFan();
    }
    delay(50);
}

void startWebServer() {
    ws.onEvent(handleWebSocketEvent);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });
    server.serveStatic("/", SPIFFS, "/");
    server.addHandler(&ws);
    server.begin();
}

void handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            DEBUG_PRINTF("ws[%s][%u] connect\n", server->url(), client->id());
            break;
        case WS_EVT_DISCONNECT:
            DEBUG_PRINTF("ws[%s][%u] disconnect\n", server->url(), client->id());
            break;
        case WS_EVT_DATA:
            handleDataEvent(client, arg, data, len);
            break;
        default:
            DEBUG_PRINTF("Unhandled WebSocket event type\n");
            break;
    }
}

void handleDataEvent(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len) {
        JSONVar json = JSON.parse((char*)data);
        if (JSON.typeof(json) == "undefined") {
            DEBUG_PRINTF("Failed to parse JSON\n");
            return;
        }
        String action = json["action"];
        if (action == "set_colors" && JSON.typeof(json["data"]) == "object") {
            handleSetColors(json["data"]);
        } else if (action == "set_fan" && JSON.typeof(json["data"]) == "object") {
            handleSetFan(json["data"]);
        } else if (action == "set_color_fx" && JSON.typeof(json["data"]) == "object") {
            handleSetRainbow(json["data"]);
        } else if (action == "set_ass_override" && JSON.typeof(json["data"]) == "object") {
            handleSetAssOverride(json["data"]);
        } else if (action == "get_sensor_values") {
            sendSensorValues(client);
        } else {
            DEBUG_PRINTF("Invalid action or missing data\n");
        }
    }}

void handleSetColors(JSONVar data) {
    if (JSON.typeof(data["r"]) == "number" && JSON.typeof(data["g"]) == "number" && JSON.typeof(data["b"])) {
        valR = data["r"];
        valG = data["g"];
        valB = data["b"];

        JSONVar response;
        response["type"] = "sensor_values";
        response["data"]["r"] = valR;
        response["data"]["g"] = valG;
        response["data"]["b"] = valB;
        ws.textAll(JSON.stringify(response).c_str());

        DEBUG_PRINTF("Set Colors - R: %d, G: %d, B: %d\n", valR, valG, valB);
    } else {
        if (JSON.typeof(data["r"]) != "number") DEBUG_PRINTF("Invalid data for R\n");
        if (JSON.typeof(data["g"]) != "number") DEBUG_PRINTF("Invalid data for G\n");
        if (JSON.typeof(data["b"]) != "number") DEBUG_PRINTF("Invalid data for B\n");
    }
}

void handleSetFan(JSONVar data) {
    if (JSON.typeof(data["fan"]) == "number") {
        int fan = (int) data["fan"];
        if (fan > 0) {
            valFan = 75 + (fan / 100.0) * 180;
        } else {
            valFan = 0;
        }
        JSONVar response;
        response["type"] = "sensor_values";
        response["data"]["fan"] = fan;
        ws.textAll(JSON.stringify(response).c_str());
        DEBUG_PRINTF("Set fan: %d\n", valFan);
    } else {
        DEBUG_PRINTF("Invalid data for fan\n");
    }
}

void handleSetRainbow(JSONVar data) {
    if (JSON.typeof(data["rainbow"]) == "boolean") {
        rainbow = (bool) data["rainbow"];
        JSONVar response;
        response["type"] = "sensor_values";
        response["data"]["colorfx"] = rainbow;
        ws.textAll(JSON.stringify(response).c_str());
        DEBUG_PRINTF("Set rainbow: %d\n", rainbow);
    } else {
        DEBUG_PRINTF("Invalid data for rainbow\n");
    }
}

void handleSetAssOverride(JSONVar data) {
    if (JSON.typeof(data["override"]) == "boolean") {
        fanOverride = (bool) data["override"];
        JSONVar response;
        response["type"] = "sensor_values";
        response["data"]["assover"] = fanOverride;
        ws.textAll(JSON.stringify(response).c_str());
        DEBUG_PRINTF("Set fan override: %d\n", fanOverride);
    } else {
        DEBUG_PRINTF("Invalid data for ASS override\n");
    }
}

void rainbowFan() {
    float s = 1;
    float v = 1;
    float c = v * s; // Chroma
    float x = c * (1 - abs(fmod(h / 60.0, 2) - 1));
    float m = v - c;
    float r, g, b;

    if (h >= 0 && h < 60) {
        r = c, g = x, b = 0;
    } else if (h >= 60 && h < 120) {
        r = x, g = c, b = 0;
    } else if (h >= 120 && h < 180) {
        r = 0, g = c, b = x;
    } else if (h >= 180 && h < 240) {
        r = 0, g = x, b = c;
    } else if (h >= 240 && h < 300) {
        r = x, g = 0, b = c;
    } else {
        r = c, g = 0, b = x;
    }

    h += 1;
    if (h > 360) h = 0;

    valR = (r + m) * 255;
    valG = (g + m) * 255;
    valB = (b + m) * 255;
}

void sendChangedSensorValues() {
    bool changed = false;
    JSONVar response;
    response["type"] = "sensor_values";
    if (lastTemperature != temperature) {
        response["data"]["temperature"] = temperature;
        lastTemperature = temperature;
        changed = true;
    }
    if (lastHumidity != humidity) {
        response["data"]["humidity"] = humidity;
        lastHumidity = humidity;
        changed = true;
    }
    String responseStr = JSON.stringify(response);
    if (changed) {
        ws.textAll(responseStr.c_str());
    }
}

void sendSensorValues(AsyncWebSocketClient *client) {
    JSONVar response;
    response["type"] = "sensor_values";
    response["data"]["temperature"] = temperature;
    response["data"]["humidity"] = humidity;
    response["data"]["colorfx"] = rainbow;
    response["data"]["assover"] = fanOverride;
    response["data"]["fan"] = max((double) 0, ((valFan - 75) / 180.0) * 100);
    response["data"]["r"] = valR;
    response["data"]["g"] = valG;
    response["data"]["b"] = valB;
    String responseStr = JSON.stringify(response);
    client->text(responseStr.c_str());
}
