import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch

from . import (
    CONF_LV_CLASSIC300S_HUMIDIFIER_ID,
    LVClassic300SHumidifier,
    lv_classic300s_humidifier_ns,
)

DEPENDENCIES = ["lv_classic300s_humidifier"]

LVClassic300SSwitch = lv_classic300s_humidifier_ns.class_(
    "LVClassic300SSwitch", switch.Switch, cg.Component
)
SwitchKind = lv_classic300s_humidifier_ns.enum("SwitchKind", is_class=True)

CONF_POWER = "power"
CONF_DISPLAY = "display"
CONF_STOP_AT_TARGET = "stop_at_target"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_LV_CLASSIC300S_HUMIDIFIER_ID): cv.use_id(LVClassic300SHumidifier),
        cv.Optional(CONF_POWER): switch.switch_schema(
            LVClassic300SSwitch, icon="mdi:power"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_DISPLAY): switch.switch_schema(
            LVClassic300SSwitch, icon="mdi:monitor"
        ).extend(cv.COMPONENT_SCHEMA),
        cv.Optional(CONF_STOP_AT_TARGET): switch.switch_schema(
            LVClassic300SSwitch, icon="mdi:target"
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def _new_child_switch(conf, parent, kind):
    var = await switch.new_switch(conf)
    await cg.register_component(var, conf)
    cg.add(var.set_parent(parent))
    cg.add(var.set_kind(kind))


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV_CLASSIC300S_HUMIDIFIER_ID])

    if CONF_POWER in config:
        await _new_child_switch(config[CONF_POWER], parent, SwitchKind.POWER)
    if CONF_DISPLAY in config:
        await _new_child_switch(config[CONF_DISPLAY], parent, SwitchKind.DISPLAY)
    if CONF_STOP_AT_TARGET in config:
        await _new_child_switch(config[CONF_STOP_AT_TARGET], parent, SwitchKind.STOP_AT_TARGET)
