// A fake SIM7600-like modem for the host-side tests.  It plugs into the real
// TinyGsmTCP template (the same GsmClient read/write code the drivers use) and
// replaces the AT command layer with an in-memory socket:
//  - each send command takes at most `maxSend` bytes (SIM7600 AT+CIPSEND
//    limit: 1500) and every send is recorded in `sends`;
//  - accepted bytes are appended to `wire`;
//  - deliver() queues data from the network and sets got_data, as the driver
//    does on a "+CIPRXGET: 1,<mux>" URC.
#pragma once

#include "Arduino.h"
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
#include "TinyGsmTCP.tpp"

#include <string>
#include <vector>

// The modem's UART: nothing ever arrives on it, the fake answers directly
class FakeStream : public Stream {
 public:
  int available() override {
    return 0;
  }
  int read() override {
    return -1;
  }
  int peek() override {
    return -1;
  }
  size_t write(uint8_t) override {
    return 1;
  }
};

class FakeModem : public TinyGsmTCP<FakeModem, 2> {
  friend class TinyGsmTCP<FakeModem, 2>;

 public:
  class FakeClient : public GsmClient {
    friend class FakeModem;

   public:
    explicit FakeClient(FakeModem& m) {
      this->at     = &m;
      this->mux    = 0;
      m.sockets[0] = this;
    }
    // Starts like every TinyGSM2 driver's connect(): a new connection begins
    // without the previous connection's write error
    int connect(const char*, uint16_t) override {
      clearWriteError();
      sock_connected = true;
      return 1;
    }
    int connect(IPAddress, uint16_t) override {
      return connect("ip", 0);
    }
    void stop() override {
      sock_connected = false;
    }
  };

  FakeStream          stream;
  FakeClient*         sockets[2] = {nullptr, nullptr};
  std::string         inbound;  // data waiting in the modem's socket buffer
  std::string         wire;     // everything the modem sent to the network
  std::vector<size_t> sends;    // size of each send command
  size_t maxSend   = 1500;   // larger sends are answered with ERROR
  size_t acceptMax = 0;      // if set, accept at most this many bytes per send
  bool   refuse    = false;  // answer ERROR to every send

  // New data from the network
  void deliver(const std::string& data) {
    inbound += data;
    if (sockets[0]) sockets[0]->got_data = true;
  }

  std::string sendSizes() const {
    std::string r;
    for (size_t s : sends) r += std::to_string(s) + " ";
    return r;
  }

  // ---- what TinyGsmTCP expects from a driver ----
  int8_t waitResponse(uint32_t, const char* = nullptr, const char* = nullptr) {
    return 0;
  }
  void streamClear() {}
  int16_t modemSend(const void* buf, size_t len, uint8_t) {
    sends.push_back(len);
    if (refuse || len > maxSend) return 0;
    if (acceptMax && len > acceptMax) len = acceptMax;
    wire.append(static_cast<const char*>(buf), len);
    return static_cast<int16_t>(len);
  }
  size_t modemGetAvailable(uint8_t) {
    return inbound.size();
  }
  bool modemGetConnected(uint8_t) {
    return sockets[0] && sockets[0]->sock_connected;
  }
  size_t modemRead(size_t size, uint8_t mux) {
    size_t n = size < inbound.size() ? size : inbound.size();
    sockets[mux]->rx.put(reinterpret_cast<const uint8_t*>(inbound.data()),
                         static_cast<int>(n));
    inbound.erase(0, n);
    sockets[mux]->sock_available = static_cast<uint16_t>(inbound.size());
    return n;
  }
};

// ---- test helpers ----
static int failures = 0;
#define CHECK(cond, what)                                      \
  do {                                                         \
    bool ok_ = (cond);                                         \
    printf("  [%s] %s\n", ok_ ? "PASS" : "FAIL", what);        \
    if (!ok_) failures++;                                      \
  } while (0)

static inline int testResult() {
  printf("  => %s\n", failures ? "FAILED" : "ALL PASSED");
  return failures ? 1 : 0;
}
