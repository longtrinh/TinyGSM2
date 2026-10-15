/**
 * @file       TinyGsmTCP.tpp
 * @author     Volodymyr Shymanskyy
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2016 Volodymyr Shymanskyy
 * @date       Nov 2016
 */

#ifndef SRC_TINYGSMTCP_H_
#define SRC_TINYGSMTCP_H_

#include "TinyGsmCommon.h"

#define TINY_GSM_MODEM_HAS_TCP

#include "TinyGsmFifo.h"

#if !defined(TINY_GSM_RX_BUFFER)
#if defined(TINY_GSM_PLATFORM_ESP32)
// ESP32 has plenty of RAM; a tiny buffer means one AT round-trip per 63 bytes
#define TINY_GSM_RX_BUFFER 1024
#else
#define TINY_GSM_RX_BUFFER 64
#endif
#endif

// Because of the ordering of resolution of overrides in templates, these need
// to be written out every time.  This macro is to shorten that.
#define TINY_GSM_CLIENT_CONNECT_OVERRIDES                             \
  int connect(IPAddress ip, uint16_t port, int timeout_s) {           \
    return connect(TinyGsmStringFromIp(ip).c_str(), port, timeout_s); \
  }                                                                   \
  int connect(const char* host, uint16_t port) override {             \
    return connect(host, port, 75);                                   \
  }                                                                   \
  int connect(IPAddress ip, uint16_t port) override {                 \
    return connect(ip, port, 75);                                     \
  }

// // For modules that do not store incoming data in any sort of buffer
// #define TINY_GSM_NO_MODEM_BUFFER
// // Data is stored in a buffer, but we can only read from the buffer,
// // not check how much data is stored in it
// #define TINY_GSM_BUFFER_READ_NO_CHECK
// // Data is stored in a buffer and we can both read and check the size
// // of the buffer
// #define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE

template <class modemType, uint8_t muxCount>
class TinyGsmTCP {
  /* =========================================== */
  /* =========================================== */
  /*
   * Define the interface
   */
 public:
  /*
   * Basic functions
   */
  void maintain() {
    TINY_GSM_LOCK(thisModem());
    return thisModem().maintainImpl();
  }

  /*
   * CRTP Helper
   */
 protected:
  inline const modemType& thisModem() const {
    return static_cast<const modemType&>(*this);
  }
  inline modemType& thisModem() {
    return static_cast<modemType&>(*this);
  }
  ~TinyGsmTCP() {}

  /*
   * Inner Client
   */
 public:
  class GsmClient : public Client {
    // Make all classes created from the modem template friends
    friend class TinyGsmTCP<modemType, muxCount>;
    typedef TinyGsmFifo<uint8_t, TINY_GSM_RX_BUFFER> RxFifo;

   public:
    // A client that has not been init()'ed yet must still be safe to query
    // (e.g. PubSubClient calls connected() before connecting).
    GsmClient()
        : at(nullptr),
          mux(0),
          sock_available(0),
          prev_check(0),
          sock_connected(false),
          got_data(false) {}

    // Un-register from the modem so URCs never write into a destroyed client
    // (e.g. a TinyGsmClient that was a local variable in a function).
    // (not marked override: Print has no virtual destructor on some cores)
    ~GsmClient() {
      if (at && mux < muxCount && at->sockets[mux] == this) {
        at->sockets[mux] = nullptr;
      }
    }

    // bool init(modemType* modem, uint8_t);
    // int connect(const char* host, uint16_t port, int timeout_s);

    // Connect to a IP address given as an IPAddress object by
    // converting said IP address to text
    // virtual int connect(IPAddress ip,uint16_t port, int timeout_s) {
    //   return connect(TinyGsmStringFromIp(ip).c_str(), port,
    //   timeout_s);
    // }
    // int connect(const char* host, uint16_t port) override {
    //   return connect(host, port, 75);
    // }
    // int connect(IPAddress ip,uint16_t port) override {
    //   return connect(ip, port, 75);
    // }

    static inline String TinyGsmStringFromIp(IPAddress ip) {
#if defined(TINY_GSM_ESP32_CORE3)
      // arduino-esp32 3.x IPAddress is dual-stack; operator[] on an IPv6
      // address would return the first raw bytes, not an IPv4 octet.
      if (ip.type() == IPv6) { return ip.toString(); }
#endif
      String host;
      host.reserve(16);
      host += ip[0];
      host += ".";
      host += ip[1];
      host += ".";
      host += ip[2];
      host += ".";
      host += ip[3];
      return host;
    }

    // void stop(uint32_t maxWaitMs);
    // void stop() override {
    //   stop(15000L);
    // }

    // Writes data out on the client using the modem send functionality
    size_t write(const uint8_t* buf, size_t size) override {
      if (!at || buf == nullptr || size == 0) return 0;
      TINY_GSM_LOCK(*at);
      at->maintain();
      // Drivers return the number of bytes sent (int16_t or size_t); never
      // let an error value turn into a huge size_t
      long sent = static_cast<long>(at->modemSend(buf, size, mux));
      return sent > 0 ? static_cast<size_t>(sent) : 0;
    }

    size_t write(uint8_t c) override {
      return write(&c, 1);
    }

    size_t write(const char* str) {
      if (str == nullptr) return 0;
      return write((const uint8_t*)str, strlen(str));
    }

    int available() override {
      if (!at) return 0;
      TINY_GSM_LOCK(*at);
      int result = 0;
#if defined TINY_GSM_NO_MODEM_BUFFER
      // Returns the number of characters available in the TinyGSM fifo
      if (!rx.size() && sock_connected) { at->maintain(); }
      result = rx.size();

#elif defined TINY_GSM_BUFFER_READ_NO_CHECK
      // Returns the combined number of characters available in the TinyGSM
      // fifo and the modem chips internal fifo.
      if (!rx.size()) { at->maintain(); }
      result = static_cast<uint16_t>(rx.size()) + sock_available;

#elif defined TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
      // Returns the combined number of characters available in the TinyGSM
      // fifo and the modem chips internal fifo, doing an extra check-in
      // with the modem to see if anything has arrived without a UURC.
      if (!rx.size()) {
        if (millis() - prev_check > 500) {
          // setting got_data to true will tell maintain to run
          // modemGetAvailable(mux)
          got_data   = true;
          prev_check = millis();
        }
        at->maintain();
      }
      result = static_cast<uint16_t>(rx.size()) + sock_available;

#else
#error Modem client has been incorrectly created
#endif
      // Only yield when there is nothing to hand out, so polling loops like
      // `while (!client.available())` don't starve other tasks.
      if (!result) { TINY_GSM_YIELD(); }
      return result;
    }

    int read(uint8_t* buf, size_t size) override {
      if (!at || buf == nullptr) return 0;
      TINY_GSM_LOCK(*at);
      size_t cnt = 0;

#if defined TINY_GSM_NO_MODEM_BUFFER
      // Reads characters out of the TinyGSM fifo, waiting for any URC's
      // from the modem for new data if there's nothing in the fifo.
      // Returns as soon as some data was read and the fifo is drained.
      uint32_t _startMillis = millis();
      while (cnt < size && millis() - _startMillis < _timeout) {
        size_t chunk = TinyGsmMin(size - cnt, rx.size());
        if (chunk > 0) {
          rx.get(buf, chunk);
          buf += chunk;
          cnt += chunk;
          continue;
        } /* TODO: Read directly into user buffer? */
        if (cnt > 0 || !sock_connected) { break; }
        at->maintain();
      }
      return cnt;

#elif defined TINY_GSM_BUFFER_READ_NO_CHECK
      // Reads characters out of the TinyGSM fifo, and from the modem chip's
      // internal fifo if avaiable.
      at->maintain();
      while (cnt < size) {
        size_t chunk = TinyGsmMin(size - cnt, rx.size());
        if (chunk > 0) {
          rx.get(buf, chunk);
          buf += chunk;
          cnt += chunk;
          continue;
        } /* TODO: Read directly into user buffer? */
        at->maintain();
        if (sock_available > 0) {
          int n = at->modemRead(TinyGsmMin((uint16_t)rx.free(), sock_available),
                                mux);
          if (n <= 0) break;
        } else {
          break;
        }
      }
      return cnt;

#elif defined TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
      // Reads characters out of the TinyGSM fifo, and from the modem chips
      // internal fifo if avaiable, also double checking with the modem if
      // data has arrived without issuing a UURC.
      at->maintain();
      while (cnt < size) {
        size_t chunk = TinyGsmMin(size - cnt, rx.size());
        if (chunk > 0) {
          rx.get(buf, chunk);
          buf += chunk;
          cnt += chunk;
          continue;
        }
        // Workaround: Some modules "forget" to notify about data arrival
        if (millis() - prev_check > 500) {
          // setting got_data to true will tell maintain to run
          // modemGetAvailable()
          got_data   = true;
          prev_check = millis();
        }
        // TODO(vshymanskyy): Read directly into user buffer?
        at->maintain();
        if (sock_available > 0) {
          int n = at->modemRead(TinyGsmMin((uint16_t)rx.free(), sock_available),
                                mux);
          if (n <= 0) break;
        } else {
          break;
        }
      }
      return cnt;

#else
#error Modem client has been incorrectly created
#endif
    }

    int read() override {
      uint8_t c;
      if (read(&c, 1) == 1) { return c; }
      return -1;
    }

    // Returns the next byte without removing it, or -1 if there is none
    int peek() override {
      if (!at) return -1;
      TINY_GSM_LOCK(*at);
#if defined TINY_GSM_BUFFER_READ_AND_CHECK_SIZE || \
    defined TINY_GSM_BUFFER_READ_NO_CHECK
      // Pull data waiting in the modem's buffer into the fifo so peek() agrees
      // with available()
      if (!rx.size() && sock_available > 0) {
        at->modemRead(TinyGsmMin((uint16_t)rx.free(), sock_available), mux);
      }
#endif
      return rx.peek();
    }

    void flush() override {
      if (!at) return;
      at->stream.flush();
    }

    uint8_t connected() override {
      if (!at) return false;
      TINY_GSM_LOCK(*at);
      if (available()) { return true; }
#if defined TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
      // If the modem is one where we can read and check the size of the buffer,
      // then the 'available()' function will call a check of the current size
      // of the buffer and state of the connection. [available calls maintain,
      // maintain calls modemGetAvailable, modemGetAvailable calls
      // modemGetConnected]  This cascade means that the sock_connected value
      // should be correct and all we need
      return sock_connected;
#elif defined TINY_GSM_NO_MODEM_BUFFER || defined TINY_GSM_BUFFER_READ_NO_CHECK
      // If the modem doesn't have an internal buffer, or if we can't check how
      // many characters are in the buffer then the cascade won't happen.
      // We need to call modemGetConnected to check the sock state.
      return at->modemGetConnected(mux);
#else
#error Modem client has been incorrectly created
#endif
    }
    operator bool() override {
      return connected();
    }

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;

   protected:
    // Read and dump anything remaining in the modem's internal buffer.
    // Using this in the client stop() function.
    // The socket will appear open in response to connected() even after it
    // closes until all data is read from the buffer.
    // Doing it this way allows the external mcu to find and get all of the
    // data that it wants from the socket even if it was closed externally.
    inline void dumpModemBuffer(uint32_t maxWaitMs) {
#if defined TINY_GSM_BUFFER_READ_AND_CHECK_SIZE || \
    defined TINY_GSM_BUFFER_READ_NO_CHECK
      TINY_GSM_YIELD();
      uint32_t startMillis = millis();
      while (sock_available > 0 && (millis() - startMillis < maxWaitMs)) {
        rx.clear();
        if (at->modemRead(TinyGsmMin((uint16_t)rx.free(), sock_available),
                          mux) <= 0) {
          break;
        }
      }
      rx.clear();
      at->streamClear();

#elif defined TINY_GSM_NO_MODEM_BUFFER
      rx.clear();
      at->streamClear();

#else
#error Modem client has been incorrectly created
#endif
    }

    modemType* at;
    uint8_t    mux;
    uint16_t   sock_available;
    uint32_t   prev_check;
    bool       sock_connected;
    bool       got_data;
    RxFifo     rx;
  };

  /* =========================================== */
  /* =========================================== */
  /*
   * Define the default function implementations
   */

  /*
   * Basic functions
   */
 protected:
  void maintainImpl() {
#if defined TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
    // Keep listening for modem URC's and proactively iterate through
    // sockets asking if any data is avaiable
    for (int mux = 0; mux < muxCount; mux++) {
      GsmClient* sock = thisModem().sockets[mux];
      if (sock && sock->got_data) {
        sock->got_data       = false;
        sock->sock_available = thisModem().modemGetAvailable(mux);
      }
    }
    while (thisModem().stream.available()) {
      thisModem().waitResponse(15, nullptr, nullptr);
    }

#elif defined TINY_GSM_NO_MODEM_BUFFER || defined TINY_GSM_BUFFER_READ_NO_CHECK
    // Just listen for any URC's
    thisModem().waitResponse(100, nullptr, nullptr);

#else
#error Modem client has been incorrectly created
#endif
  }

  // Waits up to the socket time-out for the next character from the modem.
  // Returns the character, or -1 on time-out (never stores the -1).
  inline int streamReadSocketChar(uint8_t mux) {
    uint32_t timeout = thisModem().sockets[mux]
        ? thisModem().sockets[mux]->_timeout
        : 1000L;
    uint32_t startMillis = millis();
    while (!thisModem().stream.available() &&
           (millis() - startMillis < timeout)) {
      TINY_GSM_YIELD();
    }
    return thisModem().stream.read();
  }

  // Moves one character from the stream into the mux FIFO.  If the socket no
  // longer exists the character is consumed and dropped so the stream stays in
  // sync with the modem's framing.
  // Returns false if no character arrived within the socket time-out.
  inline bool moveCharFromStreamToFifo(uint8_t mux) {
    int c = streamReadSocketChar(mux);
    if (c < 0) {
      DBG("### Timed out reading socket data on mux", mux);
      return false;
    }
    if (thisModem().sockets[mux]) {
      thisModem().sockets[mux]->rx.put(static_cast<uint8_t>(c));
    }
    return true;
  }

  // Moves `len` characters of socket payload from the stream into the mux
  // FIFO, reading in chunks instead of byte by byte.  Stops early if no data
  // arrives within the socket time-out.  Returns the number of characters
  // consumed from the stream.
  inline size_t moveCharsFromStreamToFifo(uint8_t mux, size_t len) {
    GsmClient* sock    = thisModem().sockets[mux];
    uint32_t   timeout = sock ? sock->_timeout : 1000L;
    uint8_t    buf[64];
    size_t     moved       = 0;
    uint32_t   startMillis = millis();
    while (moved < len) {
      int avail = thisModem().stream.available();
      if (avail <= 0) {
        if (millis() - startMillis >= timeout) {
          DBG("### Timed out reading socket data:", moved, "of", len);
          break;
        }
        TINY_GSM_YIELD();
        continue;
      }
      size_t chunk = TinyGsmMin(len - moved, static_cast<size_t>(avail));
      chunk        = TinyGsmMin(chunk, sizeof(buf));
      size_t got   = thisModem().stream.readBytes(buf, chunk);
      if (got == 0) { continue; }
      if (sock) {
        size_t stored = sock->rx.put(buf, got);
        if (stored < got) {
          DBG("### Buffer overflow on mux", mux, ", dropped", got - stored);
        }
      }
      moved += got;
      startMillis = millis();
    }
    return moved;
  }
};

#endif  // SRC_TINYGSMTCP_H_
