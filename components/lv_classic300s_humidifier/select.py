import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select

from . import (
    CONF_LV_CLASSIC300S_HUMIDIFIER_ID,
    LVClassic300SHumidifier,
    lv_classic300s_humidifier_ns,
)

DEPENDENCIES = ["lv_classic300s_humidifier"]

LVClassic300SModeSelect = lv_classic300s_humidifier_ns.class_(
    "LVClassic300SModeSelect", select.Select, cg.Component
)

CONF_MODE = "mode"

MODE_OPTIONS = ["auto", "manual", "sleep"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_LV_CLASSIC300S_HUMIDIFIER_ID): cv.use_id(LVClassic300SHumidifier),
        cv.Required(CONF_MODE): select.select_schema(
            LVClassic300SModeSelect, icon="mdi:tune"
        ).extend(cv.COMPONENT_SCHEMA),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LV_CLASSIC300S_HUMIDIFIER_ID])

    conf = config[CONF_MODE]
    var = await select.new_select(conf, options=MODE_OPTIONS)
    await cg.register_component(var, conf)
    cg.add(var.set_parent(parent))
