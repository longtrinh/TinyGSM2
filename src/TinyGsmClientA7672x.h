/**
 * @file       TinyGsmClientA7672x.h
 * @author     Giovanni de Rosso Unruh
 * @license    LGPL-3.0
 * @copyright  Copyright (c) 2022 Giovanni de Rosso Unruh
 * @date       Oct 2022
 */

#ifndef SRC_TINYGSMCLIENTA7672X_H_
#define SRC_TINYGSMCLIENTA7672X_H_

// #define TINY_GSM_DEBUG Serial

// Optional settings (define before including TinyGsmClient.h):
//  - TINY_GSM_A7672X_DNS "\"8.8.8.8\",\"8.8.4.4\""
//      Override the carrier DNS servers (AT+CDNSCFG) in gprsConnect().
//  - TINY_GSM_A7672X_SSL_VERSION 3
//      Force an SSL/TLS version (AT+CSSLCFG="sslversion"). The meaning of the
//      value depends on the firmware (see the AT manual); by default the
//      modem's own default ("all versions") is kept.

#define TINY_GSM_MUX_COUNT 10
#define TINY_GSM_BUFFER_READ_AND_CHECK_SIZE
#ifdef AT_NL
#undef AT_NL
#endif
#define AT_NL "\r\n"

#ifdef MODEM_MANUFACTURER
#undef MODEM_MANUFACTURER
#endif
#define MODEM_MANUFACTURER "SIMCom"

#ifdef MODEM_MODEL
#undef MODEM_MODEL
#endif
#define MODEM_MODEL "A7672x"

#include "TinyGsmModem.tpp"
#include "TinyGsmTCP.tpp"
#include "TinyGsmSSL.tpp"
#include "TinyGsmGPRS.tpp"
#include "TinyGsmCalling.tpp"
#include "TinyGsmSMS.tpp"
#include "TinyGsmGSMLocation.tpp"
#include "TinyGsmTime.tpp"
#include "TinyGsmNTP.tpp"
#include "TinyGsmBattery.tpp"
#include "TinyGsmTemperature.tpp"

enum A7672xRegStatus {
  REG_NO_RESULT    = -1,
  REG_UNREGISTERED = 0,
  REG_SEARCHING    = 2,
  REG_DENIED       = 3,
  REG_OK_HOME      = 1,
  REG_OK_ROAMING   = 5,
  REG_UNKNOWN      = 4,
};
class TinyGsmA7672X : public TinyGsmModem<TinyGsmA7672X>,
                      public TinyGsmGPRS<TinyGsmA7672X>,
                      public TinyGsmTCP<TinyGsmA7672X, TINY_GSM_MUX_COUNT>,
                      public TinyGsmSSL<TinyGsmA7672X, TINY_GSM_MUX_COUNT>,
                      public TinyGsmCalling<TinyGsmA7672X>,
                      public TinyGsmSMS<TinyGsmA7672X>,
                      public TinyGsmGSMLocation<TinyGsmA7672X>,
                      public TinyGsmTime<TinyGsmA7672X>,
                      public TinyGsmNTP<TinyGsmA7672X>,
                      public TinyGsmBattery<TinyGsmA7672X>,
                      public TinyGsmTemperature<TinyGsmA7672X> {
  friend class TinyGsmModem<TinyGsmA7672X>;
  friend class TinyGsmGPRS<TinyGsmA7672X>;
  friend class TinyGsmTCP<TinyGsmA7672X, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmSSL<TinyGsmA7672X, TINY_GSM_MUX_COUNT>;
  friend class TinyGsmCalling<TinyGsmA7672X>;
  friend class TinyGsmSMS<TinyGsmA7672X>;
  friend class TinyGsmGSMLocation<TinyGsmA7672X>;
  friend class TinyGsmTime<TinyGsmA7672X>;
  friend class TinyGsmNTP<TinyGsmA7672X>;
  friend class TinyGsmBattery<TinyGsmA7672X>;
  friend class TinyGsmTemperature<TinyGsmA7672X>;

  /*
   * Inner Client
   */
 public:
  class GsmClientA7672X : public GsmClient {
    friend class TinyGsmA7672X;

   public:
    GsmClientA7672X() {}

    explicit GsmClientA7672X(TinyGsmA7672X& modem, uint8_t mux = 0) {
      init(&modem, mux);
    }

    bool init(TinyGsmA7672X* modem, uint8_t mux = 0) {
      this->at       = modem;
      sock_available = 0;
      prev_check     = 0;
      sock_connected = false;
      got_data       = false;

      if (mux < TINY_GSM_MUX_COUNT) {
        this->mux = mux;
      } else {
        this->mux = (mux % TINY_GSM_MUX_COUNT);
      }
      at->sockets[this->mux] = this;

      return true;
    }

   public:
    virtual int connect(const char* host, uint16_t port, int timeout_s) {
      if (!at) return 0;
      TINY_GSM_LOCK(*at);
      clearWriteError();  // a new connection starts without a write error
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, false, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES

    // Closes the TCP link or SSL session, whichever this mux is using
    void stop(uint32_t maxWaitMs) {
      if (!at) return;
      TINY_GSM_LOCK(*at);
      dumpModemBuffer(maxWaitMs);
      at->modemDisconnect(mux);
      sock_connected = false;
    }
    void stop() override {
      stop(15000L);
    }

    /*
     * Extended API
     */

    String remoteIP() TINY_GSM_ATTR_NOT_IMPLEMENTED;
  };

  /*
   * Inner Secure Client
   */
 public:
  class GsmClientSecureA7672X : public GsmClientA7672X {
   public:
    GsmClientSecureA7672X() {}

    explicit GsmClientSecureA7672X(TinyGsmA7672X& modem, uint8_t mux = 0)
        : GsmClientA7672X(modem, mux) {}

   public:
    bool addCertificate(const char* certificateName, const char* cert,
                        const uint16_t len) {
      return at->addCertificate(certificateName, cert, len);
    }

    bool setCertificate(const char* certificateName) {
      return at->setCertificate(certificateName, mux);
    }

    bool deleteCertificate(const char* certificateName) {
      return at->deleteCertificate(certificateName);
    }

    // NOTE: the module supports fewer SSL sessions than TCP links (typically
    // 2), so use mux 0 or 1 for secure clients.
    int connect(const char* host, uint16_t port, int timeout_s) override {
      if (!at) return 0;
      TINY_GSM_LOCK(*at);
      clearWriteError();  // a new connection starts without a write error
      stop();
      TINY_GSM_YIELD();
      rx.clear();
      sock_connected = at->modemConnect(host, port, mux, true, timeout_s);
      return sock_connected;
    }
    TINY_GSM_CLIENT_CONNECT_OVERRIDES
  };

  /*
   * Constructor
   */
 public:
  explicit TinyGsmA7672X(Stream& stream) : stream(stream) {
    memset(sockets, 0, sizeof(sockets));
    memset(sockSSL, 0, sizeof(sockSSL));
  }

  /*
   * Basic functions
   */
 protected:
  bool initImpl(const char* pin = nullptr) {
    DBG(GF("### TinyGSM Version:"), TINYGSM_VERSION);
    DBG(GF("### TinyGSM Compiled Module:  A7672X"));

    // Make sure result codes are verbose ("OK"/"ERROR"); a modem left in
    // numeric mode (ATV0) would never answer "OK" to the commands below.
    sendAT(GF("V1"));
    waitResponse();

    if (!testAT(2000)) { return false; }

    // sendAT(GF("&FZ"));  // Factory + Reset
    // waitResponse();

    sendAT(GF("E0"));  // Echo Off
    if (waitResponse() != 1) { return false; }

#ifdef TINY_GSM_DEBUG
    sendAT(GF("+CMEE=2"));  // turn on verbose error codes
#else
    sendAT(GF("+CMEE=0"));  // turn off error codes
#endif
    waitResponse();

    DBG(GF("### Modem:"), getModemName());

    SimStatus ret = getSimStatus();
    // if the sim isn't ready and a pin has been provided, try to unlock the sim
    if (ret != SIM_READY && pin != nullptr && strlen(pin) > 0) {
      simUnlock(pin);
      return (getSimStatus() == SIM_READY);
    } else {
      // if the sim is ready, or it's locked but no pin has been provided,
      // return true
      return (ret == SIM_READY || ret == SIM_LOCKED);
    }
  }

  bool factoryDefaultImpl() {
    sendAT(GF("&F"));  // Factory + Reset
    waitResponse();
    sendAT(GF("+IFC=0,0"));  // No Flow Control
    waitResponse();
    sendAT(GF("+ICF=2,2"));  // 8 data 0 parity 1 stop
    waitResponse();
    sendAT(GF("+CSCLK=0"));  // Control UART Sleep always work
    waitResponse();
    sendAT(GF("&W"));  // Write configuration
    return waitResponse() == 1;
  }

  /*
   * Power functions
   */
 protected:
  bool restartImpl(const char* pin = nullptr) {
    if (!testAT()) { return false; }
    sendAT(GF("+CRESET"));
    if (waitResponse(10000L) != 1) { return false; }
    // The module reboots now; all sockets and services are gone
    markSocketsClosed(true, true);
    netOpened  = false;
    sslStarted = false;
    delay(5000L);  // Don't talk to the module while it is booting
    if (!testAT(30000L)) { return false; }
    return init(pin);
  }

  bool powerOffImpl() {
    sendAT(GF("+CPOF"));
    return waitResponse(10000L) == 1;
  }

  //  This command is used to enable UART Sleep or always work. If set to 0,
  //  UART always work. If set to 1, ensure that DTR is pulled high and the
  //  module can go to DTR sleep. If set to 2, the module will enter RXsleep. RX
  //  wakeup directly sends data through the serial port (for example: AT) to
  //  wake up
  bool sleepEnableImpl(bool enable = true) {
    sendAT(GF("+CSCLK="),
           enable ? "2" : "1");  // 2: RXsleep (at wakeup) 1: DTR sleep
    return waitResponse() == 1;
  }

  // <fun> 0 minimum functionality
  // <fun> 1 full functionality, online mode
  // <fun> 4 disable phone both transmit and receive RF circuits
  // <fun> 5 Factory Test Mode (The A7600's 5 and 1 have the same function)
  // <fun> 6 Reset
  // <fun> 7 Offline Mode
  // <rst> 0 do not reset the ME before setting it to <fun> power level
  // <rst> 1 reset the ME before setting it to <fun> power level. This
  // valueonlytakes effect when <fun> equals 1
  bool setPhoneFunctionalityImpl(uint8_t fun, bool reset = false) {
    sendAT(GF("+CFUN="), fun, reset ? ",1" : ",0");
    return waitResponse(10000L) == 1;
  }

  /*
   * Generic network functions
   */
 public:
  // The A7672x is an LTE Cat-1 module: it registers on the EPS network
  // (CEREG).  CGREG/CREG are checked too for variants with 2G fallback.
  A7672xRegStatus getRegistrationStatus() {
    TINY_GSM_LOCK(*this);
    A7672xRegStatus epsStatus =
        (A7672xRegStatus)getRegistrationStatusXREG("CEREG");
    if (epsStatus == REG_OK_HOME || epsStatus == REG_OK_ROAMING) {
      return epsStatus;
    }
    A7672xRegStatus gprsStatus =
        (A7672xRegStatus)getRegistrationStatusXREG("CGREG");
    if (gprsStatus == REG_OK_HOME || gprsStatus == REG_OK_ROAMING) {
      return gprsStatus;
    }
    A7672xRegStatus gsmStatus =
        (A7672xRegStatus)getRegistrationStatusXREG("CREG");
    if (gsmStatus == REG_OK_HOME || gsmStatus == REG_OK_ROAMING) {
      return gsmStatus;
    }
    return epsStatus != REG_NO_RESULT ? epsStatus : gsmStatus;
  }

 protected:
  bool isNetworkConnectedImpl() {
    A7672xRegStatus s = getRegistrationStatus();
    return (s == REG_OK_HOME || s == REG_OK_ROAMING);
  }

  /*
   * Secure socket layer (SSL) functions
   */
 public:
  // The name of the certificate/key/password file. The file name must
  // havetype like ".pem" or ".der".
  // The certificate like - const char ca_cert[] PROGMEM =  R"EOF(-----BEGIN...
  // len of certificate like - sizeof(ca_cert)
  bool addCertificate(const char* certificateName, const char* cert,
                      const uint16_t len) {
    TINY_GSM_LOCK(*this);
    sendAT(GF("+CCERTDOWN="), certificateName, GF(","), len);
    if (waitResponse(GF(">")) != 1) { return false; }
    stream.write(cert, len);
    stream.flush();
    return waitResponse() == 1;
  }

  bool deleteCertificate(const char* certificateName) {  // todo test
    TINY_GSM_LOCK(*this);
    sendAT(GF("+CCERTDELE="), certificateName);
    return waitResponse() == 1;
  }

  /*
   * WiFi functions
   */
  // No functions of this type supported

  /*
   * GPRS functions
   */
 protected:
  bool gprsConnectImpl(const char* apn, const char* user = nullptr,
                       const char* pwd = nullptr) {
    gprsDisconnect();

    // Define the PDP context
    sendAT(GF("+CGDCONT=1,\"IP\",\""), apn, '"');
    waitResponse();

    // Set the authentication; NOTE: the password comes before the user name
    if (user && strlen(user) > 0) {
      sendAT(GF("+CGAUTH=1,3,\""), pwd ? pwd : "", GF("\",\""), user, '"');
      waitResponse();
    }

    // Activate the PDP context
    sendAT(GF("+CGACT=1,1"));
    waitResponse(60000L);

    // Attach to GPRS
    sendAT(GF("+CGATT=1"));
    if (waitResponse(60000L) != 1) { return false; }

    // Set to get data manually
    sendAT(GF("+CIPRXGET=1"));
    if (waitResponse() != 1) { return false; }

    // Get Local IP Address, only assigned after connection
    sendAT(GF("+CGPADDR=1"));
    if (waitResponse(10000L) != 1) { return false; }

#ifdef TINY_GSM_A7672X_DNS
    // Configure Domain Name Server (DNS)
    sendAT(GF("+CDNSCFG="), TINY_GSM_A7672X_DNS);
    if (waitResponse() != 1) { return false; }
#endif

    return true;
  }

  bool gprsDisconnectImpl() {
    // Stop the SSL and TCP/IP services; an ERROR here only means the service
    // was not running.
    sendAT(GF("+CCHSTOP"));
    waitResponse(10000L);
    sslStarted = false;

    sendAT(GF("+NETCLOSE"));
    waitResponse(60000L);
    netOpened = false;
    markSocketsClosed(true, true);

    sendAT(GF("+CGATT=0"));  // Detach from GPRS
    if (waitResponse(60000L) != 1) { return false; }

    return true;
  }

  String getProviderImpl() {
    sendAT(GF("+CSPN?"));
    if (waitResponse(GF("+CSPN:")) != 1) { return ""; }
    streamSkipUntil('"'); /* Skip mode and format */
    String res = stream.readStringUntil('"');
    waitResponse();
    return res;
  }

  /*
   * SIM card functions
   */
 protected:
  SimStatus getSimStatusImpl(uint32_t timeout_ms = 10000L) {
    for (uint32_t start = millis(); millis() - start < timeout_ms;) {
      sendAT(GF("+CPIN?"));
      if (waitResponse(GF("+CPIN:")) != 1) {
        delay(1000);
        continue;
      }
      int8_t status = waitResponse(GF("READY"), GF("SIM PIN"), GF("SIM PUK"),
                                   GF("SIM not inserted"), GF("SIM REMOVED"));
      waitResponse();
      switch (status) {
        case 2:
        case 3: return SIM_LOCKED;
        case 1: return SIM_READY;
        default: return SIM_ERROR;
      }
    }
    return SIM_ERROR;
  }

  String getSimCCIDImpl() {
    sendAT(GF("+CICCID"));
    if (waitResponse(GF(AT_NL "+ICCID:")) != 1) { return ""; }
    String res = stream.readStringUntil('\n');
    waitResponse();
    res.trim();
    return res;
  }

  /*
   * Phone Call functions
   */
 public:
  bool setGsmBusy(bool busy = true) {
    TINY_GSM_LOCK(*this);
    sendAT(GF("+CCFC=1,"), busy ? 1 : 0);
    return waitResponse() == 1;
  }

  /*
   * Audio functions
   */
  // No functions of this type supported

  /*
   * Text messaging (SMS) functions
   */
  // Follows all text messaging (SMS) functions as inherited from TinyGsmSMS.tpp

  /*
   * GSM Location functions
   */
  // No functions of this type supported
  /*
   * GPS/GNSS/GLONASS location functions
   */
  // No functions of this type supported

  /*
   * Time functions
   */
  // No functions of this type supported
  /*
   * NTP server functions
   */
  // No functions of this type supported

  /*
   * BLE functions
   */
  // No functions of this type supported

  /*
   * Battery functions
   */
  // No functions of this type supported

  /*
   * Temperature functions
   */
 protected:
  // get temperature in degree celsius
  float getTemperatureImpl() {
    sendAT(GF("+CPMUTEMP"));
    // +CPMUTEMP: <temp>
    if (waitResponse(GF(AT_NL "+CPMUTEMP:")) != 1) { return 0; }
    float temp = streamGetFloatBefore('\n');
    waitResponse();
    return temp;
  }

  /*
   * Client related functions
   */
 protected:
  // Starts the TCP/IP service (AT+NETOPEN) unless it is already running.
  // AT+NETOPEN answers ERROR if the network is already open, so it must not
  // be sent blindly before every connection.
  bool openNetwork() {
    if (netOpened) { return true; }
    sendAT(GF("+NETOPEN?"));
    if (waitResponse(GF("+NETOPEN:")) == 1) {
      int16_t state = streamGetIntBefore('\n');
      waitResponse();
      if (state == 1) {
        netOpened = true;
        return true;
      }
    }
    sendAT(GF("+NETOPEN"));
    // OK, then +NETOPEN: <err>
    int8_t rsp = waitResponse(30000L, GF(AT_NL "+NETOPEN:"),
                              GF("Network is already opened"), GFP(GSM_ERROR));
    if (rsp == 1) {
      netOpened = (streamGetIntBefore('\n') == 0);
    } else if (rsp == 2) {
      waitResponse();  // the trailing ERROR
      netOpened = true;
    }
    return netOpened;
  }

  // Starts the SSL service (AT+CCHSTART) unless it is already running.
  bool startSSLService() {
    if (sslStarted) { return true; }
    // Report send results and cache received data until it is read with
    // AT+CCHRECV.  NOTE: must be configured before AT+CCHSTART
    sendAT(GF("+CCHSET=1,1"));
    waitResponse(2000L);

    sendAT(GF("+CCHSTART"));
    // OK, then +CCHSTART: <err>
    int8_t rsp = waitResponse(30000L, GF(AT_NL "+CCHSTART:"), GFP(GSM_ERROR));
    if (rsp == 1) {
      sslStarted = (streamGetIntBefore('\n') == 0);
    } else if (rsp == 2) {
      // Most likely the service is already running (e.g. the MCU restarted
      // but the module did not).  If not, AT+CCHOPEN will fail.
      sslStarted = true;
    }
    return sslStarted;
  }

  bool modemConnect(const char* host, uint16_t port, uint8_t mux,
                    bool ssl = false, int timeout_s = 75) {
    uint32_t timeout_ms = ((uint32_t)timeout_s) * 1000;
    sockSSL[mux]        = ssl;

    // +CTCPKA:<keepalive>,<keepidle>,<keepcount>,<keepinterval>
    sendAT(GF("+CTCPKA=1,2,5,1"));
    waitResponse(2000L);

    if (ssl) {
      if (!startSSLService()) { return false; }

      // Each session uses the SSL context with the same index (0-9)
#ifdef TINY_GSM_A7672X_SSL_VERSION
      // AT+CSSLCFG="sslversion",<ssl_ctx_index>,<sslversion>
      sendAT(GF("+CSSLCFG=\"sslversion\","), mux, ',',
             TINY_GSM_A7672X_SSL_VERSION);
      if (waitResponse(5000L) != 1) { return false; }
#endif

      if (certificates[mux] != "") {
        // AT+CSSLCFG="cacert",<ssl_ctx_index>,<ca_file>
        sendAT(GF("+CSSLCFG=\"cacert\","), mux, GF(",\""),
               certificates[mux].c_str(), '"');
        if (waitResponse(5000L) != 1) { return false; }
        // Verify the server against the CA
        sendAT(GF("+CSSLCFG=\"authmode\","), mux, GF(",1"));
      } else {
        // No CA configured: no authentication
        sendAT(GF("+CSSLCFG=\"authmode\","), mux, GF(",0"));
      }
      if (waitResponse(5000L) != 1) { return false; }

      // set the SSL SNI (server name indication)
      // NOTE:  despite docs using caps, "enableSNI" must be in this case
      sendAT(GF("+CSSLCFG=\"enableSNI\","), mux, GF(",1"));
      if (waitResponse(2000L) != 1) { return false; }

      // AT+CCHSSLCFG=<session_id>,<ssl_ctx_index>
      sendAT(GF("+CCHSSLCFG="), mux, ',', mux);
      if (waitResponse(2000L) != 1) { return false; }

      // AT+CCHOPEN=<session_id>,<host>,<port>,<client_type: 2 = SSL/TLS>
      sendAT(GF("+CCHOPEN="), mux, GF(",\""), host, GF("\","), port,
             GF(",2"));
      // OK, then +CCHOPEN: <session_id>,<err>
      if (waitResponse(timeout_ms, GF(AT_NL "+CCHOPEN:")) != 1) {
        return false;
      }
    } else {
      if (!openNetwork()) { return false; }

      // Make sure we'll be getting data manually on this connection
      sendAT(GF("+CIPRXGET=1"));
      waitResponse();

      sendAT(GF("+CIPOPEN="), mux, GF(",\"TCP\",\""), host, GF("\","), port);
      // OK, then +CIPOPEN: <link_num>,<err>
      if (waitResponse(timeout_ms, GF(AT_NL "+CIPOPEN:")) != 1) {
        return false;
      }
    }
    int16_t opened_mux = streamGetIntBefore(',');
    int16_t opened_err = streamGetIntBefore('\n');
    if (opened_err != 0) {
      DBG("### Connection failed on mux", opened_mux, "error", opened_err);
      waitResponse(100L);  // may be followed by ERROR
      return false;
    }
    return opened_mux == mux;
  }

  void modemDisconnect(uint8_t mux) {
    if (sockSSL[mux]) {
      sendAT(GF("+CCHCLOSE="), mux);
      // OK, then +CCHCLOSE: <session_id>,<err>; just ERROR if it isn't open
      if (waitResponse(5000L, GF(AT_NL "+CCHCLOSE:")) == 1) {
        streamSkipUntil('\n');
      }
    } else {
      sendAT(GF("+CIPCLOSE="), mux);
      // OK, then +CIPCLOSE: <link_num>,<err>
      // or +CIPCLOSE: <link_num>,<err> and ERROR if it isn't open
      if (waitResponse(5000L, GF(AT_NL "+CIPCLOSE:")) == 1) {
        streamSkipUntil(',');
        if (streamGetIntBefore('\n') != 0) { waitResponse(100L); }
      }
    }
  }

  size_t modemSend(const void* buff, size_t len, uint8_t mux) {
    const uint8_t* data     = reinterpret_cast<const uint8_t*>(buff);
    const bool     ssl      = sockSSL[mux];
    const size_t   maxChunk = ssl ? 2048 : 1500;
    size_t         sent     = 0;

    while (sent < len) {
      size_t chunk = TinyGsmMin(len - sent, maxChunk);
      if (ssl) {
        sendAT(GF("+CCHSEND="), mux, ',', (uint16_t)chunk);
      } else {
        sendAT(GF("+CIPSEND="), mux, ',', (uint16_t)chunk);
      }
      if (waitResponse(GF(">")) != 1) { break; }
      stream.write(data + sent, chunk);
      stream.flush();

      if (ssl) {
        // OK, then +CCHSEND: <session_id>,<err> (report_send_result = 1)
        if (waitResponse(15000L, GF(AT_NL "+CCHSEND:")) != 1) { break; }
        streamSkipUntil(',');  // Skip session id
        if (streamGetIntBefore('\n') != 0) { break; }
        sent += chunk;
      } else {
        // OK, then +CIPSEND: <link_num>,<reqSendLength>,<cnfSendLength>
        if (waitResponse(15000L, GF(AT_NL "+CIPSEND:")) != 1) { break; }
        streamSkipUntil(',');  // Skip mux
        streamSkipUntil(',');  // Skip requested bytes to send
        int16_t confirmed = streamGetIntBefore('\n');
        if (confirmed <= 0) { break; }
        sent += confirmed;
        if ((size_t)confirmed < chunk) { break; }  // the module's buffer is full
      }
    }
    return sent;
  }

  // Waits for a "+CIPRXGET: <mode>," response.  A "+CIPRXGET: 1,<link>" data
  // notification can arrive before the actual answer; it is handled here.
  // On success the stream is positioned right after "<mode>,".
  bool waitCipRxGetResponse() {
    for (uint8_t i = 0; i < 3; i++) {
      if (waitResponse(GF("+CIPRXGET:")) != 1) { return false; }
      int16_t mode = streamGetIntBefore(',');
      if (mode != 1) { return true; }
      int16_t urcMux = streamGetIntBefore('\n');
      if (urcMux >= 0 && urcMux < TINY_GSM_MUX_COUNT && sockets[urcMux]) {
        sockets[urcMux]->got_data = true;
      }
    }
    return false;
  }

  size_t modemRead(size_t size, uint8_t mux) {
    if (!sockets[mux] || size == 0) return 0;
    size_t moved = 0;

    if (sockSSL[mux]) {
      // AT+CCHRECV=<session_id>,<max_recv_len> (max 2048, 0 means "all")
      sendAT(GF("+CCHRECV="), mux, ',',
             (uint16_t)TinyGsmMin(size, static_cast<size_t>(2048)));
      // OK comes FIRST, then one or more blocks of
      //   +CCHRECV: DATA,<session_id>,<len><CR><LF><data>
      // and finally +CCHRECV: <session_id>,<err>
      for (uint8_t blocks = 0; blocks < 16; blocks++) {
        if (waitResponse(5000L, GF("+CCHRECV: ")) != 1) { break; }
        String token = stream.readStringUntil(',');
        if (token == "DATA") {
          streamSkipUntil(',');  // Skip session id
          int16_t len = streamGetIntBefore('\n');
          if (len > 0) { moved += moveCharsFromStreamToFifo(mux, len); }
        } else {
          // final result: <session_id>,<err>
          if (streamGetIntBefore('\n') != 0) { waitResponse(100L); }
          break;
        }
      }
      sockets[mux]->sock_available = modemGetAvailableSSL(mux);
    } else {
      sendAT(GF("+CIPRXGET=2,"), mux, ',', (uint16_t)size);
      // +CIPRXGET: 2,<link_num>,<read_len>,<rest_len><CR><LF><data> ... OK
      // or "+IP ERROR: No data" and ERROR
      if (!waitCipRxGetResponse()) {
        sockets[mux]->sock_available = 0;
        return 0;
      }
      streamSkipUntil(',');  // Skip mux
      int16_t len_read = streamGetIntBefore(',');
      int16_t len_rest = streamGetIntBefore('\n');
      if (len_read > 0) { moved = moveCharsFromStreamToFifo(mux, len_read); }
      sockets[mux]->sock_available = len_rest > 0 ? len_rest : 0;
      waitResponse();
    }
    // DBG("### READ:", moved, "from", mux);
    return moved;
  }

  // AT+CCHRECV? -> +CCHRECV: LEN,<cache_len_0>,<cache_len_1>[,...]
  size_t modemGetAvailableSSL(uint8_t mux) {
    sendAT(GF("+CCHRECV?"));
    if (waitResponse(GF("+CCHRECV: LEN,")) != 1) { return 0; }
    String lengths = stream.readStringUntil('\n');
    waitResponse();
    int start = 0;
    for (uint8_t i = 0; i < mux; i++) {
      start = lengths.indexOf(',', start);
      if (start < 0) { return 0; }
      start++;
    }
    long result = lengths.substring(start).toInt();
    return result > 0 ? result : 0;
  }

  size_t modemGetAvailable(uint8_t mux) {
    if (!sockets[mux]) return 0;
    size_t result = 0;
    if (sockSSL[mux]) {
      result = modemGetAvailableSSL(mux);
    } else {
      sendAT(GF("+CIPRXGET=4,"), mux);
      // +CIPRXGET: 4,<link_num>,<rest_len>
      if (waitCipRxGetResponse()) {
        streamSkipUntil(',');  // Skip mux
        int16_t rest = streamGetIntBefore('\n');
        result       = rest > 0 ? rest : 0;
        waitResponse();
      }
    }
    // DBG("### Available:", result, "on", mux);
    if (result == 0) { sockets[mux]->sock_connected = modemGetConnected(mux); }
    return result;
  }

  bool modemGetConnected(uint8_t mux) {
    if (sockSSL[mux]) {
      // There is no status query for SSL sessions; the state is tracked
      // through the +CCHCLOSE / +CCH_PEER_CLOSED / +CCH: CCH STOP URCs
      return sockets[mux] ? sockets[mux]->sock_connected : false;
    }
    // Read the status of all TCP links at once
    // +CIPCLOSE: <link0_state>,<link1_state>,...,<link9_state>
    sendAT(GF("+CIPCLOSE?"));
    if (waitResponse(GF("+CIPCLOSE:")) != 1) {
      return sockets[mux] ? sockets[mux]->sock_connected : false;
    }
    bool result = false;
    for (int muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      int16_t state = streamGetIntBefore(
          muxNo < TINY_GSM_MUX_COUNT - 1 ? ',' : '\n');
      if (muxNo == mux) { result = (state == 1); }
      if (sockets[muxNo] && !sockSSL[muxNo]) {
        sockets[muxNo]->sock_connected = (state == 1);
      }
    }
    waitResponse();
    return result;
  }

  // Marks all TCP and/or SSL sockets as closed
  void markSocketsClosed(bool tcp, bool ssl) {
    for (int muxNo = 0; muxNo < TINY_GSM_MUX_COUNT; muxNo++) {
      if (!sockets[muxNo]) continue;
      if ((sockSSL[muxNo] && ssl) || (!sockSSL[muxNo] && tcp)) {
        sockets[muxNo]->sock_connected = false;
      }
    }
  }

  /*
   * Utilities
   */
 public:
  // NOTE: never send an AT command from here; this runs inside waitResponse()
  // and a nested command would swallow the response the caller waits for.
  bool handleURCs(String& data) {
    if (data.endsWith(GF(AT_NL "+CIPRXGET:"))) {
      int8_t mode = streamGetIntBefore(',');
      if (mode == 1) {
        int8_t mux = streamGetIntBefore('\n');
        if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
          sockets[mux]->got_data = true;
        }
        data = "";
        DBG("### Got Data:", mux);
        return true;
      } else {
        data += mode;
        return false;
      }
    } else if (data.endsWith(GF("+CCHEVENT:"))) {
      // +CCHEVENT: <session_id>,RECV EVENT
      int8_t mux   = streamGetIntBefore(',');
      String event = stream.readStringUntil('\n');
      if (event.indexOf(GF("RECV EVENT")) >= 0 && mux >= 0 &&
          mux < TINY_GSM_MUX_COUNT && sockets[mux]) {
        // maintain() will ask how much data there is
        sockets[mux]->got_data = true;
      }
      data = "";
      DBG("### Got SSL Data:", mux);
      return true;
    } else if (data.endsWith(GF("+IPCLOSE:"))) {
      // +IPCLOSE: <link_num>,<close_reason>
      int8_t mux = streamGetIntBefore(',');
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux] &&
          !sockSSL[mux]) {
        sockets[mux]->sock_connected = false;
      }
      data = "";
      streamSkipUntil('\n');
      DBG("### TCP Closed: ", mux);
      return true;
    } else if (data.endsWith(GF("+CCHCLOSE:")) ||
               data.endsWith(GF("+CCH_RECV_CLOSED:"))) {
      // +CCHCLOSE: <session_id>,<err>
      // +CCH_RECV_CLOSED: <session_id>,<err>
      int8_t mux = streamGetIntBefore(',');
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux] &&
          sockSSL[mux]) {
        sockets[mux]->sock_connected = false;
      }
      data = "";
      streamSkipUntil('\n');
      DBG("### SSL Closed: ", mux);
      return true;
    } else if (data.endsWith(GF("+CCH_PEER_CLOSED:"))) {
      int8_t mux = streamGetIntBefore('\n');
      if (mux >= 0 && mux < TINY_GSM_MUX_COUNT && sockets[mux] &&
          sockSSL[mux]) {
        sockets[mux]->sock_connected = false;
      }
      data = "";
      DBG("### SSL Closed: ", mux);
      return true;
    } else if (data.endsWith(GF("+CCH: CCH STOP"))) {
      // SSL service stopped because of a network error
      streamSkipUntil('\n');
      sslStarted = false;
      markSocketsClosed(false, true);
      data = "";
      DBG("### SSL service stopped");
      return true;
    } else if (data.endsWith(GF("+CIPEVENT:"))) {
      // +CIPEVENT: NETWORK CLOSED UNEXPECTEDLY
      streamSkipUntil('\n');
      netOpened = false;
      markSocketsClosed(true, false);
      data = "";
      DBG("### Network closed unexpectedly");
      return true;
    } else if (data.endsWith(GF("+CIPCLOSE:")) ||
               data.endsWith(GF("+NETOPEN:")) ||
               data.endsWith(GF("+NETCLOSE:")) ||
               data.endsWith(GF("+CCHSTART:")) ||
               data.endsWith(GF("+CCHSTOP:")) ||
               data.endsWith(GF("+CCHSEND:"))) {
      // Late results of commands that already timed out; nothing to do
      streamSkipUntil('\n');
      data = "";
      return true;
    } else if (data.endsWith(GF("*PSNWID:"))) {
      streamSkipUntil('\n');  // Refresh network name by network
      data = "";
      DBG("### Network name updated.");
      return true;
    } else if (data.endsWith(GF("*PSUTTZ:"))) {
      streamSkipUntil('\n');  // Refresh time and time zone by network
      data = "";
      DBG("### Network time and time zone updated.");
      return true;
    } else if (data.endsWith(GF("+CTZV:"))) {
      streamSkipUntil('\n');  // Refresh network time zone by network
      data = "";
      DBG("### Network time zone updated.");
      return true;
    } else if (data.endsWith(GF("DST:"))) {
      streamSkipUntil('\n');  // Refresh Network Daylight Saving Time by network
      data = "";
      DBG("### Daylight savings time state updated.");
      return true;
    }
    return false;
  }

 public:
  Stream& stream;

 protected:
  GsmClientA7672X* sockets[TINY_GSM_MUX_COUNT];
  // Whether each mux is an SSL session (AT+CCH*) or a TCP link (AT+CIP*)
  bool sockSSL[TINY_GSM_MUX_COUNT];
  bool netOpened  = false;
  bool sslStarted = false;
};

#endif  // SRC_TINYGSMCLIENTA7672X_H_
