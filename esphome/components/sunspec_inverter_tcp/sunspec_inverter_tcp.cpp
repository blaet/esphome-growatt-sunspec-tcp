#include "sunspec_inverter_tcp.h"
#include "esphome/core/log.h"

namespace esphome::sunspec_inverter_tcp {

static const char *const TAG = "sunspec_inverter_tcp";

void SunspecInverterTcp::setup() {
  this->bridge_.set_log_tag(TAG);
  this->bridge_.build_static_registers();
  this->bridge_.setup_tcp();
}

void SunspecInverterTcp::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Sunspec inverter TCP (sensor-only):\n"
                "  TCP port: %u  Unit ID: %u\n"
                "  Rated power: %u W\n"
                "  SunSpec base: %u  Registers: %u\n"
                "  Manufacturer: %s  Model: %s\n"
                "  Model 123 writes: RAM only (no DER to inverter)",
                this->bridge_.get_tcp_port(), this->bridge_.get_unit_id(), this->bridge_.get_rated_power_w(),
                sunspec_tcp_bridge::SUNSPEC_BASE, sunspec_tcp_bridge::TOTAL_REGS, this->bridge_.get_manufacturer().c_str(),
                this->bridge_.get_model().c_str());
}

void SunspecInverterTcp::loop() {
  this->bridge_.loop_tcp();
  this->bridge_.refresh_sensors_tick();
}

}  // namespace esphome::sunspec_inverter_tcp
