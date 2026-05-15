#include "growatt_sunspec_tcp.h"
#include "esphome/components/modbus/modbus_definitions.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::growatt_sunspec_tcp {

static const char *const TAG = "growatt_sunspec_tcp";

void GrowattSunSpecTcp::setup() {
  this->bridge_.set_log_tag(TAG);
  this->bridge_.build_static_registers();
  this->bridge_.set_der_callback([this](uint16_t pct_raw, bool enabled) { this->forward_power_limit_(pct_raw, enabled); });
  this->bridge_.setup_tcp();
  this->last_der_command_ms_ = millis();
}

void GrowattSunSpecTcp::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Growatt SunSpec TCP:\n"
                "  Address: 0x%02X\n"
                "  TCP port: %u  Unit ID: %u\n"
                "  Rated power: %u W\n"
                "  Growatt holding reg (active power %%): %u\n"
                "  Min RTU spacing: %u ms\n"
                "  DER idle revert: %u ms (0=off)\n"
                "  SunSpec base: %u  Registers: %u\n"
                "  Manufacturer: %s  Model: %s",
                this->address_, this->bridge_.get_tcp_port(), this->bridge_.get_unit_id(), this->bridge_.get_rated_power_w(),
                this->holding_active_pct_reg_, this->min_rtu_gap_ms_, this->der_idle_revert_ms_,
                sunspec_tcp_bridge::SUNSPEC_BASE, sunspec_tcp_bridge::TOTAL_REGS, this->bridge_.get_manufacturer().c_str(),
                this->bridge_.get_model().c_str());
}

void GrowattSunSpecTcp::loop() {
  this->bridge_.loop_tcp();
  this->bridge_.refresh_sensors_tick();

  uint32_t now = millis();

  if (this->der_idle_revert_ms_ > 0 && !this->expecting_rtu_ack_ &&
      (now - this->last_der_command_ms_) >= this->der_idle_revert_ms_) {
    this->last_der_command_ms_ = now;
    if (this->last_applied_growatt_pct_ == 100) {
      ESP_LOGD(TAG, "DER idle timeout: Growatt already at 100%%, skip redundant FC06");
    } else {
      this->pending_growatt_pct_ = 100;
      ESP_LOGI(TAG, "DER idle timeout (%u ms): Growatt active power %% -> 100", (unsigned) this->der_idle_revert_ms_);
    }
  }

  if (this->pending_growatt_pct_ != 255 && !this->expecting_rtu_ack_ && this->ready_for_immediate_send() &&
      (this->last_rtu_command_ms_ == 0 || now - this->last_rtu_command_ms_ >= this->min_rtu_gap_ms_)) {
    uint8_t pct = this->pending_growatt_pct_;
    this->pending_growatt_pct_ = 255;
    this->inflight_growatt_pct_ = pct;
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
  if (this->inflight_growatt_pct_ != 255) {
    this->last_applied_growatt_pct_ = this->inflight_growatt_pct_;
    this->inflight_growatt_pct_ = 255;
  }
  ESP_LOGD(TAG, "Growatt RTU: FC06 echo (%zu bytes)", data.size());
}

void GrowattSunSpecTcp::on_modbus_error(uint8_t function_code, uint8_t exception_code) {
  if (!this->expecting_rtu_ack_)
    return;
  this->expecting_rtu_ack_ = false;
  this->inflight_growatt_pct_ = 255;
  this->last_applied_growatt_pct_ = 255;
  ESP_LOGW(TAG, "Growatt RTU: Modbus error fc=0x%02X exc=%u", function_code, exception_code);
}

void GrowattSunSpecTcp::forward_power_limit_(uint16_t pct_raw, bool enabled) {
  this->last_der_command_ms_ = millis();
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
  if (this->last_applied_growatt_pct_ != 255 && growatt_pct == this->last_applied_growatt_pct_) {
    ESP_LOGD(TAG, "DER unchanged (active power %%=%u), skip FC06", growatt_pct);
    return;
  }
  this->pending_growatt_pct_ = growatt_pct;
  ESP_LOGI(TAG, "Queued Growatt active power %% = %u (from SunSpec %.1f%%, ena=%d)", growatt_pct, pct_f, enabled);
}

}  // namespace esphome::growatt_sunspec_tcp
