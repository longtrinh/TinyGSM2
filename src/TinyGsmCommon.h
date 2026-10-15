/**
 * @file       TinyGsmCommon.h
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMCOMMON_H_
#define SRC_TINYGSMCOMMON_H_

// The current library version number
#define TINYGSM_VERSION "1.0.0"

#if defined(SPARK) || defined(PARTICLE)
#include "Particle.h"
#elif defined(ARDUINO)
#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#endif

#if defined(ARDUINO_DASH)
#include <ArduinoCompat/Client.h>
#else
#include <Client.h>
#endif

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
#define TINY_GSM_PLATFORM_ESP32
#include <type_traits>
#endif

#ifndef TINY_GSM_YIELD_MS
#define TINY_GSM_YIELD_MS 0
#endif

// TINY_GSM_YIELD() is only called while waiting for the modem, never while
// data is flowing.  On ESP32 delay(0) is vTaskDelay(0), which never lets the
// IDLE task run; a long wait on core 0 then trips the task watchdog (which
// panics/reboots by default on arduino-esp32 3.x).  Block for at least one
// tick instead.
#ifndef TINY_GSM_YIELD
#if defined(TINY_GSM_PLATFORM_ESP32) && TINY_GSM_YIELD_MS == 0
#define TINY_GSM_YIELD() \
  { vTaskDelay(1); }
#else
#define TINY_GSM_YIELD() \
  { delay(TINY_GSM_YIELD_MS); }
#endif
#endif

/*
 * Optional modem lock for multi-task (FreeRTOS) use.
 * Define TINY_GSM_USE_MUTEX before including the library to serialize every
 * command/response transaction.  Functions that only exist in a specific
 * modem driver can be protected by the sketch with TINY_GSM_LOCK(modem).
 */
#if defined(TINY_GSM_USE_MUTEX) && defined(TINY_GSM_PLATFORM_ESP32)
#define TINY_GSM_MUTEX_ENABLED
class TinyGsmMutex {
 public:
  TinyGsmMutex() : _handle(xSemaphoreCreateRecursiveMutex()) {}
  ~TinyGsmMutex() {
    if (_handle) vSemaphoreDelete(_handle);
  }
  TinyGsmMutex(const TinyGsmMutex&)            = delete;
  TinyGsmMutex& operator=(const TinyGsmMutex&) = delete;
  void          lock() {
    if (_handle) xSemaphoreTakeRecursive(_handle, portMAX_DELAY);
  }
  void unlock() {
    if (_handle) xSemaphoreGiveRecursive(_handle);
  }

 private:
  SemaphoreHandle_t _handle;
};

template <class T>
class TinyGsmLockGuard {
 public:
  explicit TinyGsmLockGuard(T& modem) : _modem(modem) {
    _modem.modemMutex().lock();
  }
  ~TinyGsmLockGuard() {
    _modem.modemMutex().unlock();
  }
  TinyGsmLockGuard(const TinyGsmLockGuard&)            = delete;
  TinyGsmLockGuard& operator=(const TinyGsmLockGuard&) = delete;

 private:
  T& _modem;
};

#define TINY_GSM_LOCK(modem)                                              \
  TinyGsmLockGuard<typename std::remove_reference<decltype(modem)>::type> \
      tinyGsmLockGuard_(modem)
#else
#define TINY_GSM_LOCK(modem)
#endif

#define TINY_GSM_ATTR_NOT_AVAILABLE \
  __attribute__((error("Not available on this modem type")))
#define TINY_GSM_ATTR_NOT_IMPLEMENTED __attribute__((error("Not implemented")))

#if defined(__AVR__) && !defined(__AVR_ATmega4809__)
#define TINY_GSM_PROGMEM PROGMEM
typedef const __FlashStringHelper* GsmConstStr;
#define GFP(x) (reinterpret_cast<GsmConstStr>(x))
#define GF(x) F(x)
#else
#define TINY_GSM_PROGMEM
typedef const char* GsmConstStr;
#define GFP(x) x
#define GF(x) x
#endif

#ifdef TINY_GSM_DEBUG
namespace {
template <typename T>
static void DBG_PLAIN(T last) {
  TINY_GSM_DEBUG.println(last);
}

template <typename T, typename... Args>
static void DBG_PLAIN(T head, Args... tail) {
  TINY_GSM_DEBUG.print(head);
  TINY_GSM_DEBUG.print(' ');
  DBG_PLAIN(tail...);
}

template <typename... Args>
static void DBG(Args... args) {
  TINY_GSM_DEBUG.print(GF("["));
  TINY_GSM_DEBUG.print(millis());
  TINY_GSM_DEBUG.print(GF("] "));
  DBG_PLAIN(args...);
}
}  // namespace
#else
#define DBG_PLAIN(...)
#define DBG(...)
#endif

/*
 * CRTP Helper
 */
template <typename modemType, template <typename> class crtpType>
struct tinygsm_crtp {
  modemType& thisModem() {
    return static_cast<modemType&>(*this);
  }
  modemType const& thisModem() const {
    return static_cast<modemType const&>(*this);
  }

 private:
  tinygsm_crtp() {}
  friend crtpType<modemType>;
};

/*
 * Min/Max Helpers
 */
template <class T>
const T& TinyGsmMin(const T& a, const T& b) {
  return (b < a) ? b : a;
}

template <class T>
const T& TinyGsmMax(const T& a, const T& b) {
  return (b < a) ? a : b;
}

/*
 * Change the baud rate of the serial port used for the modem.
 * On ESP32 a HardwareSerial that is already running only gets its baud rate
 * updated, so the RX/TX pins and buffer sizes set by the sketch are kept.
 */
template <class T>
void TinyGsmSerialSetBaud(T& SerialAT, uint32_t rate) {
#if defined(TINY_GSM_PLATFORM_ESP32)
  if constexpr (std::is_base_of<HardwareSerial, T>::value) {
    if (SerialAT) {
      SerialAT.updateBaudRate(rate);
      return;
    }
  }
#endif
  SerialAT.begin(rate);
}

/*
 * Automatically find baud rate
 *
 * On ESP32, call SerialAT.setRxBufferSize() and SerialAT.begin(baud, config,
 * rx, tx) with your pins first; this function then only changes the rate.
 */
template <class T>
uint32_t TinyGsmAutoBaud(T& SerialAT, uint32_t minimum = 9600,
                         uint32_t maximum = 115200) {
  static uint32_t rates[] = {115200, 57600,  38400, 19200, 9600,  74400, 74880,
                             230400, 460800, 2400,  4800,  14400, 28800};
#if defined(TINY_GSM_PLATFORM_ESP32)
  // Each probe waits for the full stream timeout; 100 ms is plenty for "OK"
  unsigned long oldTimeout = SerialAT.getTimeout();
  SerialAT.setTimeout(100);
#endif

  for (uint8_t i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
    uint32_t rate = rates[i];
    if (rate < minimum || rate > maximum) continue;

    DBG("Trying baud rate", rate, "...");
    TinyGsmSerialSetBaud(SerialAT, rate);
    delay(10);
    for (int j = 0; j < 10; j++) {
      SerialAT.print("AT\r\n");
      String input = SerialAT.readString();
      if (input.indexOf("OK") >= 0) {
        DBG("Modem responded at rate", rate);
#if defined(TINY_GSM_PLATFORM_ESP32)
        SerialAT.setTimeout(oldTimeout);
#endif
        return rate;
      }
    }
  }
  TinyGsmSerialSetBaud(SerialAT, minimum);
#if defined(TINY_GSM_PLATFORM_ESP32)
  SerialAT.setTimeout(oldTimeout);
#endif
  return 0;
}

#endif  // SRC_TINYGSMCOMMON_H_
