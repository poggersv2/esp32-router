#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

const int AP_CHANNEL = 1;
const int AP_MAX_CLIENTS = 4;

bool startAccessPoint() {
    Serial.println("\n[ap]: starting access point...");

    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),     // AP IP
        IPAddress(192, 168, 4, 1),     // gateway
        IPAddress(255, 255, 255, 0),   // subnet mask
        IPAddress(192, 168, 4, 2),     // first address handed to clients
        IPAddress(1, 1, 1, 1)          // DNS server clients are told to use
    );

    // Uses your AP_CHANNEL and AP_MAX_CLIENTS constants
    if (WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS)) {
        Serial.println("[success(ap)]: AP started! IP: " + WiFi.softAPIP().toString());
        return true;
    } else {
        Serial.println("error(ap): failed to start access point.");
        return false;
    }
}
