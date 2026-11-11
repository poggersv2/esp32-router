#pragma once

extern bool natEnabled;
extern bool dnsApplied;
extern IPAddress dns;

bool updateApDns();
bool enableNAT();