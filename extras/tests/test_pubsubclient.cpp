// Host test: the real PubSubClient (knolleary 2.8 or hmueller01's
// PubSubClient3) on top of the real TinyGSM GsmClient, with a fake SIM7600-like
// modem underneath.  PubSubClient writes a whole MQTT packet in one write()
// and treats anything short of the full length as a failure, so packets larger
// than the modem's 1500-byte send limit must be split by GsmClient::write().
#include "fake_modem.h"

#include "PubSubClient.h"

#ifndef PSC_LABEL
#define PSC_LABEL "PubSubClient"
#endif

static std::string encodeLen(size_t len) {
  std::string r;
  do {
    uint8_t d = len % 128;
    len /= 128;
    if (len) d |= 0x80;
    r += static_cast<char>(d);
  } while (len);
  return r;
}

// A QoS 0 PUBLISH packet, byte for byte
static std::string publishPacket(const std::string& topic,
                                 const std::string& payload) {
  std::string vh;
  vh += static_cast<char>(topic.size() >> 8);
  vh += static_cast<char>(topic.size() & 0xFF);
  vh += topic;
  return std::string(1, '\x30') + encodeLen(vh.size() + payload.size()) + vh +
      payload;
}

static std::string makePayload(size_t n) {
  std::string p;
  for (size_t i = 0; i < n; i++) p += static_cast<char>('A' + i % 26);
  return p;
}

static std::string lastTopic, lastPayload;
static void onMessage(char* topic, uint8_t* payload, unsigned int len) {
  lastTopic = topic;
  lastPayload.assign(reinterpret_cast<const char*>(payload), len);
}

int main() {
  printf("PubSubClient test (%s)\n", PSC_LABEL);
  FakeModem             modem;
  FakeModem::FakeClient gsm(modem);
  PubSubClient          mqtt(gsm);
  mqtt.setServer("broker.example.com", 1883);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(4096);

  // 1. CONNECT / CONNACK
  modem.inbound = std::string("\x20\x02\x00\x00", 4);
  CHECK(mqtt.connect("tinygsm2-test"),
        "connect() succeeds (CONNACK read through GsmClient)");

  // 2. Small publish
  modem.wire.clear();
  modem.sends.clear();
  CHECK(mqtt.publish("t/small", "hello"), "publish() of a 5-byte message");
  CHECK(modem.wire == publishPacket("t/small", "hello"),
        "small PUBLISH packet is byte-exact on the wire");

  // 3. Large publish (bigger than the modem's 1500-byte send limit)
  std::string big = makePayload(1800);
  modem.wire.clear();
  modem.sends.clear();
  bool ok = mqtt.publish("t/large", reinterpret_cast<const uint8_t*>(big.data()),
                         big.size());
  printf("    modem send commands: %s\n", modem.sendSizes().c_str());
  CHECK(ok, "publish() of an 1800-byte message (> 1500-byte modem limit)");
  CHECK(modem.wire == publishPacket("t/large", big),
        "large PUBLISH packet is byte-exact on the wire");

  // 4. Streamed publish: beginPublish / write / endPublish with 3000 bytes
  std::string big2 = makePayload(3000);
  modem.wire.clear();
  modem.sends.clear();
  ok       = mqtt.beginPublish("t/stream", big2.size(), false);
  size_t w = mqtt.write(reinterpret_cast<const uint8_t*>(big2.data()),
                        big2.size());
  ok       = mqtt.endPublish() && ok && w == big2.size();
  printf("    modem send commands: %s\n", modem.sendSizes().c_str());
  CHECK(ok, "beginPublish/write/endPublish of 3000 bytes");
  CHECK(modem.wire == publishPacket("t/stream", big2),
        "streamed PUBLISH packet is byte-exact on the wire");

  // 5. Receive a 1500-byte message (much more than the 64-byte RX FIFO)
  std::string in = makePayload(1500);
  modem.deliver(publishPacket("t/in", in));
  lastTopic.clear();
  lastPayload.clear();
  for (int i = 0; i < 5 && lastPayload.empty(); i++) mqtt.loop();
  CHECK(lastTopic == "t/in" && lastPayload == in,
        "receives a 1500-byte message intact via loop()");

  CHECK(mqtt.connected(), "still connected at the end");
  return testResult();
}
