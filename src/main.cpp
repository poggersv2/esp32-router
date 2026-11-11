#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_eap_client.h>
#include "secrets.h"
#include "sta.h"
#include "ap.h"
#include "nat.h"

enum class WiFiAuthType : uint8_t {
    WPA,
    EAP
};

enum class WiFiMode : uint8_t {
    STA,      // Station mode (connects to a router)
    AP,       // Access Point mode (creates its own Wi-Fi)
    AP_STA    // Dual mode (both at the same time)
};

WiFiAuthType authType = WiFiAuthType::WPA;
WiFiMode wifiMode = WiFiMode::AP_STA;

// Find secrets inside secrets.h


// Timing
unsigned long lastStatusUpdate = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastNatAttempt = 0;   // not used until NAT is added back

const unsigned long STATUS_INTERVAL = 5000;
const unsigned long RECONNECT_INTERVAL = 15000;
const unsigned long NAT_RETRY_INTERVAL = 5000;   // not used until NAT is added back

// Which interfaces the current mode uses
bool staEnabled() { return wifiMode == WiFiMode::STA || wifiMode == WiFiMode::AP_STA; }
bool apEnabled()  { return wifiMode == WiFiMode::AP  || wifiMode == WiFiMode::AP_STA; }

bool waitForConnection(uint32_t timeoutMs = 15000) {
    uint32_t start = millis();
    Serial.print("[wifi]: waiting for IP");

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= timeoutMs) {
            Serial.println("\nerror(wifi): connection timed out.");
            return false;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println("\n[success(wifi)]: connected! IP: " + WiFi.localIP().toString());
    return true;
}

bool connectToWiFi(WiFiAuthType authType, bool wait = true) {
    bool configured = false;
    switch (authType) {
        case WiFiAuthType::EAP:
            configured = connectToEAPWiFi();
            break;
        case WiFiAuthType::WPA:
            configured = connectToWPAWiFi();
            break;
    }

    if (!configured) {
        return false;
    }

    if (!wait) {
        return true;
    }

    return waitForConnection();
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.printf("[wifi]: disconnected, reason %d\n", info.wifi_sta_disconnected.reason);
    }
}

void setup() {
    Serial.begin(115200); // Required for Serial.print / Serial.printf output
    delay(1000);

    WiFi.persistent(false);   // don't write Wi-Fi settings to flash on every change
    WiFi.onEvent(onWiFiEvent);

    // The mode is always set BEFORE starting the AP or station.
    switch (wifiMode) {
        case WiFiMode::STA:
            WiFi.mode(WIFI_STA);
            WiFi.setSleep(false);
            connectToWiFi(authType);
            break;

        case WiFiMode::AP:
            WiFi.mode(WIFI_AP);
            WiFi.setSleep(false);
            startAccessPoint();
            break;

        case WiFiMode::AP_STA:
            WiFi.mode(WIFI_AP_STA);
            WiFi.setSleep(false);
            startAccessPoint();
            connectToWiFi(authType);
            break;
    }

    lastReconnectAttempt = millis();
}

void logWiFiStatus() {
    Serial.println("\n--- [Wi-Fi Status Report] ---");
    Serial.printf("Uptime: %lu s | Free Heap: %lu bytes\n", millis() / 1000, ESP.getFreeHeap());
    Serial.printf("Channel: %d\n", (int)WiFi.channel());

    // 1. Log Station (STA) details if enabled
    if (staEnabled()) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[STA] Connected to: %s\n", WiFi.SSID().c_str());
            Serial.printf("[STA] IP Address:   %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[STA] Signal (RSSI): %d dBm\n", WiFi.RSSI());
        } else {
            Serial.printf("[STA] Disconnected (Status code: %d)\n", WiFi.status());
        }
    }

    // 2. Log Access Point (AP) details if enabled
    if (apEnabled()) {
        Serial.printf("[AP]  SSID:         %s\n", AP_SSID);
        Serial.printf("[AP]  AP IP:        %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("[AP]  Clients:      %d connected\n", WiFi.softAPgetStationNum());
    }

    Serial.printf("[NAT] %s\n", natEnabled ? "Enabled" : "Disabled");

    if (natEnabled || dns != IPAddress(0, 0, 0, 0)) {
        Serial.printf("[NAT] %s\n", dns.toString().c_str());
    }


    Serial.println("-----------------------------");
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. Periodic Status Logging (every 5 seconds)
    if (currentMillis - lastStatusUpdate >= STATUS_INTERVAL) {
        lastStatusUpdate = currentMillis;
        logWiFiStatus();
    }

    // 2. Station Auto-Reconnect Check (every 15 seconds)
    if (staEnabled()) {
        if (WiFi.status() != WL_CONNECTED) {
            if (currentMillis - lastReconnectAttempt >= RECONNECT_INTERVAL) {
                lastReconnectAttempt = currentMillis;
                Serial.println("[wifi]: connection lost. Attempting reconnect...");
                WiFi.reconnect();
            }
        }
    }

    // 3. DNS + NAT: set up once the station is connected (only needed in AP_STA mode)
    if (staEnabled() && apEnabled()) {
        if (WiFi.status() != WL_CONNECTED) {
            // Upstream is down: redo both when it returns (DNS may have changed too)
            natEnabled = false;
            dnsApplied = false;
        } else if ((!dnsApplied || !natEnabled) &&
                   currentMillis - lastNatAttempt >= NAT_RETRY_INTERVAL) {
            lastNatAttempt = currentMillis;

            // DNS first, then NAT
            if (!dnsApplied) {
                dnsApplied = updateApDns();
            }
            if (dnsApplied && !natEnabled) {
                natEnabled = enableNAT();
            }
        }
    }
}