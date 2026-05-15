#include "sunspec_tcp_bridge.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(USE_ESP32)
#include "lwip/sockets.h"
#include <fcntl.h>
#include <unistd.h>
#endif

namespace esphome::sunspec_tcp_bridge {

static uint16_t be16(const uint8_t *p) { return ((uint16_t) p[0] << 8) | p[1]; }
static void put_be16(uint8_t *p, uint16_t v) {
  p[0] = v >> 8;
  p[1] = v & 0xFF;
}

void SunspecTcpBridge::write_string_regs(uint16_t *regs, const char *str, int max_regs) {
  memset(regs, 0, max_regs * sizeof(uint16_t));
  int len = strlen(str);
  if (len > max_regs * 2)
    len = max_regs * 2;
  for (int i = 0; i < len; i++) {
    if (i % 2 == 0)
      regs[i / 2] = ((uint16_t) (uint8_t) str[i]) << 8;
    else
      regs[i / 2] |= (uint8_t) str[i];
  }
}

void SunspecTcpBridge::build_static_registers() {
  this->register_map_.fill(0xFFFF);

  this->register_map_[OFF_SUNS] = 0x5375;
  this->register_map_[OFF_SUNS + 1] = 0x6e53;

  this->register_map_[OFF_MODEL1] = 1;
  this->register_map_[OFF_MODEL1 + 1] = MODEL_1_SIZE;
  uint16_t *m1 = &this->register_map_[OFF_MODEL1 + 2];
  for (int i = 0; i < MODEL_1_SIZE; i++)
    m1[i] = 0;
  write_string_regs(&m1[0], this->manufacturer_.c_str(), 16);
  write_string_regs(&m1[16], this->model_.c_str(), 16);
  write_string_regs(&m1[40], "1.0.0", 8);
  write_string_regs(&m1[48], this->serial_.c_str(), 16);
  m1[64] = this->unit_id_;
  m1[65] = 0x8000;

  this->register_map_[OFF_INV] = 101;
  this->register_map_[OFF_INV + 1] = MODEL_101_SIZE;
  uint16_t *inv = &this->register_map_[OFF_INV + 2];
  for (int i = 0; i < MODEL_101_SIZE; i++)
    inv[i] = 0xFFFF;

  inv[INV_A_SF] = (uint16_t) (int16_t) -2;
  inv[INV_V_SF] = (uint16_t) (int16_t) -1;
  inv[INV_W_SF] = (uint16_t) (int16_t) 0;
  inv[INV_Hz_SF] = (uint16_t) (int16_t) -2;
  inv[INV_VA_SF] = (uint16_t) (int16_t) 0;
  inv[INV_VAr_SF] = (uint16_t) (int16_t) 0;
  inv[INV_PF_SF] = (uint16_t) (int16_t) -2;
  inv[INV_WH_SF] = (uint16_t) (int16_t) 0;
  inv[INV_DCA_SF] = (uint16_t) (int16_t) -2;
  inv[INV_DCV_SF] = (uint16_t) (int16_t) -1;
  inv[INV_DCW_SF] = (uint16_t) (int16_t) 0;
  inv[INV_Tmp_SF] = (uint16_t) (int16_t) -1;
  inv[INV_St] = 2;
  inv[INV_Evt1] = 0;
  inv[INV_Evt1 + 1] = 0;

  this->register_map_[OFF_M120] = 120;
  this->register_map_[OFF_M120 + 1] = MODEL_120_SIZE;
  uint16_t *m120 = &this->register_map_[OFF_M120 + 2];
  for (int i = 0; i < MODEL_120_SIZE; i++)
    m120[i] = 0xFFFF;
  m120[0] = 4;
  m120[1] = this->rated_power_w_;
  m120[2] = 0;
  m120[3] = this->rated_power_w_;
  m120[4] = 0;
  float rated_a = this->rated_power_w_ > 0 ? (float) this->rated_power_w_ / 230.0f : 13.0f;
  m120[10] = (uint16_t) (rated_a * 10.0f);
  m120[11] = (uint16_t) (int16_t) -1;

  this->register_map_[OFF_M123] = 123;
  this->register_map_[OFF_M123 + 1] = MODEL_123_SIZE;
  uint16_t *m123 = &this->register_map_[OFF_M123 + 2];
  for (int i = 0; i < MODEL_123_SIZE; i++)
    m123[i] = 0xFFFF;
  m123[2] = 1;
  m123[3] = (uint16_t) (int16_t) -1;
  m123[5] = 1000;
  m123[8] = 0;

  this->register_map_[OFF_END] = 0xFFFF;
  this->register_map_[OFF_END + 1] = 0;
}

static float sensor_v(sensor::Sensor *s) {
  if (s == nullptr || !s->has_state())
    return NAN;
  return s->get_state();
}

void SunspecTcpBridge::refresh_sensors_tick() {
  uint32_t now = millis();
  if (now - this->last_sensor_refresh_ms_ < 200)
    return;
  this->last_sensor_refresh_ms_ = now;

  uint16_t *inv = &this->register_map_[OFF_INV + 2];

  float v = sensor_v(this->ac_voltage_s_);
  float a = sensor_v(this->ac_current_s_);
  float p = sensor_v(this->ac_power_s_);
  float hz = sensor_v(this->frequency_s_);
  float kwh = sensor_v(this->energy_kwh_s_);
  float pvp = sensor_v(this->pv_power_s_);
  float tc = sensor_v(this->cabinet_temp_s_);

  if (!std::isnan(v)) {
    int vi = (int) (v * 10.0f);
    if (vi < 0)
      vi = 0;
    if (vi > 65535)
      vi = 65535;
    inv[INV_PhVphA] = (uint16_t) vi;
  }

  if (!std::isnan(a)) {
    int ai = (int) (a * 100.0f);
    if (ai < 0)
      ai = 0;
    if (ai > 65535)
      ai = 65535;
    inv[INV_A] = (uint16_t) ai;
    inv[INV_AphA] = (uint16_t) ai;
  }

  if (!std::isnan(p)) {
    int pi = (int) p;
    if (pi < -32768)
      pi = -32768;
    if (pi > 32767)
      pi = 32767;
    inv[INV_W] = (uint16_t) (int16_t) pi;
  }

  const float ac_on_w = 22.0f;
  const float ac_off_w = 8.0f;
  const float pv_on_w = 35.0f;
  const float pv_off_w = 15.0f;
  const bool have_ac = !std::isnan(p);
  const bool have_pv = !std::isnan(pvp);
  const bool ac_strong = have_ac && std::abs(p) >= ac_on_w;
  const bool pv_strong = have_pv && pvp >= pv_on_w;
  const bool ac_weak = !have_ac || std::abs(p) < ac_off_w;
  const bool pv_weak = !have_pv || pvp < pv_off_w;
  if (!this->inv_st_producing_latched_) {
    if (ac_strong || pv_strong)
      this->inv_st_producing_latched_ = true;
  } else if (ac_weak && pv_weak) {
    this->inv_st_producing_latched_ = false;
  }
  inv[INV_St] = this->inv_st_producing_latched_ ? 4 : 2;

  if (!std::isnan(hz)) {
    int hi = (int) (hz * 100.0f);
    if (hi < 0)
      hi = 0;
    if (hi > 65535)
      hi = 65535;
    inv[INV_Hz] = (uint16_t) hi;
  }

  if (!std::isnan(kwh) && kwh >= 0) {
    uint32_t wh = (uint32_t) (kwh * 1000.0f);
    inv[INV_WH] = (uint16_t) (wh >> 16);
    inv[INV_WH + 1] = (uint16_t) (wh & 0xFFFF);
  }

  if (!std::isnan(pvp)) {
    int dpi = (int) pvp;
    if (dpi < -32768)
      dpi = -32768;
    if (dpi > 32767)
      dpi = 32767;
    inv[INV_DCW] = (uint16_t) (int16_t) dpi;
  }

  if (!std::isnan(tc)) {
    int ti = (int) (tc * 10.0f);
    if (ti < -32768)
      ti = -32768;
    if (ti > 32767)
      ti = 32767;
    inv[INV_TmpCab] = (uint16_t) (int16_t) ti;
  }

  inv[INV_PF] = (uint16_t) (int16_t) 100;
}

bool SunspecTcpBridge::read_sunspec_registers(uint16_t start_reg, uint16_t count, uint16_t *out) {
  if (start_reg < SUNSPEC_BASE)
    return false;
  uint16_t off = start_reg - SUNSPEC_BASE;
  if ((uint32_t) off + count > TOTAL_REGS)
    return false;
  memcpy(out, &this->register_map_[off], count * sizeof(uint16_t));
  return true;
}

bool SunspecTcpBridge::write_sunspec_registers(uint16_t start_reg, uint16_t count, const uint16_t *values) {
  if (start_reg < SUNSPEC_BASE)
    return false;
  uint16_t off = start_reg - SUNSPEC_BASE;

  if (off < OFF_M123 + 2 || (uint32_t) off + count > OFF_END) {
    ESP_LOGW(this->log_tag_, "Reject write @%u (SunSpec offset %u) — Model 123 only", start_reg, off);
    return false;
  }

  for (uint16_t i = 0; i < count; i++)
    this->register_map_[off + i] = values[i];

  uint16_t lim_off = OFF_M123 + 2 + 5;
  uint16_t ena_off = OFF_M123 + 2 + 8;
  bool touched = false;
  for (uint16_t r = off; r < off + count; r++) {
    if (r == lim_off || r == ena_off) {
      touched = true;
      break;
    }
  }
  if (touched && this->der_callback_) {
    uint16_t pct = this->register_map_[lim_off];
    uint16_t ena = this->register_map_[ena_off];
    ESP_LOGI(this->log_tag_, "DER WMaxLimPct(raw)=%u ena(raw)=%u", pct, ena);
    this->der_callback_(pct, ena == 1);
  }
  return true;
}

#ifdef USE_ESP8266

void SunspecTcpBridge::setup_tcp() { this->wifi_server_.begin(this->tcp_port_); }

void SunspecTcpBridge::loop_tcp() {
  if (!this->tcp_client_.connected()) {
    this->tcp_client_ = this->wifi_server_.accept();
    this->tcp_rx_fill_ = 0;
    if (!this->tcp_client_)
      return;
    this->tcp_client_.setNoDelay(true);
  }

  while (this->tcp_client_.available() && this->tcp_rx_fill_ < TCP_RX_CAP) {
    int n = this->tcp_client_.read(this->tcp_rx_buf_ + this->tcp_rx_fill_, TCP_RX_CAP - this->tcp_rx_fill_);
    if (n <= 0)
      break;
    this->tcp_rx_fill_ += n;
  }

  while (this->tcp_rx_fill_ >= 6) {
    uint16_t mbap_len = be16(&this->tcp_rx_buf_[4]);
    if (mbap_len < 2 || mbap_len > TCP_RX_CAP - 6) {
      ESP_LOGW(this->log_tag_, "Invalid MBAP length %u — closing TCP", mbap_len);
      this->tcp_client_.stop();
      this->tcp_rx_fill_ = 0;
      return;
    }
    size_t need = 6 + mbap_len;
    if (this->tcp_rx_fill_ < need)
      break;
    this->process_tcp_request_8266_(this->tcp_client_, this->tcp_rx_buf_, (int) need);
    memmove(this->tcp_rx_buf_, this->tcp_rx_buf_ + need, this->tcp_rx_fill_ - need);
    this->tcp_rx_fill_ -= need;
  }

  if (this->tcp_rx_fill_ >= TCP_RX_CAP) {
    ESP_LOGW(this->log_tag_, "Modbus TCP RX buffer full — closing TCP");
    this->tcp_client_.stop();
    this->tcp_rx_fill_ = 0;
  }
}

void SunspecTcpBridge::process_tcp_request_8266_(WiFiClient &client, uint8_t *buf, int len) {
  if (len < 8)
    return;

  uint16_t txn_id = be16(&buf[0]);
  uint16_t proto = be16(&buf[2]);
  uint8_t u = buf[6];
  uint8_t fc = buf[7];

  if (proto != 0)
    return;

  this->tcp_requests_++;

  if (u != this->unit_id_) {
    ESP_LOGD(this->log_tag_, "Ignore unit_id %u (want %u)", u, this->unit_id_);
    return;
  }

  switch (fc) {
    case 0x03: {
      if (len < 12)
        return;
      uint16_t start = be16(&buf[8]);
      uint16_t count = be16(&buf[10]);
      if (count > 125) {
        this->send_tcp_error_8266_(client, txn_id, u, fc, 0x03);
        this->tcp_errors_++;
        return;
      }
      uint16_t values[125];
      if (!this->read_sunspec_registers(start, count, values)) {
        this->send_tcp_error_8266_(client, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[251];
      resp[0] = (uint8_t) (count * 2);
      for (uint16_t i = 0; i < count; i++)
        put_be16(&resp[1 + i * 2], values[i]);
      this->send_tcp_response_8266_(client, txn_id, u, fc, resp, 1 + count * 2);
      break;
    }
    case 0x06: {
      if (len < 12)
        return;
      uint16_t reg = be16(&buf[8]);
      uint16_t val = be16(&buf[10]);
      if (!this->write_sunspec_registers(reg, 1, &val)) {
        this->send_tcp_error_8266_(client, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[4];
      put_be16(&resp[0], reg);
      put_be16(&resp[2], val);
      this->send_tcp_response_8266_(client, txn_id, u, fc, resp, 4);
      break;
    }
    case 0x10: {
      if (len < 13)
        return;
      uint16_t reg = be16(&buf[8]);
      uint16_t cnt = be16(&buf[10]);
      if (len < 13 + cnt * 2 || cnt > 100) {
        this->send_tcp_error_8266_(client, txn_id, u, fc, 0x03);
        this->tcp_errors_++;
        return;
      }
      uint16_t vals[100];
      for (uint16_t i = 0; i < cnt; i++)
        vals[i] = be16(&buf[13 + i * 2]);
      if (!this->write_sunspec_registers(reg, cnt, vals)) {
        this->send_tcp_error_8266_(client, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[4];
      put_be16(&resp[0], reg);
      put_be16(&resp[2], cnt);
      this->send_tcp_response_8266_(client, txn_id, u, fc, resp, 4);
      break;
    }
    default:
      this->send_tcp_error_8266_(client, txn_id, u, fc, 0x01);
      this->tcp_errors_++;
  }
}

void SunspecTcpBridge::send_tcp_response_8266_(WiFiClient &client, uint16_t txn_id, uint8_t u, uint8_t fc,
                                              const uint8_t *data, uint16_t data_len) {
  uint8_t frame[260];
  put_be16(&frame[0], txn_id);
  put_be16(&frame[2], 0);
  put_be16(&frame[4], (uint16_t) (1 + 1 + data_len));
  frame[6] = u;
  frame[7] = fc;
  memcpy(&frame[8], data, data_len);
  client.write(frame, 8 + data_len);
  client.flush();
}

void SunspecTcpBridge::send_tcp_error_8266_(WiFiClient &client, uint16_t txn_id, uint8_t u, uint8_t fc, uint8_t err) {
  uint8_t data[1] = {err};
  this->send_tcp_response_8266_(client, txn_id, u, fc | (uint8_t) 0x80, data, 1);
}

#elif defined(USE_ESP32)

void SunspecTcpBridge::close_client_sock_() {
  if (this->client_sock_ >= 0) {
    shutdown(this->client_sock_, SHUT_RDWR);
    close(this->client_sock_);
    this->client_sock_ = -1;
  }
  this->tcp_rx_fill32_ = 0;
}

void SunspecTcpBridge::setup_tcp() {
  this->listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (this->listen_sock_ < 0) {
    ESP_LOGE(this->log_tag_, "socket() failed");
    return;
  }
  int yes = 1;
  setsockopt(this->listen_sock_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in sa {};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(this->tcp_port_);
  sa.sin_addr.s_addr = INADDR_ANY;

  if (bind(this->listen_sock_, (sockaddr *) &sa, sizeof(sa)) != 0) {
    ESP_LOGE(this->log_tag_, "bind() port %u failed", (unsigned) this->tcp_port_);
    close(this->listen_sock_);
    this->listen_sock_ = -1;
    return;
  }
  if (listen(this->listen_sock_, 1) != 0) {
    ESP_LOGE(this->log_tag_, "listen() failed");
    close(this->listen_sock_);
    this->listen_sock_ = -1;
    return;
  }
  int fl = fcntl(this->listen_sock_, F_GETFL, 0);
  if (fl >= 0)
    fcntl(this->listen_sock_, F_SETFL, fl | O_NONBLOCK);
  this->client_sock_ = -1;
  ESP_LOGI(this->log_tag_, "Modbus TCP listen on port %u", (unsigned) this->tcp_port_);
}

void SunspecTcpBridge::loop_tcp() {
  if (this->listen_sock_ < 0)
    return;

  if (this->client_sock_ < 0) {
    sockaddr_in peer {};
    socklen_t plen = sizeof(peer);
    int c = accept(this->listen_sock_, (sockaddr *) &peer, &plen);
    if (c >= 0) {
      int fl = fcntl(c, F_GETFL, 0);
      if (fl >= 0)
        fcntl(c, F_SETFL, fl | O_NONBLOCK);
      this->client_sock_ = c;
      this->tcp_rx_fill32_ = 0;
    }
  }

  if (this->client_sock_ < 0)
    return;

  while (this->tcp_rx_fill32_ < TCP_RX_CAP32) {
    ssize_t n = recv(this->client_sock_, this->tcp_rx_buf32_ + this->tcp_rx_fill32_, TCP_RX_CAP32 - this->tcp_rx_fill32_, 0);
    if (n > 0) {
      this->tcp_rx_fill32_ += (size_t) n;
      continue;
    }
    if (n == 0) {
      this->close_client_sock_();
      return;
    }
    break;
  }

  while (this->tcp_rx_fill32_ >= 6) {
    uint16_t mbap_len = be16(&this->tcp_rx_buf32_[4]);
    if (mbap_len < 2 || mbap_len > TCP_RX_CAP32 - 6) {
      ESP_LOGW(this->log_tag_, "Invalid MBAP length %u — closing TCP", mbap_len);
      this->close_client_sock_();
      return;
    }
    size_t need = 6 + mbap_len;
    if (this->tcp_rx_fill32_ < need)
      break;
    this->process_tcp_request_sock_(this->client_sock_, this->tcp_rx_buf32_, (int) need);
    memmove(this->tcp_rx_buf32_, this->tcp_rx_buf32_ + need, this->tcp_rx_fill32_ - need);
    this->tcp_rx_fill32_ -= need;
  }

  if (this->tcp_rx_fill32_ >= TCP_RX_CAP32) {
    ESP_LOGW(this->log_tag_, "Modbus TCP RX buffer full — closing TCP");
    this->close_client_sock_();
  }
}

void SunspecTcpBridge::send_tcp_response_sock_(int sock, uint16_t txn_id, uint8_t u, uint8_t fc, const uint8_t *data,
                                               uint16_t data_len) {
  uint8_t frame[260];
  put_be16(&frame[0], txn_id);
  put_be16(&frame[2], 0);
  put_be16(&frame[4], (uint16_t) (1 + 1 + data_len));
  frame[6] = u;
  frame[7] = fc;
  memcpy(&frame[8], data, data_len);
  (void) send(sock, frame, 8 + data_len, 0);
}

void SunspecTcpBridge::send_tcp_error_sock_(int sock, uint16_t txn_id, uint8_t u, uint8_t fc, uint8_t err) {
  uint8_t data[1] = {err};
  this->send_tcp_response_sock_(sock, txn_id, u, fc | (uint8_t) 0x80, data, 1);
}

void SunspecTcpBridge::process_tcp_request_sock_(int sock, uint8_t *buf, int len) {
  if (len < 8)
    return;

  uint16_t txn_id = be16(&buf[0]);
  uint16_t proto = be16(&buf[2]);
  uint8_t u = buf[6];
  uint8_t fc = buf[7];

  if (proto != 0)
    return;

  this->tcp_requests_++;

  if (u != this->unit_id_) {
    ESP_LOGD(this->log_tag_, "Ignore unit_id %u (want %u)", u, this->unit_id_);
    return;
  }

  switch (fc) {
    case 0x03: {
      if (len < 12)
        return;
      uint16_t start = be16(&buf[8]);
      uint16_t count = be16(&buf[10]);
      if (count > 125) {
        this->send_tcp_error_sock_(sock, txn_id, u, fc, 0x03);
        this->tcp_errors_++;
        return;
      }
      uint16_t values[125];
      if (!this->read_sunspec_registers(start, count, values)) {
        this->send_tcp_error_sock_(sock, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[251];
      resp[0] = (uint8_t) (count * 2);
      for (uint16_t i = 0; i < count; i++)
        put_be16(&resp[1 + i * 2], values[i]);
      this->send_tcp_response_sock_(sock, txn_id, u, fc, resp, 1 + count * 2);
      break;
    }
    case 0x06: {
      if (len < 12)
        return;
      uint16_t reg = be16(&buf[8]);
      uint16_t val = be16(&buf[10]);
      if (!this->write_sunspec_registers(reg, 1, &val)) {
        this->send_tcp_error_sock_(sock, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[4];
      put_be16(&resp[0], reg);
      put_be16(&resp[2], val);
      this->send_tcp_response_sock_(sock, txn_id, u, fc, resp, 4);
      break;
    }
    case 0x10: {
      if (len < 13)
        return;
      uint16_t reg = be16(&buf[8]);
      uint16_t cnt = be16(&buf[10]);
      if (len < 13 + cnt * 2 || cnt > 100) {
        this->send_tcp_error_sock_(sock, txn_id, u, fc, 0x03);
        this->tcp_errors_++;
        return;
      }
      uint16_t vals[100];
      for (uint16_t i = 0; i < cnt; i++)
        vals[i] = be16(&buf[13 + i * 2]);
      if (!this->write_sunspec_registers(reg, cnt, vals)) {
        this->send_tcp_error_sock_(sock, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[4];
      put_be16(&resp[0], reg);
      put_be16(&resp[2], cnt);
      this->send_tcp_response_sock_(sock, txn_id, u, fc, resp, 4);
      break;
    }
    default:
      this->send_tcp_error_sock_(sock, txn_id, u, fc, 0x01);
      this->tcp_errors_++;
  }
}

#else

void SunspecTcpBridge::setup_tcp() { ESP_LOGE(this->log_tag_, "SunspecTcpBridge: no TCP stack for this target"); }
void SunspecTcpBridge::loop_tcp() {}

#endif

}  // namespace esphome::sunspec_tcp_bridge
