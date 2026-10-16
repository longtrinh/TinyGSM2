// Host test: the socket logic of ESP_SSLClient's basic-client mode (modelled on
// the sendrec write loop and recvrec read loop of BSSL_SSL_Client.cpp 2.2.2 /
// SSLClient.h 3.1.3) running on the real TinyGSM GsmClient with a fake
// SIM7600-like modem.  ESP_SSLClient writes whole TLS records, acknowledges
// partial writes, and treats a write of 0 bytes with getWriteError() set as a
// write error (otherwise as a closed socket).
#include "fake_modem.h"

#include <algorithm>

enum { SSL_OK, SSL_WRITE_ERROR, SSL_CLOSED, SSL_READ_ERROR, SSL_TIMEOUT };

// Write a whole record, accepting partial writes (br_ssl_engine_sendrec_ack)
static int sslSendRecord(Client& c, const std::string& rec) {
  size_t off = 0;
  while (off < rec.size()) {
    int wlen = static_cast<int>(c.write(
        reinterpret_cast<const uint8_t*>(rec.data()) + off, rec.size() - off));
    c.flush();
    if (wlen <= 0) {
      bool writeError = c.getWriteError() || !c.connected();
      c.stop();
      // otherwise "presumably the socket just closed itself"
      return writeError ? SSL_WRITE_ERROR : SSL_CLOSED;
    }
    off += wlen;
  }
  return SSL_OK;
}

// Read what is available until `want` bytes arrived; a read of <= 0 bytes
// after available() reported data is fatal
static int sslRecvRecord(Client& c, size_t want, std::string& out,
                         unsigned long timeoutMs = 3000) {
  uint8_t       buf[1536];
  unsigned long t0 = millis();
  while (out.size() < want) {
    int avail = c.available();
    if (avail > 0) {
      size_t len  = std::min(sizeof(buf), want - out.size());
      len         = std::min(len, static_cast<size_t>(avail));
      int    rlen = c.read(buf, len);
      if (rlen <= 0) {
        c.stop();
        return SSL_READ_ERROR;
      }
      out.append(reinterpret_cast<const char*>(buf), rlen);
      continue;
    }
    if (millis() - t0 > timeoutMs) return SSL_TIMEOUT;
  }
  return SSL_OK;
}

static std::string bytes(size_t n, char seed) {
  std::string s;
  for (size_t i = 0; i < n; i++) s += static_cast<char>(seed + i % 50);
  return s;
}

int main() {
  printf("ESP_SSLClient socket logic test\n");
  FakeModem             modem;
  FakeModem::FakeClient gsm(modem);
  CHECK(gsm.connect("mqtt.example.com", 8883) == 1, "basic client connect()");

  std::string hello = bytes(300, 'a');
  CHECK(sslSendRecord(gsm, hello) == SSL_OK && modem.wire == hello,
        "send a 300-byte record (ClientHello)");

  // A certificate chain, much larger than the 64-byte RX FIFO
  std::string chain = bytes(5000, 'A'), got;
  modem.deliver(chain);
  CHECK(sslRecvRecord(gsm, chain.size(), got) == SSL_OK && got == chain,
        "receive 5000 bytes of server records via available()/read()");

  // Gateway configuration: setBufferSizes(1536, 1536)
  std::string rec = bytes(1536, 'k');
  modem.wire.clear();
  modem.sends.clear();
  int r = sslSendRecord(gsm, rec);
  printf("    modem send commands: %s\n", modem.sendSizes().c_str());
  CHECK(r == SSL_OK && modem.wire == rec,
        "send a 1536-byte record (> 1500-byte modem limit)");

  // The modem buffer only takes part of each send: write() reports the
  // partial count and the record loop sends the rest
  modem.wire.clear();
  modem.sends.clear();
  modem.acceptMax = 700;
  r               = sslSendRecord(gsm, rec);
  modem.acceptMax = 0;
  printf("    modem send commands: %s\n", modem.sendSizes().c_str());
  CHECK(r == SSL_OK && modem.wire == rec,
        "send a 1536-byte record with partial acknowledgements");

  modem.refuse = true;
  r            = sslSendRecord(gsm, bytes(200, 'z'));
  modem.refuse = false;
  CHECK(r == SSL_WRITE_ERROR && gsm.getWriteError() != 0,
        "modem refuses data -> write error (getWriteError() set)");

  CHECK(gsm.connect("mqtt.example.com", 8883) == 1 && gsm.getWriteError() == 0,
        "reconnect clears the write error");
  modem.wire.clear();
  CHECK(sslSendRecord(gsm, hello) == SSL_OK && modem.wire == hello,
        "send works again after reconnect");

  return testResult();
}
