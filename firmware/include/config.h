#pragma once

static const char *kHostname = "netwatch";
static const uint16_t kHttpPort = 80;
static const uint32_t kCheckIntervalMs = 5000;
static const char *kInternetHost = "1.1.1.1";
static const char *kExtraHost = "8.8.8.8";
static const char *kSetupApName = "NETWATCH-SETUP";
static const size_t kHistoryCap = 64;
static const char *kDefaultPcEth = "";
static const char *kDefaultPcWifi = "";
static const uint16_t kPcTcpFallbackPorts[] = {445, 135, 3389, 80};
static const size_t kPcTcpFallbackCount =
    sizeof(kPcTcpFallbackPorts) / sizeof(kPcTcpFallbackPorts[0]);
static const uint32_t kConfigSchema = 3;
