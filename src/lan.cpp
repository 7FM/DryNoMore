#include "lan.hpp"
#include "serial.hpp"
#include "settings.hpp"

#ifdef USE_ETHERNET
#include "lan_protocol.hpp"
#include "macros.hpp"
#include <Ethernet.h>

#include "utility/w5100.h"

static constexpr uint8_t mac[] = MAC_ADDRESS;

// Set the static IP address to use if the DHCP fails to assign
static const IPAddress fallbackIP(FALLBACK_IP);
static const IPAddress fallbackDns(FALLBACK_DNS);

static const IPAddress serverIP(LOCAL_SERVER_IP);

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

#ifndef ETH_PWR_MAPPING
void powerUpEthernet();
void powerDownEthernet();
void setupEthernet();
#endif

void setupEthernet(
#ifdef ETH_PWR_MAPPING
    const ShiftReg &shiftReg
#endif
) {
#ifdef ETH_PWR_MAPPING
  shiftReg.update(ETH_PWR_MAPPING);
  shiftReg.enableOutput();
#endif
  Ethernet.init();
  W5100.init();
#ifdef ETH_PWR_MAPPING
  powerDownEthernet(shiftReg);
#else
  powerDownEthernet();
#endif
}

void powerUpEthernet(
#ifdef ETH_PWR_MAPPING
    const ShiftReg &shiftReg
#endif
) {

#ifdef ETH_PWR_MAPPING
  shiftReg.update(ETH_PWR_MAPPING);
  shiftReg.enableOutput();
#endif
  W5100.init();

  // Select 10 MBit half-duplex
  W5100.writePHYCFGR_W5500(W5500_RST_LOW | W5500_OPMD | W5500_OPM_10_HALF);
  // perform reset to ensure that the settings are applied
  W5100.writePHYCFGR_W5500(W5500_OPMD | W5500_OPM_10_HALF);
  W5100.writePHYCFGR_W5500(W5500_RST_LOW | W5500_OPMD | W5500_OPM_10_HALF);

  if (Ethernet.begin(mac, 10 /* tries */) == 0) {
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
    Ethernet.end();
    Ethernet.begin(mac, fallbackIP, fallbackDns);
  } else {
    SERIALprintP(PSTR("  DHCP assigned IP "));
    SERIALprintln(Ethernet.localIP());
  }

  client.connect(serverIP, LOCAL_SERVER_PORT);
}

void powerDownEthernet(
#ifdef ETH_PWR_MAPPING
    const ShiftReg &shiftReg
#endif
) {
  if (client.connected()) {
    client.stop();
  }

  // Enter power down mode
  W5100.writePHYCFGR_W5500(W5500_RST_LOW | W5500_OPMD | W5500_OPM_POWER_DOWN);
  // TODO stay in reset mode?
  W5100.writePHYCFGR_W5500(W5500_OPMD | W5500_OPM_POWER_DOWN);

  Ethernet.end();

#ifdef ETH_PWR_MAPPING
  // Ensure that no pin has a low level -> we want to power off the ethernet adapter!
  // sadly this will turn the SCK LED on :/
  pinMode(/*D*/13, INPUT_PULLUP); /*SCK*/
  pinMode(/*D*/12, INPUT_PULLUP); /*MISO*/
  pinMode(/*D*/11, INPUT_PULLUP); /*MOSI*/
  pinMode(/*D*/10, INPUT_PULLUP); /*CS*/

  shiftReg.disableOutput();
  shiftReg.update(0);
  shiftReg.enableOutput();
#else
  // Turn off SCK LED!
  digitalWrite(/*D*/13, LOW);
#endif
}

#ifndef ETH_PWR_MAPPING
void powerUpEthernet(const ShiftReg &shiftReg) { powerUpEthernet(); }
void powerDownEthernet(const ShiftReg &shiftReg) { powerDownEthernet(); }
void setupEthernet(const ShiftReg &shiftReg) { setupEthernet(); }
#endif

void sendStatus(const Status &status) {
  uint8_t buf[sizeof(status) + 1];
  buf[0] = REPORT_STATUS;
  memcpy(buf + 1, &status, sizeof(status));
  client.write(reinterpret_cast<const char *>(buf), sizeof(status) + 1);
  client.flush();
}

void sendWarning(uint8_t waterSensIdx) {
  uint8_t buf[] = {WARN_MSG, 'r', 'u', 'n', 'n', 'i', 'n', 'g', ' ', 'l',
                   'o',      'w', ' ', 'o', 'n', ' ', 'w', 'a', 't', 'e',
                   'r',      ' ', 'a', 't', ' ', 'w', 'a', 't', 'e', 'r',
                   ' ',      'l', 'e', 'v', 'e', 'l', ' ', 's', 'e', 'n',
                   's',      'o', 'r', ' ', 'X', '!'};
  buf[CONST_ARRAY_SIZE(buf) - 2] = '0' + waterSensIdx;
  client.write(reinterpret_cast<const char *>(buf), CONST_ARRAY_SIZE(buf));
  client.flush();
}

void updateSettings(Settings &settings) {
  uint8_t buf[sizeof(settings)];
  buf[0] = REQUEST_SETTINGS;
  client.write(reinterpret_cast<const char *>(buf), 1);
  client.flush();
  unsigned readBytes = client.read(buf, sizeof(settings));
  if (readBytes < sizeof(settings)) {
    // the server has no settings stored, yet -> sending our current settings to
    // the server
    client.write(reinterpret_cast<const char *>(&settings), sizeof(settings));
    client.flush();
  } else {
    // the server has settings stored -> update ours!
    memcpy(&settings, buf, sizeof(settings));
  }
  // TODO pray for no endianess issues
}

void sendErrorWaterEmpty(uint8_t waterSensIdx) {
  uint8_t buf[] = {ERR_MSG, 'w', 'a', 't', 'e', 'r', ' ', 'r', 'e', 's',
                   'e',     'r', 'v', 'o', 'i', 'r', ' ', 'X', ' ', 'i',
                   's',     ' ', 'e', 'm', 'p', 't', 'y', '!'};
  buf[CONST_ARRAY_SIZE(buf) - 11] = '1' + waterSensIdx;
  client.write(reinterpret_cast<const char *>(buf), CONST_ARRAY_SIZE(buf));
  client.flush();
}

void sendErrorHardware(uint8_t moistSensIdx) {
  uint8_t buf[] = {
      FAILURE_MSG, 'i', 'r', 'r', 'i', 'g', 'a', 't', 'i', 'o', 'n', ' ',
      't',         'i', 'm', 'e', 'd', ' ', 'o', 'u', 't', ',', ' ', 'a',
      's',         's', 'u', 'm', 'i', 'n', 'g', ' ', 'a', ' ', 'h', 'a',
      'r',         'd', 'w', 'a', 'r', 'e', ' ', 'f', 'a', 'i', 'l', 'u',
      'r',         'e', ' ', 'a', 't', ' ', 'e', 'i', 't', 'h', 'e', 'r',
      ' ',         't', 'h', 'e', ' ', 'm', 'o', 'i', 's', 't', 'u', 'r',
      'e',         ' ', 's', 'e', 'n', 's', 'o', 'r', ' ', 'X', ' ', 'o',
      'r',         ' ', 'i', 't', 's', ' ', 'p', 'u', 'm', 'p', '!'};
  buf[CONST_ARRAY_SIZE(buf) - 14] = '1' + moistSensIdx;
  client.write(reinterpret_cast<const char *>(buf), CONST_ARRAY_SIZE(buf));
  client.flush();
}
#endif
