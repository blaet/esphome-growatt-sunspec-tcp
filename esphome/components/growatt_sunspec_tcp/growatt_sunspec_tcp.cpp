#include "growatt_sunspec_tcp.h"
#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace esphome::growatt_sunspec_tcp {

static const char *const TAG = "growatt_sunspec_tcp";

static uint16_t be16(const uint8_t *p) { return ((uint16_t) p[0] << 8) | p[1]; }
static void put_be16(uint8_t *p, uint16_t v) {
  p[0] = v >> 8;
  p[1] = v & 0xFF;
}

void GrowattSunSpecTcp::write_string_regs(uint16_t *regs, const char *str, int max_regs) {
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

void GrowattSunSpecTcp::setup() {
  this->build_static_registers_();
  this->setup_tcp_server_();
}

void GrowattSunSpecTcp::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Growatt SunSpec TCP:\n"
                "  Address: 0x%02X\n"
                "  TCP port: %u  Unit ID: %u\n"
                "  Rated power: %u W\n"
                "  Growatt holding reg (active power %%): %u\n"
                "  Min RTU spacing: %u ms\n"
                "  SunSpec base: %u  Registers: %u\n"
                "  Manufacturer: %s  Model: %s",
                this->address_, this->tcp_port_, this->unit_id_, this->rated_power_w_, this->holding_active_pct_reg_,
                this->min_rtu_gap_ms_, SUNSPEC_BASE, TOTAL_REGS, this->manufacturer_.c_str(), this->model_.c_str());
}

void GrowattSunSpecTcp::build_static_registers_() {
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

void GrowattSunSpecTcp::refresh_registers_from_sensors_() {
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
    inv[INV_St] = (std::abs(p) > 40.0f) ? 4 : 2;
  } else {
    inv[INV_St] = 2;
  }

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

void GrowattSunSpecTcp::loop() {
  this->handle_tcp_clients_();

  uint32_t now = millis();
  if (now - this->last_sensor_refresh_ms_ >= 200) {
    this->last_sensor_refresh_ms_ = now;
    this->refresh_registers_from_sensors_();
  }

  if (this->pending_growatt_pct_ != 255 && !this->expecting_rtu_ack_ && this->ready_for_immediate_send() &&
      (this->last_rtu_command_ms_ == 0 || now - this->last_rtu_command_ms_ >= this->min_rtu_gap_ms_)) {
    uint8_t pct = this->pending_growatt_pct_;
    this->pending_growatt_pct_ = 255;
    uint16_t word = pct;
    uint8_t payload[2] = {(uint8_t) (word >> 8), (uint8_t) (word & 0xFF)};
    this->send(static_cast<uint8_t>(modbus::ModbusFunctionCode::WRITE_SINGLE_REGISTER), this->holding_active_pct_reg_, 0,
               2, payload);
    this->expecting_rtu_ack_ = true;
    this->last_rtu_command_ms_ = now;
    ESP_LOGI(TAG, "Growatt RTU: wrote holding %u = active power %% %u", this->holding_active_pct_reg_, pct);
  }
}

void GrowattSunSpecTcp::on_modbus_data(const std::vector<uint8_t> &data) {
  if (!this->expecting_rtu_ack_)
    return;
  if (data.size() != 4)
    return;
  this->expecting_rtu_ack_ = false;
  ESP_LOGD(TAG, "Growatt RTU: FC06 echo (%zu bytes)", data.size());
}

void GrowattSunSpecTcp::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  if (!this->expecting_rtu_ack_)
    return;
  this->expecting_rtu_ack_ = false;
  ESP_LOGW(TAG, "Growatt RTU: Modbus error fc=0x%02X exc=%u", function_code, exception_code);
}

#ifdef USE_ESP8266

void GrowattSunSpecTcp::setup_tcp_server_() {
  this->wifi_server_.begin(this->tcp_port_);
}

void GrowattSunSpecTcp::handle_tcp_clients_() {
  WiFiClient client = this->wifi_server_.accept();
  if (!client)
    return;
  client.setNoDelay(true);

  // Drain one full Modbus TCP ADU per accept(); MBAP length avoids fragmented-read bugs.
  uint8_t buf[260];
  size_t fill = 0;
  while (client.connected() && fill < sizeof(buf)) {
    int avail = client.available();
    if (avail <= 0)
      break;
    int n = client.read(buf + fill, std::min(avail, (int) (sizeof(buf) - fill)));
    if (n <= 0)
      break;
    fill += n;
  }

  if (fill < 8)
    return;

  uint16_t mbap_len = be16(&buf[4]);
  if (mbap_len < 2 || mbap_len > sizeof(buf) - 6 || fill < (size_t)(6 + mbap_len))
    return;

  this->process_tcp_request_(client, buf, (int) (6 + mbap_len));
}

void GrowattSunSpecTcp::process_tcp_request_(WiFiClient &client, uint8_t *buf, int len) {
  if (len < 8)
    return;

  uint16_t txn_id = be16(&buf[0]);
  uint16_t proto = be16(&buf[2]);
  uint8_t u = buf[6];
  uint8_t fc = buf[7];

  if (proto != 0)
    return;

  this->last_tcp_activity_ms_ = millis();
  this->tcp_requests_++;

  if (u != this->unit_id_) {
    ESP_LOGD(TAG, "Ignore unit_id %u (want %u)", u, this->unit_id_);
    return;
  }

  switch (fc) {
    case 0x03: {
      if (len < 12)
        return;
      uint16_t start = be16(&buf[8]);
      uint16_t count = be16(&buf[10]);
      if (count > 125) {
        this->send_tcp_error_(client, txn_id, u, fc, 0x03);
        this->tcp_errors_++;
        return;
      }
      uint16_t values[125];
      if (!this->read_sunspec_registers_(start, count, values)) {
        this->send_tcp_error_(client, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[251];
      resp[0] = (uint8_t) (count * 2);
      for (uint16_t i = 0; i < count; i++)
        put_be16(&resp[1 + i * 2], values[i]);
      this->send_tcp_response_(client, txn_id, u, fc, resp, 1 + count * 2);
      break;
    }
    case 0x06: {
      if (len < 12)
        return;
      uint16_t reg = be16(&buf[8]);
      uint16_t val = be16(&buf[10]);
      if (!this->write_sunspec_registers_(reg, 1, &val)) {
        this->send_tcp_error_(client, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[4];
      put_be16(&resp[0], reg);
      put_be16(&resp[2], val);
      this->send_tcp_response_(client, txn_id, u, fc, resp, 4);
      break;
    }
    case 0x10: {
      if (len < 13)
        return;
      uint16_t reg = be16(&buf[8]);
      uint16_t cnt = be16(&buf[10]);
      if (len < 13 + cnt * 2 || cnt > 100) {
        this->send_tcp_error_(client, txn_id, u, fc, 0x03);
        this->tcp_errors_++;
        return;
      }
      uint16_t vals[100];
      for (uint16_t i = 0; i < cnt; i++)
        vals[i] = be16(&buf[13 + i * 2]);
      if (!this->write_sunspec_registers_(reg, cnt, vals)) {
        this->send_tcp_error_(client, txn_id, u, fc, 0x02);
        this->tcp_errors_++;
        return;
      }
      uint8_t resp[4];
      put_be16(&resp[0], reg);
      put_be16(&resp[2], cnt);
      this->send_tcp_response_(client, txn_id, u, fc, resp, 4);
      break;
    }
    default:
      this->send_tcp_error_(client, txn_id, u, fc, 0x01);
      this->tcp_errors_++;
  }
}

void GrowattSunSpecTcp::send_tcp_response_(WiFiClient &client, uint16_t txn_id, uint8_t u, uint8_t fc,
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

void GrowattSunSpecTcp::send_tcp_error_(WiFiClient &client, uint16_t txn_id, uint8_t u, uint8_t fc, uint8_t err) {
  uint8_t data[1] = {err};
  this->send_tcp_response_(client, txn_id, u, fc | (uint8_t) 0x80, data, 1);
}

#else

void GrowattSunSpecTcp::setup_tcp_server_() {
  ESP_LOGE(TAG, "growatt_sunspec_tcp is only implemented for ESP8266 (USE_ESP8266)");
}

void GrowattSunSpecTcp::handle_tcp_clients_() {}

#endif

bool GrowattSunSpecTcp::read_sunspec_registers_(uint16_t start_reg, uint16_t count, uint16_t *out) {
  if (start_reg < SUNSPEC_BASE)
    return false;
  uint16_t off = start_reg - SUNSPEC_BASE;
  if ((uint32_t) off + count > TOTAL_REGS)
    return false;
  memcpy(out, &this->register_map_[off], count * sizeof(uint16_t));
  return true;
}

bool GrowattSunSpecTcp::write_sunspec_registers_(uint16_t start_reg, uint16_t count, const uint16_t *values) {
  if (start_reg < SUNSPEC_BASE)
    return false;
  uint16_t off = start_reg - SUNSPEC_BASE;

  if (off < OFF_M123 + 2 || (uint32_t) off + count > OFF_END) {
    ESP_LOGW(TAG, "Reject write @%u (SunSpec offset %u) — Model 123 only", start_reg, off);
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
  if (touched) {
    uint16_t pct = this->register_map_[lim_off];
    uint16_t ena = this->register_map_[ena_off];
    ESP_LOGI(TAG, "DER WMaxLimPct(raw)=%u ena(raw)=%u", pct, ena);
    this->forward_power_limit_(pct, ena == 1);
  }
  return true;
}

void GrowattSunSpecTcp::forward_power_limit_(uint16_t pct_raw, bool enabled) {
  float pct_f = pct_raw / 10.0f;
  uint8_t growatt_pct = 100;
  if (enabled && pct_f < 99.5f) {
    int g = (int) (pct_f + 0.5f);
    if (g < 0)
      g = 0;
    if (g > 100)
      g = 100;
    growatt_pct = (uint8_t) g;
  }
  this->pending_growatt_pct_ = growatt_pct;
  ESP_LOGI(TAG, "Queued Growatt active power %% = %u (from SunSpec %.1f%%, ena=%d)", growatt_pct, pct_f, enabled);
}

}  // namespace esphome::growatt_sunspec_tcp
