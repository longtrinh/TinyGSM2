# TinyGSM2

A small Arduino library for GSM, LTE and WiFi modules with an AT-command interface, updated for **arduino-esp32 3.x** (ESP-IDF 5.x). It still builds on arduino-esp32 2.x.

[![CI](https://github.com/longtrinh/TinyGSM2/actions/workflows/ci.yml/badge.svg)](https://github.com/longtrinh/TinyGSM2/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-1.0.3-blue.svg)](https://github.com/longtrinh/TinyGSM2/releases)
[![License](https://img.shields.io/badge/license-LGPL--3.0-blue.svg)](LICENSE)

TinyGSM2 is a fork of [TinyGSM](https://github.com/vshymanskyy/TinyGSM) 0.12.0 by Volodymyr Shymanskyy and contributors.
It keeps the same API, headers and class names, so existing TinyGSM sketches compile unchanged.
It also fixes the stability problems that appear with the current ESP32 Arduino core, and reworks the SIMCom A7672X (A7670/A7672) driver.

> **🤖 Modified by an AI agent**
>
> The changes in TinyGSM2 were made by **Claude**, Anthropic's AI coding agent (Claude Code, model Claude Opus 5.5), working under the direction of [longtrinh](https://github.com/longtrinh).
> Claude analyzed TinyGSM 0.12.0 against the arduino-esp32 3.3.12 source and the SIMCom A76XX AT Command Manual V2.04.
> It then implemented the fixes, the README and the changelog.
>
> Every modem driver compiles warning-free with the arduino-esp32 3.3.12 toolchain and the ESP-IDF 6.1 toolchain (`-Werror=all`), with and without `TINY_GSM_USE_MUTEX`.
> Full linked builds also passed for ESP32, ESP32-S3, ESP32-C3, ESP8266, AVR, and PlatformIO (official `espressif32` platform).
> All example sketches compile for every supported modem on ESP32-S3, Arduino Mega and Arduino Uno.
> PubSubClient (legacy 2.8 and PubSubClient3) and ESP_SSLClient (2.2.2 and 3.1.3) were tested on top of TinyGSM2 with a simulated modem; the PubSubClient tests and a model of the ESP_SSLClient socket logic ([extras/tests](extras/tests)) run in CI on every push.
> The changes have **not yet been tested on real modem hardware**, so please test them in your setup and report any problems in [Issues](https://github.com/longtrinh/TinyGSM2/issues).

- [Why TinyGSM2?](#why-tinygsm2)
- [Supported modems](#supported-modems)
- [Features](#features)
- [Installation](#installation)
- [Quick start (ESP32 + A7672X)](#quick-start-esp32--a7672x)
- [Configuration options](#configuration-options)
- [ESP32 notes](#esp32-notes)
- [PlatformIO and ESP-IDF](#platformio-and-esp-idf)
- [Migrating from TinyGSM](#migrating-from-tinygsm)
- [Troubleshooting](#troubleshooting)
- [Credits](#credits)
- [License](#license)

## Why TinyGSM2?

TinyGSM 0.12.0 was released in May 2024, before arduino-esp32 3.x matured.
On current cores (tested against 3.3.12, ESP-IDF 5.5, GCC 14) it still compiles, but it misbehaves at runtime.
TinyGSM2 fixes this:

**ESP32 / FreeRTOS**
- Waits no longer starve the IDLE task, so long modem operations stop triggering task-watchdog reboots.
- The library yields only while it is waiting, never once per received byte, so modem data bursts are not lost.
- Socket data is read in chunks, and a read time-out no longer inserts `0xFF` bytes into your data.
- Bigger receive buffers are safe: each read stays within the modem's per-request limit. The default is still 64 bytes, as in TinyGSM.
- IPv6-aware handling of the dual-stack `IPAddress` in core 3.x. Core 2.x (used by PlatformIO's `espressif32` platform) still builds, with IPv4 only.
- `TinyGsmAutoBaud()` keeps the pins and buffer sizes of an already started `HardwareSerial`.
- Optional mutex (`TINY_GSM_USE_MUTEX`) so several FreeRTOS tasks can share one modem safely.

**All modems**
- Clients unregister from the modem when destroyed. A local `TinyGsmClient` no longer causes random crashes later.
- A client that has not been `init()`'ed yet is safe to use; PubSubClient, for example, calls `connected()` before connecting.
- Bytes >= 0x80 are no longer dropped from responses, `peek()` follows the Arduino contract, and several length/overflow bugs are fixed.
- Large writes are split into chunks the modem accepts, so MQTT packets and TLS records bigger than the modem's per-send limit (e.g. 1500 bytes on the SIM7600) no longer fail.
- A send the modem refuses sets `getWriteError()`, so TLS libraries such as ESP_SSLClient can report it.
- Socket reads stay within each modem's per-request limit (e.g. 750 bytes in SIMCom HEX mode), and `localIP()` parses every modem's answer format.

**SIMCom A7672X**
- Release builds work: the old driver sent `ATV0` without `TINY_GSM_DEBUG`, which broke every response check.
- TCP `write()` works: it used to wait 10 s and return 0.
- Reconnecting works (no more "Network is already opened" errors).
- Multiple sockets are supported, with TCP and TLS tracked per socket.
- TLS receive was reworked to follow the AT manual.
- Optional CA verification.
- APN authentication.
- Fixed `getTemperature()`, LTE registration (CEREG), and `restart()`.

The full list is in [ChangeLog.md](ChangeLog.md).

## Supported modems

| Modem | Define |
|---|---|
| SIMCom SIM800 series (SIM800A/C/L/H) | `TINY_GSM_MODEM_SIM800` |
| SIMCom SIM808 / SIM868 | `TINY_GSM_MODEM_SIM808` / `TINY_GSM_MODEM_SIM868` |
| SIMCom SIM900 series (SIM900A/D, SIM908, SIM968) | `TINY_GSM_MODEM_SIM900` |
| SIMCom SIM5320 / SIM5360 / SIM5300 / SIM7100 | `TINY_GSM_MODEM_SIM5360` (or the matching name) |
| SIMCom SIM7500 / SIM7600 / SIM7800 | `TINY_GSM_MODEM_SIM7600` (or the matching name) |
| SIMCom SIM7000 (TCP/IP toolkit, up to 8 sockets, no SSL) | `TINY_GSM_MODEM_SIM7000` |
| SIMCom SIM7000 (application commands, SSL, 2 sockets) | `TINY_GSM_MODEM_SIM7000SSL` |
| SIMCom SIM7070 / SIM7080 / SIM7090 | `TINY_GSM_MODEM_SIM7080` (or the matching name) |
| **SIMCom A7672X (A7670, A7672) LTE Cat-1** | `TINY_GSM_MODEM_A7672X` |
| Quectel BG96 / BG95 | `TINY_GSM_MODEM_BG96` / `TINY_GSM_MODEM_BG95` / `TINY_GSM_MODEM_BG95SSL` |
| Quectel M95 | `TINY_GSM_MODEM_M95` |
| Quectel MC60 / MC60E | `TINY_GSM_MODEM_MC60` / `TINY_GSM_MODEM_MC60E` |
| u-blox 2G/3G/4G (LEON, LISA, SARA-G/U, TOBY-L2, LARA-R2, ...) | `TINY_GSM_MODEM_UBLOX` |
| u-blox SARA-R4 / N4 | `TINY_GSM_MODEM_SARAR4` |
| u-blox SARA-R5 | `TINY_GSM_MODEM_SARAR5` |
| AI-Thinker A6 / A7 / A20 | `TINY_GSM_MODEM_A6` / `TINY_GSM_MODEM_A7` |
| Neoway M590 | `TINY_GSM_MODEM_M590` |
| Sequans Monarch (VZM20Q) | `TINY_GSM_MODEM_SEQUANS_MONARCH` |
| Digi XBee WiFi / Cellular (command mode) | `TINY_GSM_MODEM_XBEE` |
| ESP8266 / ESP32 running the ESP-AT firmware | `TINY_GSM_MODEM_ESP8266` / `TINY_GSM_MODEM_ESP32` |

Define exactly **one** modem before `#include <TinyGsmClient.h>`. `#include <TinyGSM2.h>` works the same way.

## Features

- **TCP** on every modem, usually with several simultaneous sockets.
For example: A7672X 10, SIM7600 10, SIM800 5, BG96 12, SIM7080 12, XBee 1.
- **SSL/TLS** on SIM800 (firmware dependent), SIM7000SSL, SIM7080, A7672X, BG95/BG96, u-blox, Sequans Monarch, XBee cellular and ESP8266.
  - Not supported on SIM5360/SIM7600, SIM900, A6/A7, M590 and XBee WiFi.
  - TLS can also be done on the MCU with a library such as [ESP_SSLClient](https://github.com/mobizt/ESP_SSLClient) (2.2.2 and 3.1.3 tested) on top of a plain `TinyGsmClient`.
- **SMS** sending, **USSD**, **voice calls** (dial, hang up, DTMF) on most cellular modems.
- **GPS/GNSS** (SIM808, SIM7000, SIM7600, BG96/BG95, u-blox with external GPS) and **GSM location** (SIM800, SIM7000, Quectel, u-blox).
- **Network time / NTP**, battery and temperature readings on supported modems.
- The standard Arduino **`Client`** interface, so it works with PubSubClient (both the legacy 2.8 and **PubSubClient3**), ArduinoMqttClient, ArduinoHttpClient, Blynk and similar libraries. Large writes, such as MQTT packets of several kB set up with `setBufferSize()`, are split into chunks the modem accepts.

See [examples/AllFunctions](examples/AllFunctions/AllFunctions.ino) for the complete API.

## Installation

**Arduino IDE**
1. Remove the original TinyGSM library if you have it installed (both provide `TinyGsmClient.h`).
2. Open **Library Manager** (**Tools → Manage Libraries...**), search for **TinyGSM2** and click **Install**.
Alternatively, download a release ZIP and use **Sketch → Include Library → Add .ZIP Library...**, or clone the repository into your libraries folder:
   ```bash
   git clone https://github.com/longtrinh/TinyGSM2.git
   ```

**PlatformIO**: TinyGSM2 is in the [PlatformIO registry](https://registry.platformio.org/libraries/longtrinh/TinyGSM2) (search for "TinyGSM2" in the PlatformIO **Libraries** view):
```ini
lib_deps =
    longtrinh/TinyGSM2@^1.0.3
```
See [PlatformIO and ESP-IDF](#platformio-and-esp-idf) for platform choices and ESP-IDF use.

**Examples.** After installing, the examples are under **File → Examples → TinyGSM2**.
The helper sketches are in the **tools** sub-menu:
- `AT_Debug`: auto-detects the baud rate, then lets you type AT commands to the modem.
- `AT_Spy`: listens in on the AT traffic between another microcontroller and the modem.
- `Diagnostics`: check SIM, network and data step by step.
- `FactoryReset`: restore the modem's factory settings.

## Quick start (ESP32 + A7672X)

```cpp
#define TINY_GSM_MODEM_A7672X
#define TINY_GSM_RX_BUFFER 1024
// #define TINY_GSM_DEBUG Serial   // print library debug output
#include <TinyGsmClient.h>

#define SerialMon    Serial
#define SerialAT     Serial1
#define MODEM_RX_PIN 18  // ESP32 pin connected to the modem TX - change to your wiring
#define MODEM_TX_PIN 17  // ESP32 pin connected to the modem RX - change to your wiring

const char apn[] = "your-apn";

TinyGsm       modem(SerialAT);
TinyGsmClient client(modem, 0);  // global, never a local variable that goes out of scope

void setup() {
  SerialMon.begin(115200);

  SerialAT.setRxBufferSize(4096);  // must be called BEFORE begin()
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(3000);  // power the modem on (PWRKEY) before this, as your board requires

  if (!modem.init()) { SerialMon.println("Modem init failed"); return; }
  SerialMon.println(modem.getModemInfo());

  if (!modem.waitForNetwork(600000L)) { SerialMon.println("No network"); return; }
  if (!modem.gprsConnect(apn)) { SerialMon.println("No data connection"); return; }
  SerialMon.print("IP: ");
  SerialMon.println(modem.localIP());

  if (client.connect("example.com", 80)) {
    client.print("GET / HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    uint32_t last = millis();
    while (client.connected() && millis() - last < 10000L) {
      while (client.available()) {
        SerialMon.write(client.read());
        last = millis();
      }
    }
    client.stop();
  }
}

void loop() {}
```

For HTTPS on the A7672X use `TinyGsmClientSecure client(modem, 0);`.
The module has fewer TLS sessions than TCP sockets, so use mux 0 or 1 for secure clients.
To verify the server certificate, upload the CA with `modem.addCertificate(...)` and select it with `client.setCertificate("ca.pem")` before `connect()`.

## Configuration options

Define these **before** `#include <TinyGsmClient.h>`:

| Option | Default | Purpose |
|---|---|---|
| `TINY_GSM_MODEM_xxx` | (required) | Selects the modem driver; see [Supported modems](#supported-modems) |
| `TINY_GSM_RX_BUFFER` | 64 | Per-socket receive FIFO size (bytes). On ESP32, 1024 gives much better throughput, together with a larger UART buffer. |
| `TINY_GSM_SEND_MAX_SIZE` | 1024 | Largest chunk handed to the modem per send command. Bigger `write()`s (e.g. a large MQTT packet) are split automatically. |
| `TINY_GSM_DEBUG` | off | Stream for library debug output, e.g. `Serial` |
| `TINY_GSM_USE_MUTEX` | off | ESP32: serialize modem access across FreeRTOS tasks |
| `TINY_GSM_YIELD_MS` | 0 | Milliseconds to block while waiting for the modem (0 = one RTOS tick on ESP32) |
| `TINY_GSM_USE_HEX` | off | SIMCom SIM800/SIM7000/SIM5360/SIM7600: receive socket data in HEX mode |
| `TINY_GSM_A7672X_DNS` | carrier DNS | A7672X: DNS override, e.g. `"\"8.8.8.8\",\"8.8.4.4\""` |
| `TINY_GSM_A7672X_SSL_VERSION` | modem default (all) | A7672X: force a value for `AT+CSSLCFG="sslversion"` (its meaning depends on the firmware) |

## ESP32 notes

- **Faster socket reads (optional).** By default each read moves at most 63 bytes, like TinyGSM, which works with the core's default 256-byte UART buffer.
For more throughput, define `TINY_GSM_RX_BUFFER 1024`. The modem then sends up to 1–2 kB per read, so also enlarge the UART buffer: call `SerialAT.setRxBufferSize(4096)` before `SerialAT.begin(...)`.
Always pass your RX/TX pins to `begin()`.
- **Use one task, or turn on the mutex.** TinyGSM2 blocks while it waits for the modem.
If more than one FreeRTOS task uses the modem, define `TINY_GSM_USE_MUTEX`.
To keep a group of calls together (or to protect functions that only exist in one modem driver), hold the lock yourself:
  ```cpp
  {
    TINY_GSM_LOCK(modem);
    modem.sendAT("+CSQ");
    modem.waitResponse();
  }
  ```
- **Debug output on ESP32-S3 USB.** With "USB CDC On Boot" enabled, writes to `Serial` can block for up to ~2 s when the host is not reading.
Don't route `TINY_GSM_DEBUG` there in production, or call `Serial.setTxTimeoutMs(0)`.
- **Core `HTTPClient` / `HTTPUpdate` cannot use a modem client.** They only accept a `NetworkClient`.
Use [ArduinoHttpClient](https://github.com/arduino-libraries/ArduinoHttpClient) instead, and stream OTA downloads into `Update.h` yourself.
- **Alternative: PPP.** arduino-esp32 3.x also ships a native `PPP` library.
It turns the modem into a full lwIP network interface, so `NetworkClient`, `NetworkClientSecure`, `HTTPClient` and the rest work unchanged.
That can be a good fit for TCP/TLS-heavy projects; TinyGSM2 remains handy for SMS, calls, GNSS and modem management.

## PlatformIO and ESP-IDF

### VS Code + PlatformIO (Arduino framework)

Install TinyGSM2 from the PlatformIO registry, pinned to a version range:

```ini
[env:esp32-s3]
platform  = espressif32
board     = esp32-s3-devkitc-1
framework = arduino
lib_deps  = longtrinh/TinyGSM2@^1.0.3
build_flags = -D TINY_GSM_MODEM_A7672X
```

- **Official `espressif32` platform.** It ships arduino-esp32 **2.0.x**. TinyGSM2 builds and runs there, but IPv6 `IPAddress` support is off (it needs core 3.x).
Verified with PlatformIO Core 6.2 / platform 7.1.3 (Arduino 2.0.17) on ESP32-S3 and ESP32-C3.
- **arduino-esp32 3.x in PlatformIO.** Use the community [pioarduino](https://github.com/pioarduino/platform-espressif32) platform, which ships the same 3.x core that the Arduino IDE uses:
  ```ini
  platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
  ```
- **`framework = espidf` alone is not supported.**
TinyGSM2 is built on the Arduino API (`Stream`, `Client`, `String`, `millis()`), so PlatformIO skips it in a pure ESP-IDF project.
Use `framework = arduino, espidf` (Arduino as an ESP-IDF component) on a platform that supports it, such as pioarduino.
- **VS Code IntelliSense:** to get code highlighting when you open the library's `.tpp` template files, add this to `.vscode/settings.json`:
  ```json
  "files.associations": { "*.tpp": "cpp" }
  ```

### ESP-IDF (`idf.py`) with Arduino as a component

TinyGSM2 includes a `CMakeLists.txt`, so it can be used as an ESP-IDF component.
It requires the arduino-esp32 component (3.x supports ESP-IDF 5.3 – 6.1).

1. Add Arduino to your ESP-IDF project:
   ```bash
   idf.py add-dependency "espressif/arduino-esp32^3.3.0"
   ```
2. Clone TinyGSM2 into the project's `components` folder:
   ```bash
   git clone https://github.com/longtrinh/TinyGSM2.git components/TinyGSM2
   ```
3. In `main/CMakeLists.txt`, add `REQUIRES TinyGSM2`, and define your modem before including the header (or add it with `target_compile_definitions`).
4. Configure the project for Arduino as Espressif describes in [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html). For example, `CONFIG_FREERTOS_HZ=1000`.

TinyGSM2 declares `REQUIRES arduino-esp32`. That name matches the component added in step 1.
If you copied arduino-esp32 into `components/` by hand, name that folder `arduino-esp32`, not `arduino`.

TinyGSM2 compiles warning-free with the ESP-IDF 6.1 toolchain (GCC 15.2) using `-Wall -Wextra -Werror=all`.
A complete `idf.py` application build has not been tested yet.

## Migrating from TinyGSM

TinyGSM2 is a drop-in replacement: same headers, same `TinyGsm` / `TinyGsmClient` / `TinyGsmClientSecure` types.

1. Remove the old `TinyGSM` folder from your Arduino `libraries` directory and install TinyGSM2.
2. Rebuild your sketch.
3. Review these behaviour changes:
   - Nothing to change for the receive buffer: `TINY_GSM_RX_BUFFER` still defaults to 64.
   - A7672X: `gprsConnect()` keeps the carrier DNS (it used to force Google DNS). Define `TINY_GSM_A7672X_DNS` to override it.
   - A7672X: the TLS version is no longer forced to TLS 1.2. Define `TINY_GSM_A7672X_SSL_VERSION` if your server needs a specific one.
   - A7672X: when a CA certificate is set, the server certificate is now actually verified (`authmode` 1).

## Troubleshooting

- **Power first.** Most modules draw up to 2 A in bursts; an insufficient supply causes resets and failed registrations.
- **First steps.**
Disable the SIM PIN (or pass it to `init()`), check that the APN is correct, and attach the antenna.
Then test the serial link with [examples/tools/AT_Debug](examples/tools/AT_Debug/AT_Debug.ino).
- **See the AT traffic.** Define `TINY_GSM_DEBUG Serial`.
To dump every AT command, wrap the modem stream with [StreamDebugger](https://github.com/vshymanskyy/StreamDebugger):
  ```cpp
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
  ```
- **Diagnostics.** Run [examples/tools/Diagnostics](examples/tools/Diagnostics/Diagnostics.ino) to check SIM, network and data connection step by step.
- **Bad configuration.** If the module was left in a strange state, [examples/tools/FactoryReset](examples/tools/FactoryReset/FactoryReset.ino) restores factory defaults.
- **Baud rate.** `TinyGsmAutoBaud()` is for bench testing only. In production, use a fixed baud rate.
- **First connection is slow.** A new SIM, module or location can take many minutes to register. Use generous `waitForNetwork()` time-outs.
- **HTTP "bad request".** TinyGSM2 gives you a TCP/TLS socket; you must format HTTP yourself or use a library such as ArduinoHttpClient.
Send each header in a single `print()`, and end the headers with an empty line.

## Credits

TinyGSM2 builds on the work of the TinyGSM authors and contributors:
- [Volodymyr Shymanskyy](https://github.com/vshymanskyy) – original author
- [SRGDamia1](https://github.com/SRGDamia1/) – primary contributor
- SIM7000: [captFuture](https://github.com/captFuture/), [FStefanni](https://github.com/FStefanni/)
- Sequans Monarch: [nootropicdesign](https://github.com/nootropicdesign/)
- Quectel MC60: [V1pr](https://github.com/V1pr)
- Quectel M95: [replicadeltd](https://github.com/replicadeltd)
- u-blox SARA-R5: [Sebastian Bergner](https://github.com/sebastianbergner)
- SIMCom A7672X: [Giovanni de Rosso Unruh](https://github.com/giovannirosso)
- All [TinyGSM contributors](https://github.com/vshymanskyy/TinyGSM/graphs/contributors)

TinyGSM2 maintainer: [longtrinh](https://github.com/longtrinh).
The ESP32 3.x / A7672X modifications were made by Claude (Anthropic's AI coding agent) under longtrinh's direction.

## License

Released under the GNU Lesser General Public License v3.0 ([LGPL-3.0](LICENSE)), like the original TinyGSM.
Copyright (c) 2016 Volodymyr Shymanskyy and TinyGSM contributors; modifications (c) 2026 longtrinh, made with Claude (Anthropic's AI coding agent).
