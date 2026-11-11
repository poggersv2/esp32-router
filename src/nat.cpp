#include <Arduino.h>
#include <esp_netif.h>
#include <WiFi.h>
#include "nat.h"
#include "dhcpserver/dhcpserver.h"

bool natEnabled = false;
bool dnsApplied = false;

bool enableNAT() {
    // "WIFI_AP_DEF" is the name ESP-IDF gives the default AP interface
    esp_netif_t* apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    if (apNetif == nullptr) {
        Serial.println("error(nat): AP network interface not found.");
        return false;
    }

    esp_err_t error = esp_netif_napt_enable(apNetif);

    if (error != ESP_OK) {
        Serial.printf("error(nat): failed to enable NAPT - %s\n", esp_err_to_name(error));
        return false;
    }

    Serial.println("[success(nat)]: NAPT enabled on the AP interface.");
    return true;
}

// Tells the AP's DHCP server to hand clients the DNS server the station learned upstream.
bool updateApDns() {
    IPAddress dns = WiFi.dnsIP();

    if (dns == IPAddress(0, 0, 0, 0)) {
        Serial.println("error(dns): upstream DNS not known yet.");
        return false;
    }

    esp_netif_t* apNetif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (apNetif == nullptr) {
        Serial.println("error(dns): AP network interface not found.");
        return false;
    }

    esp_netif_dns_info_t dnsInfo = {};
    dnsInfo.ip.type = ESP_IPADDR_TYPE_V4;
    dnsInfo.ip.u_addr.ip4.addr = (uint32_t)dns;

    dhcps_offer_t offerDns = OFFER_DNS;

    // DHCP options can only be changed while the DHCP server is stopped.
    esp_netif_dhcps_stop(apNetif);

    esp_err_t error = esp_netif_dhcps_option(
        apNetif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
        &offerDns, sizeof(offerDns));

    if (error == ESP_OK) {
        error = esp_netif_set_dns_info(apNetif, ESP_NETIF_DNS_MAIN, &dnsInfo);
    }

    esp_netif_dhcps_start(apNetif);   // always restart it, even if setting failed

    if (error != ESP_OK) {
        Serial.printf("error(dns): failed to set AP DNS - %s\n", esp_err_to_name(error));
        return false;
    }

    Serial.println("[success(dns)]: AP clients will be told to use " + dns.toString());
    return true;
}