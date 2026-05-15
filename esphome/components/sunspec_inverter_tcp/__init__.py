"""SunSpec Modbus TCP server driven by ESPHome sensors (read-only DER on model 123)."""

from esphome.components import sensor
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL

CODEOWNERS = ["@blaet"]
DEPENDENCIES = ["sensor"]
AUTO_LOAD = ["sensor"]

sunspec_inverter_tcp_ns = cg.esphome_ns.namespace("sunspec_inverter_tcp")
SunspecInverterTcp = sunspec_inverter_tcp_ns.class_("SunspecInverterTcp", cg.Component)

CONF_TCP_PORT = "tcp_port"
CONF_UNIT_ID = "unit_id"
CONF_RATED_POWER_W = "rated_power_w"
CONF_MANUFACTURER = "manufacturer"
CONF_SERIAL = "serial"

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
            cv.GenerateID(): cv.declare_id(SunspecInverterTcp),
            cv.Optional(CONF_TCP_PORT, default=502): cv.port,
            cv.Optional(CONF_UNIT_ID, default=126): cv.int_range(min=1, max=247),
            cv.Optional(CONF_RATED_POWER_W, default=3000): cv.int_range(min=100, max=65535),
            cv.Optional(CONF_MANUFACTURER, default="Generic"): cv.string,
            cv.Optional(CONF_MODEL, default="Inverter"): cv.string,
            cv.Optional(CONF_SERIAL, default="SUNSPEC-TCP"): cv.string,
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
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_tcp_port(config[CONF_TCP_PORT]))
    cg.add(var.set_unit_id(config[CONF_UNIT_ID]))
    cg.add(var.set_rated_power_w(config[CONF_RATED_POWER_W]))
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_serial(config[CONF_SERIAL]))

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
