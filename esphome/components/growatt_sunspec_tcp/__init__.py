"""SunSpec Modbus TCP façade for Growatt (classic RTU) + Victron GX."""

from esphome.components import modbus, sensor
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL

CODEOWNERS = ["@blaet"]
DEPENDENCIES = ["modbus", "sensor", "wifi"]
AUTO_LOAD = ["sensor"]

growatt_sunspec_tcp_ns = cg.esphome_ns.namespace("growatt_sunspec_tcp")
GrowattSunSpecTcp = growatt_sunspec_tcp_ns.class_(
    "GrowattSunSpecTcp", cg.Component, modbus.ModbusDevice
)

CONF_TCP_PORT = "tcp_port"
CONF_UNIT_ID = "unit_id"
CONF_RATED_POWER_W = "rated_power_w"
CONF_MANUFACTURER = "manufacturer"
CONF_SERIAL = "serial"
CONF_HOLDING_ACTIVE_PCT = "holding_register_active_power_pct"
CONF_MIN_RTU_GAP = "min_rtu_command_gap"
CONF_FULL_POWER_AFTER_DER_SILENCE = "full_power_after_der_silence"

CONF_AC_VOLTAGE = "ac_voltage"
CONF_AC_CURRENT = "ac_current"
CONF_AC_POWER = "ac_power"
CONF_FREQUENCY = "frequency"
CONF_ENERGY_KWH = "energy_kwh"
CONF_PV_POWER = "pv_power"
CONF_CABINET_TEMP = "cabinet_temp"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GrowattSunSpecTcp),
            cv.Optional(CONF_TCP_PORT, default=502): cv.port,
            cv.Optional(CONF_UNIT_ID, default=126): cv.int_range(min=1, max=247),
            cv.Optional(CONF_RATED_POWER_W, default=3000): cv.int_range(min=100, max=65535),
            cv.Optional(CONF_MANUFACTURER, default="Growatt"): cv.string,
            cv.Optional(CONF_MODEL, default="3000-S"): cv.string,
            cv.Optional(CONF_SERIAL, default="GROWATT-SUNSPEC"): cv.string,
            cv.Optional(CONF_HOLDING_ACTIVE_PCT, default=3): cv.int_range(min=0, max=65535),
            cv.Optional(CONF_MIN_RTU_GAP, default="850ms"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_FULL_POWER_AFTER_DER_SILENCE, default="5min"): cv.Any(
                cv.one_of("never"),
                cv.positive_time_period_milliseconds,
            ),
            cv.Optional(CONF_AC_VOLTAGE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_AC_CURRENT): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_AC_POWER): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_FREQUENCY): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_ENERGY_KWH): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_PV_POWER): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_CABINET_TEMP): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(modbus.modbus_device_schema(0x01))
)


FINAL_VALIDATE_SCHEMA = modbus.final_validate_modbus_device(
    "growatt_sunspec_tcp", role="client"
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await modbus.register_modbus_device(var, config)

    cg.add(var.set_tcp_port(config[CONF_TCP_PORT]))
    cg.add(var.set_unit_id(config[CONF_UNIT_ID]))
    cg.add(var.set_rated_power_w(config[CONF_RATED_POWER_W]))
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_serial(config[CONF_SERIAL]))
    cg.add(var.set_holding_active_pct_reg(config[CONF_HOLDING_ACTIVE_PCT]))
    cg.add(var.set_min_rtu_gap_ms(config[CONF_MIN_RTU_GAP]))
    silence = config[CONF_FULL_POWER_AFTER_DER_SILENCE]
    if silence == "never":
        cg.add(var.set_der_idle_revert_ms(0))
    else:
        cg.add(var.set_der_idle_revert_ms(silence))

    if CONF_AC_VOLTAGE in config:
        cg.add(var.set_ac_voltage_sensor(await cg.get_variable(config[CONF_AC_VOLTAGE])))
    if CONF_AC_CURRENT in config:
        cg.add(var.set_ac_current_sensor(await cg.get_variable(config[CONF_AC_CURRENT])))
    if CONF_AC_POWER in config:
        cg.add(var.set_ac_power_sensor(await cg.get_variable(config[CONF_AC_POWER])))
    if CONF_FREQUENCY in config:
        cg.add(var.set_frequency_sensor(await cg.get_variable(config[CONF_FREQUENCY])))
    if CONF_ENERGY_KWH in config:
        cg.add(var.set_energy_kwh_sensor(await cg.get_variable(config[CONF_ENERGY_KWH])))
    if CONF_PV_POWER in config:
        cg.add(var.set_pv_power_sensor(await cg.get_variable(config[CONF_PV_POWER])))
    if CONF_CABINET_TEMP in config:
        cg.add(var.set_cabinet_temp_sensor(await cg.get_variable(config[CONF_CABINET_TEMP])))
