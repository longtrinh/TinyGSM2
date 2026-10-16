// Minimal Arduino API for the host-side tests: just enough of Print, Stream,
// Client, IPAddress and String for TinyGsmTCP.tpp, PubSubClient and the test
// code to compile and run on a PC.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#if defined(_MSC_VER)
#define __attribute__(x)
#endif

typedef bool    boolean;
typedef uint8_t byte;

inline unsigned long millis() {
  using namespace std::chrono;
  static const steady_clock::time_point t0 = steady_clock::now();
  return static_cast<unsigned long>(
      duration_cast<milliseconds>(steady_clock::now() - t0).count());
}
inline void delay(unsigned long ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline void yield() {}

class __FlashStringHelper;
#define F(x) (reinterpret_cast<const __FlashStringHelper*>(x))
#define PSTR(x) (x)
#define PROGMEM
#define PGM_P const char*
#define strlen_P strlen
#define strnlen_P strnlen
#define memcpy_P memcpy
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#define pgm_read_byte_near(p) pgm_read_byte(p)

class String {
 public:
  String() {}
  String(const char* c) : s(c ? c : "") {}  // NOLINT(runtime/explicit)
  String(int v) : s(std::to_string(v)) {}   // NOLINT(runtime/explicit)
  String& operator+=(const char* c) {
    s += c;
    return *this;
  }
  String& operator+=(int v) {
    s += std::to_string(v);
    return *this;
  }
  int indexOf(const char* c) const {
    size_t i = s.find(c);
    return i == std::string::npos ? -1 : static_cast<int>(i);
  }
  void        reserve(unsigned) {}
  const char* c_str() const {
    return s.c_str();
  }
  unsigned length() const {
    return static_cast<unsigned>(s.size());
  }

 private:
  std::string s;
};

class Print {
 public:
  virtual ~Print() {}
  int getWriteError() {
    return write_error;
  }
  void clearWriteError() {
    setWriteError(0);
  }

  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) {
    size_t n = 0;
    while (n < size && write(buf[n])) { n++; }
    return n;
  }
  size_t write(const char* str) {
    if (str == nullptr) return 0;
    return write(reinterpret_cast<const uint8_t*>(str), strlen(str));
  }
  virtual void flush() {}
  size_t       print(const char* str) {
    return write(str);
  }

 protected:
  void setWriteError(int err = 1) {
    write_error = err;
  }

 private:
  int write_error = 0;
};

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read()      = 0;
  virtual int peek()      = 0;
  void        setTimeout(unsigned long timeout) {
    _timeout = timeout;
  }
  size_t readBytes(uint8_t* buf, size_t size) {
    size_t n = 0;
    while (n < size) {
      int c = read();
      if (c < 0) break;
      buf[n++] = static_cast<uint8_t>(c);
    }
    return n;
  }

 protected:
  unsigned long _timeout = 1000;
};

class IPAddress {
 public:
  IPAddress() {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    o[0] = a;
    o[1] = b;
    o[2] = c;
    o[3] = d;
  }
  uint8_t operator[](int i) const {
    return o[i];
  }
  bool operator==(const IPAddress& x) const {
    return memcmp(o, x.o, 4) == 0;
  }
  bool operator!=(const IPAddress& x) const {
    return !(*this == x);
  }

 private:
  uint8_t o[4] = {0, 0, 0, 0};
};

class Client : public Stream {
 public:
  virtual int     connect(IPAddress ip, uint16_t port)        = 0;
  virtual int     connect(const char* host, uint16_t port)    = 0;
  virtual size_t  write(uint8_t)                              = 0;
  virtual size_t  write(const uint8_t* buf, size_t size)      = 0;
  virtual int     available()                                 = 0;
  virtual int     read()                                      = 0;
  virtual int     read(uint8_t* buf, size_t size)             = 0;
  virtual int     peek()                                      = 0;
  virtual void    flush()                                     = 0;
  virtual void    stop()                                      = 0;
  virtual uint8_t connected()                                 = 0;
  virtual         operator bool()                             = 0;
};
