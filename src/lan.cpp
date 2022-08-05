#include "lan.hpp"
#include "serial.hpp"
#include "settings.hpp"

#ifdef USE_ETHERNET
#include <Ethernet.h>

static constexpr uint8_t mac[] = MAC_ADDRESS;

// Set the static IP address to use if the DHCP fails to assign
static IPAddress fallbackIP(FALLBACK_IP);
static IPAddress fallbackDns(FALLBACK_DNS);

// initialize the library instance:
static EthernetClient client;

static bool setupEth;

void initEthernet() {
    setupEth = false;
    Ethernet.init();

    if (Ethernet.begin(mac) == 0) {
        SERIALprintlnP(PSTR("Failed to configure Ethernet using DHCP"));
        // Check for Ethernet hardware present
        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            SERIALprintlnP(PSTR("Ethernet shield was not found. Sorry, can't run without hardware. :("));
            setupEth = false;
            return;
        }
        if (Ethernet.linkStatus() == LinkOFF) {
            SERIALprintlnP(PSTR("Ethernet cable is not connected."));
        }
        // try to configure using IP address instead of DHCP:
        Ethernet.begin(mac, fallbackIP, fallbackDns);
    } else {
        SERIALprintP(PSTR("  DHCP assigned IP "));
        SERIALprintln(Ethernet.localIP());
    }
    setupEth = true;
}
void deinitEthernet() {
    // TODO what can we do here?
    // TODO go into energy saving mode!
    setupEth = false;
}

void sendStatus() {
    // TODO
}
void sendWarning() {
    // TODO
}

void updateSettings(Settings &settings) {
    // TODO
}

// TODO unify
void sendErrorWaterEmpty() {
    // TODO
}
void sendErrorHardware() {
    // TODO
}
#endif
