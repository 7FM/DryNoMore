#include "lan.hpp"
#include "serial.hpp"
#include "settings.hpp"

#ifdef USE_ETHERNET
#include "lan_protocol.hpp"
#include <Ethernet.h>

#include "utility/w5100.h"

static constexpr uint8_t mac[] = MAC_ADDRESS;

// Set the static IP address to use if the DHCP fails to assign
static IPAddress fallbackIP(FALLBACK_IP);
static IPAddress fallbackDns(FALLBACK_DNS);

// initialize the library instance:
static EthernetClient client;

/* PHYCFGR register:
Bit: Symbol: Description:
7    RST     Reset [R/W]: reset on 0 value -> set to 1 for normal operation
6    OPMD    Operation Mode: 1: Configure with OPMDC 0: Configure with HW pins
5-3  OPMDC   Operation Mode Configuration Bit [R/W]
             MSb LSb Description
             0 0 0   10 MBit half-duplex, auto-negotiation disabled
             0 0 1   10 MBit full-duplex, auto-negotiation disabled
             0 1 0   100 MBit half-duplex, auto-negotiation disabled
             0 1 1   100 MBit full-duplex, auto-negotiation disabled
             1 0 0   100 MBit full-duplex, auto-negotiation enabled
             1 0 1   Not used
             1 1 0   Power down mode
             1 1 1   All capable, auto-negotiation enabled
2    DPX     Duplex Status [Read only]: 1: Full duplex 0: Half duplex
1    SPD     Speed Status [Read only]: 1: 100 MBit 0: 10 MBit
0    LNK     Link Status [Read only]: 1: Link up 0: Link down
*/
#define W5500_RST_LOW 0x80
#define W5500_OPMD 0x40
#define W5500_OPM_10_HALF ((0b000) << 3)
#define W5500_OPM_10_FULL ((0b001) << 3)
#define W5500_OPM_100_HALF ((0b010) << 3)
#define W5500_OPM_100_FULL ((0b011) << 3)
#define W5500_OPM_100_FULL_AUTO_NEG ((0b100) << 3)
#define W5500_OPM_POWER_DOWN ((0b110) << 3)
#define W5500_OPM_ALL ((0b111) << 3)

void setupEthernet() {
  Ethernet.init();
  W5100.init();
  powerDownEthernet();
}

void powerUpEthernet() {
  // Select 10 MBit half-duplex
  W5100.writePHYCFGR_W5500(W5500_RST_LOW | W5500_OPMD | W5500_OPM_10_HALF);
  // perform reset to ensure that the settings are applied
  W5100.writePHYCFGR_W5500(W5500_OPMD | W5500_OPM_10_HALF);
  W5100.writePHYCFGR_W5500(W5500_RST_LOW | W5500_OPMD | W5500_OPM_10_HALF);

  if (Ethernet.begin(mac) == 0) {
    SERIALprintlnP(PSTR("Failed to configure Ethernet using DHCP"));
    // Check for Ethernet hardware present
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      SERIALprintlnP(PSTR("Ethernet shield was not found. Sorry, can't run "
                          "without hardware. :("));
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
}

void powerDownEthernet() {
  // Enter power down mode
  W5100.writePHYCFGR_W5500(W5500_RST_LOW | W5500_OPMD | W5500_OPM_POWER_DOWN);
  // TODO stay in reset mode?
  W5100.writePHYCFGR_W5500(W5500_OPMD | W5500_OPM_POWER_DOWN);
}

void sendStatus(const Status &status) {
  // TODO
}
void sendWarning(uint8_t waterSensIdx) {
  // TODO
}

void updateSettings(Settings &settings) {
  // TODO
}

// TODO unify
void sendErrorWaterEmpty(uint8_t waterSensIdx) {
  // TODO
}
void sendErrorHardware(uint8_t moistSensIdx, uint8_t waterSensIdx) {
  // TODO
}
#endif
