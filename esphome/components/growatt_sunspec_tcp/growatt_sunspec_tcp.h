#pragma once

#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#endif

#include "esphome/core/component.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"
#include <array>
#include <cstring>
#include <vector>

namespace esphome::growatt_sunspec_tcp {

// Victron SunSpec base address (PDU / logical)
static const uint16_t SUNSPEC_BASE = 40000;

static const uint16_t MODEL_1_SIZE = 66;
static const uint16_t MODEL_101_SIZE = 50;
static const uint16_t MODEL_120_SIZE = 26;
static const uint16_t MODEL_123_SIZE = 24;

// Model 101 field offsets (after id + length words)
static const uint16_t INV_A = 0;
static const uint16_t INV_AphA = 1;
static const uint16_t INV_A_SF = 4;
static const uint16_t INV_PhVphA = 8;
static const uint16_t INV_V_SF = 11;
static const uint16_t INV_W = 12;
static const uint16_t INV_W_SF = 13;
static const uint16_t INV_Hz = 14;
static const uint16_t INV_Hz_SF = 15;
static const uint16_t INV_VA = 16;
static const uint16_t INV_VA_SF = 17;
static const uint16_t INV_VAr = 18;
static const uint16_t INV_VAr_SF = 19;
static const uint16_t INV_PF = 20;
static const uint16_t INV_PF_SF = 21;
static const uint16_t INV_WH = 22;
static const uint16_t INV_WH_SF = 24;
static const uint16_t INV_DCA = 25;
static const uint16_t INV_DCA_SF = 26;
static const uint16_t INV_DCV = 27;
static const uint16_t INV_DCV_SF = 28;
static const uint16_t INV_DCW = 29;
static const uint16_t INV_DCW_SF = 30;
static const uint16_t INV_TmpCab = 31;
static const uint16_t INV_Tmp_SF = 35;
static const uint16_t INV_St = 36;
static const uint16_t INV_Evt1 = 38;

static const uint16_t OFF_SUNS = 0;
static const uint16_t OFF_MODEL1 = 2;
static const uint16_t OFF_INV = 70;
static const uint16_t OFF_M120 = 122;
static const uint16_t OFF_M123 = 150;
static const uint16_t OFF_END = 176;
static const uint16_t TOTAL_REGS = 178;

class GrowattSunSpecTcp : public Component, public modbus::ModbusDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void on_modbus_data(const std::vector<uint8_t> &data) override;
  void on_modbus_error(uint8_t function_code, uint8_t exception_code) override;

  void set_tcp_port(uint16_t p) { tcp_port_ = p; }
  void set_unit_id(uint8_t id) { unit_id_ = id; }
  void set_rated_power_w(uint16_t w) { rated_power_w_ = w; }
  void set_manufacturer(const std::string &s) { manufacturer_ = s; }
  void set_model(const std::string &s) { model_ = s; }
  void set_serial(const std::string &s) { serial_ = s; }
  void set_holding_active_pct_reg(uint16_t r) { holding_active_pct_reg_ = r; }
  void set_min_rtu_gap_ms(uint32_t ms) { min_rtu_gap_ms_ = ms; }

  void set_ac_voltage_sensor(sensor::Sensor *s) { ac_voltage_s_ = s; }
  void set_ac_current_sensor(sensor::Sensor *s) { ac_current_s_ = s; }
  void set_ac_power_sensor(sensor::Sensor *s) { ac_power_s_ = s; }
  void set_frequency_sensor(sensor::Sensor *s) { frequency_s_ = s; }
  void set_energy_kwh_sensor(sensor::Sensor *s) { energy_kwh_s_ = s; }
  void set_pv_power_sensor(sensor::Sensor *s) { pv_power_s_ = s; }
  void set_cabinet_temp_sensor(sensor::Sensor *s) { cabinet_temp_s_ = s; }

 protected:
  void setup_tcp_server_();
  void handle_tcp_clients_();

  uint16_t tcp_port_{502};

#ifdef USE_ESP8266
  static constexpr size_t TCP_RX_CAP = 260;
  void process_tcp_request_(WiFiClient &client, uint8_t *buf, int len);
  void send_tcp_response_(WiFiClient &client, uint16_t transaction_id, uint8_t u, uint8_t fc, const uint8_t *data,
                          uint16_t data_len);
  void send_tcp_error_(WiFiClient &client, uint16_t transaction_id, uint8_t u, uint8_t fc, uint8_t err);
  WiFiServer wifi_server_{80};
  WiFiClient tcp_client_;
  uint8_t tcp_rx_buf_[TCP_RX_CAP]{};
  size_t tcp_rx_fill_{0};
#endif

  bool read_sunspec_registers_(uint16_t start_reg, uint16_t count, uint16_t *out);
  bool write_sunspec_registers_(uint16_t start_reg, uint16_t count, const uint16_t *values);

  void build_static_registers_();
  void refresh_registers_from_sensors_();
  void forward_power_limit_(uint16_t pct_raw_sunspec, bool enabled);
  static void write_string_regs(uint16_t *regs, const char *str, int max_regs);

  uint8_t unit_id_{126};
  uint16_t rated_power_w_{3000};
  std::string manufacturer_{"Growatt"};
  std::string model_{"3000-S"};
  std::string serial_{"GROWATT-SUNSPEC"};
  uint16_t holding_active_pct_reg_{3};
  uint32_t min_rtu_gap_ms_{850};

  std::array<uint16_t, TOTAL_REGS> register_map_{};

  uint32_t tcp_requests_{0};
  uint32_t tcp_errors_{0};
  uint32_t last_tcp_activity_ms_{0};
  uint32_t last_rtu_command_ms_{0};
  bool expecting_rtu_ack_{false};
  uint8_t pending_growatt_pct_{255};
  uint32_t last_sensor_refresh_ms_{0};

  sensor::Sensor *ac_voltage_s_{nullptr};
  sensor::Sensor *ac_current_s_{nullptr};
  sensor::Sensor *ac_power_s_{nullptr};
  sensor::Sensor *frequency_s_{nullptr};
  sensor::Sensor *energy_kwh_s_{nullptr};
  sensor::Sensor *pv_power_s_{nullptr};
  sensor::Sensor *cabinet_temp_s_{nullptr};
};

}  // namespace esphome::growatt_sunspec_tcp
