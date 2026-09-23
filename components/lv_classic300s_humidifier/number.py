import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import UNIT_PERCENT

from . import (
    CONF_LV_CLASSIC300S_HUMIDIFIER_ID,
    LVClassic300SHumidifier,
    lv_classic300s_humidifier_ns,
)

DEPENDENCIES = ["lv_classic300s_humidifier"]

LVClassic300SNumber = lv_classic300s_humidifier_ns.class_(
    "LVClassic300SNumber", number.Number, cg.Component
)
NumberKind = lv_classic300s_humidifier_ns.enum("NumberKind", is_class=True)

CONF_MANUAL_LEVEL = "manual_level"
CONF_AUTO_TARGET_HUMIDITY = "auto_target_humidity"
CONF_SLEEP_TARGET_HUMIDITY = "sleep_target_humidity"

# The MCU protocol accepts humidity targets outside this range too (see
# docs/A5_UART_protocol.md), but this covers what the stock app exposes.
# Widen min/max in your own YAML copy if you find your unit accepts more.
HUMIDITY_NUMBER_SCHEMA = number.number_schema(
    LVClassic300SNumber,
    unit_of_measurement=UNIT_PERCENT,
    icon="mdi:water-percent",
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_LV_CLASSIC300S_HUMIDIFIER_ID): cv.use_id(LVClassic300SHumidifier),
        cv.Optional(CONF_MANUAL_LEVEL): number.number_schema(
            LVClassic300SNumber, icon="mdi:water"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_AUTO_TARGET_HUMIDITY): HUMIDITY_NUMBER_SCHEMA,
        cv.Optional(CONF_SLEEP_TARGET_HUMIDITY): HUMIDITY_NUMBER_SCHEMA,
    }
)


async def _new_child_number(conf, parent, kind, min_value, max_value, step):
    var = await number.new_number(conf, min_value=min_value, max_value=max_value, step=step)
    await cg.register_component(var, conf)
    cg.add(var.set_parent(parent))
    cg.add(var.set_kind(kind))


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV_CLASSIC300S_HUMIDIFIER_ID])

    if CONF_MANUAL_LEVEL in config:
        await _new_child_number(
            config[CONF_MANUAL_LEVEL], parent, NumberKind.MANUAL_LEVEL, 1, 9, 1
        )
    if CONF_AUTO_TARGET_HUMIDITY in config:
        await _new_child_number(
            config[CONF_AUTO_TARGET_HUMIDITY], parent, NumberKind.AUTO_TARGET_HUMIDITY, 30, 80, 1
        )
    if CONF_SLEEP_TARGET_HUMIDITY in config:
        await _new_child_number(
            config[CONF_SLEEP_TARGET_HUMIDITY], parent, NumberKind.SLEEP_TARGET_HUMIDITY, 30, 80, 1
        )
