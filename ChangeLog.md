# ChangeLog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)
and its stricter, better defined, brother [Common Changelog](https://common-changelog.org/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

***


## [Unreleased]
****
### Changed
- Moved the tool sketches into the library's examples (`examples/tools/AT_Debug`, `AT_Spy`, `Diagnostics`, `FactoryReset`), so they appear under File → Examples → TinyGSM2 → tools. The compile-test sketch moved to `extras/test_build`. This is required by the Arduino Library Manager (Arduino Lint rule LD003).

### Added
- `TinyGSM2.h` primary header, named after the library (it includes `TinyGsmClient.h`).

### Removed

### Fixed

***


## [v1.0.0]
TinyGSM2: fork of TinyGSM 0.12.0 (https://github.com/vshymanskyy/TinyGSM), published at https://github.com/longtrinh/TinyGSM2.

### Changed
- Library renamed to `TinyGSM2` in `library.properties` and `library.json`; the headers (`TinyGsmClient.h`, ...) are unchanged.
- First TinyGSM2 release: version 1.0.0, with a new README for the fork (the changes were made by Claude, Anthropic's AI coding agent, under the maintainer's direction).
- On ESP32, `TINY_GSM_YIELD()` now blocks for one FreeRTOS tick (`vTaskDelay(1)`) instead of `delay(0)`, so long waits let the IDLE task run and no longer trip the task watchdog (which reboots by default on arduino-esp32 3.x).
- Yields happen only while waiting for the modem, never per received byte: removed the per-byte yield in `waitResponse()`, and `GsmClient::write()`/`available()`/`read()` (and the XBee client) only yield when there is nothing to do.
- On ESP32, `TINY_GSM_RX_BUFFER` defaults to 1024 bytes (it is still 64 elsewhere).
- Socket payloads are moved from the modem stream into the client FIFO in chunks with `readBytes()` (new `moveCharsFromStreamToFifo()`), instead of one `read()` per byte.
- `TinyGsmAutoBaud()` keeps the pins and buffer sizes of an ESP32 `HardwareSerial` that is already running (it uses `updateBaudRate()`), and probes each rate with a 100 ms timeout.
- `TinyGsmIpFromString()` takes the first address of a dual-stack answer and on ESP32 also parses IPv6 addresses; connecting to an IPv6 `IPAddress` on ESP32 passes the IPv6 string to the modem instead of a garbled IPv4 address.
- A7672X: removed the forced Google DNS from `gprsConnect()`; define `TINY_GSM_A7672X_DNS` to override the carrier DNS.
- A7672X: the SSL version is no longer forced to TLS 1.2 (the value meant something else on TLS 1.3 firmware); define `TINY_GSM_A7672X_SSL_VERSION` to force one.
- A7672X: `getRegistrationStatus()` checks CEREG, then CGREG, then CREG (LTE Cat-1 modules register on EPS).

### Added
- Optional `TINY_GSM_USE_MUTEX` (ESP32) to serialize modem access from several FreeRTOS tasks, and `TINY_GSM_LOCK(modem)` to hold the lock across several calls.
- `TinyGsmSerialSetBaud()` helper.
- A7672X: `gprsConnect()` uses the APN user name and password (`AT+CGAUTH`).
- A7672X: secure clients verify the server (`authmode` 1) when a CA certificate has been set with `setCertificate()`.

### Removed
- The upstream TinyGSM README (replaced by a TinyGSM2 README).
- A7672X: the hard-coded Google DNS (`AT+CDNSCFG="8.8.8.8","8.8.4.4"`) in `gprsConnect()`.
- A7672X: the hard-coded `sslversion` 3 (TLS 1.2) in secure `connect()`.

### Fixed
- Clients unregister themselves from the modem when destroyed, so URCs no longer write into a destroyed (e.g. local) `TinyGsmClient`.
- Default-constructed clients initialize all members; calling `connected()`, `available()`, `stop()` etc. before `init()` no longer dereferences a garbage pointer.
- A read time-out in the socket read loops no longer stores `0xFF` (`-1`) bytes into the received data.
- `waitResponse()` no longer drops received bytes >= 0x80.
- `streamGetLength()` no longer overflows when more than 127 bytes are buffered.
- With `TINY_GSM_DEBUG`, reading a `+CME ERROR` / `+CMS ERROR` detail no longer waits for the full stream time-out.
- `GsmClient::peek()` returns -1 when there is no data instead of a stale byte, and pulls data waiting in the modem's buffer first.
- `GsmClient::read()` for modems without an internal buffer returns as soon as some data was read instead of waiting for the full time-out.
- `write()` no longer returns a huge `size_t` when the driver reports an error.
- A7672X: `init()` sent `ATV0` (numeric result codes) in non-debug builds, so the modem never answered "OK"; it now uses `AT+CMEE` like the other drivers and forces `ATV1`.
- A7672X: TCP `write()` waited 10 s for `+CCHSEND` and returned 0; it now parses `+CIPSEND: <link>,<req>,<cnf>`, and splits large writes into 1500 (TCP) / 2048 (SSL) byte chunks.
- A7672X: `AT+NETOPEN` / `AT+CCHSTART` are only sent when the service is not running, so reconnecting no longer fails with "Network is already opened".
- A7672X: the `mux` number is used for `AT+CIPOPEN` / `AT+CCHOPEN` (it was hard-coded to 0), and SSL vs. TCP is tracked per socket instead of by one global flag.
- A7672X: SSL reads follow the `AT+CCHRECV` response order (OK first, then one or more DATA blocks, then the result), and the number of bytes left is queried afterwards.
- A7672X: the `+CCHEVENT: <id>,RECV EVENT` URC no longer sends an AT command from inside `waitResponse()`.
- A7672X: `getTemperature()` always returned 0.
- A7672X: `restart()` waits for the module to reboot after `AT+CRESET`.
- A7672X: TCP connection state is read with `AT+CIPCLOSE?`, and `+CIPEVENT`, `+CCH: CCH STOP` and late command results are handled.

***


## [v0.12.0]

### Changed
- Moved bulk of waitResponse function to modem template and gave modems handleURCs fxn
- Moved option in waitResponse for verbose outputs.
- setBaud now returns a bool
- Replace new line consts with defines and renamed to AT_NL
- Renamed all RegStatus enums to be unique
- Replaced `NULL` with `nullptr` and replaced c-style casts.
- Moved setCertificates function and the certificate name matrix to the SSL template.
- Changed inputs for (unimplemented) SSL certificate functions.
- All modems will now return the pre-defined manufacturer and model in the name if the function to get the internal name fails.
- Cleaned up code for getting modem names.
- Made battery return types signed.

### Added
- Added support for SSL for the Quentcel BG95 and BG96 from [Aurelien BOUIN](https://github.com/aurelihein) and [George O'Connor](https://github.com/georgeman93)
- Added support for UBLOX SARA-R5 from [Sebastian Bergner](https://github.com/sebastianbergner)
- Added support for SIMCOM A7672X from [Giovanni de Rosso Unruh](https://github.com/giovannirosso)
- Added SIM5320 GPS location from [Bengarman](https://github.com/Bengarman)
- Added functions `getModemSerialNumber`, `getModemModel`, and `getModemRevision`.
- Added deep debugging option
- Added documentation to the FIFO class

### Removed
- Removed non-functional factory reset from SIM70xx series

### Fixed
- Removed extra wait on SIM7000 from [Mikael Fredriksson](https://github.com/Gelemikke)
- Fix status returns on ESP8266/ESP32 AT commands
- Fix length of HEX for Sequans Monarch
- Fix SIM7600 password then user when cid is set from [github0013](https://github.com/github0013)
- Fix cardinal points in location by gps for SIM7600 from [Juxn3](https://github.com/Juxn3)
- Fix NTP server sync for SIM70xx models from [Gonzalo Brusco](https://github.com/gonzabrusco)
- Fixed SIM70xx inheritance

***
