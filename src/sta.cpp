#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_eap_client.h>
#include "secrets.h"

// Returns true if the ESP-IDF call succeeded, otherwise prints the error.
bool wpa2Check(esp_err_t error, const char* step) {
    if (error == ESP_OK) return true;
    Serial.printf("\nerror(wpa2): failed to %s - %s\n", step, esp_err_to_name(error));
    return false;
}

bool connectToEAPWiFi() {
    Serial.println("\n[wpa2]: initialising enterprise authentication...");

    // Each step runs in order and stops at the first failure.
    bool ok =
        wpa2Check(esp_eap_client_set_identity((uint8_t*)EAP_IDENTITY, strlen(EAP_IDENTITY)), "configure identity") &&
        wpa2Check(esp_eap_client_set_username((uint8_t*)EAP_USERNAME, strlen(EAP_USERNAME)), "configure username") &&
        wpa2Check(esp_eap_client_set_password((uint8_t*)EAP_PASSWORD, strlen(EAP_PASSWORD)), "configure password") &&
        wpa2Check(esp_wifi_sta_enterprise_enable(), "enable enterprise authentication");

    if (!ok) return false;

    WiFi.begin(STA_EAP_SSID);
    Serial.println("[success(wpa2)]: enterprise authentication configured successfully.");
    return true;
}

bool connectToWPAWiFi() {
    Serial.println("\n[wpa]: connecting to personal Wi-Fi...");
    WiFi.begin(STA_WPA_SSID, WPA_PASSWORD);
    return true;
}

