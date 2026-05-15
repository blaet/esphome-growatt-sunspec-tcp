#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "sunspec_tcp_bridge.h"

namespace esphome::sunspec_inverter_tcp {

/** Sensor-driven SunSpec Modbus TCP (models 1, 101, 120, 123). Model 123 writes update RAM only — no inverter RTU. */
class SunspecInverterTcp : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_tcp_port(uint16_t p) { this->bridge_.set_tcp_port(p); }
  void set_unit_id(uint8_t id) { this->bridge_.set_unit_id(id); }
  void set_rated_power_w(uint16_t w) { this->bridge_.set_rated_power_w(w); }
  void set_manufacturer(const std::string &s) { this->bridge_.set_manufacturer(s); }
  void set_model(const std::string &s) { this->bridge_.set_model(s); }
  void set_serial(const std::string &s) { this->bridge_.set_serial(s); }

  void set_ac_voltage_sensor(sensor::Sensor *s) { this->bridge_.set_ac_voltage_sensor(s); }
  void set_ac_current_sensor(sensor::Sensor *s) { this->bridge_.set_ac_current_sensor(s); }
  void set_ac_power_sensor(sensor::Sensor *s) { this->bridge_.set_ac_power_sensor(s); }
  void set_frequency_sensor(sensor::Sensor *s) { this->bridge_.set_frequency_sensor(s); }
  void set_energy_kwh_sensor(sensor::Sensor *s) { this->bridge_.set_energy_kwh_sensor(s); }
  void set_pv_power_sensor(sensor::Sensor *s) { this->bridge_.set_pv_power_sensor(s); }
  void set_cabinet_temp_sensor(sensor::Sensor *s) { this->bridge_.set_cabinet_temp_sensor(s); }

 protected:
  sunspec_tcp_bridge::SunspecTcpBridge bridge_{};
};

}  // namespace esphome::sunspec_inverter_tcp
