import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.components.light import LightType

from . import (
    CONF_LV_CLASSIC300S_HUMIDIFIER_ID,
    LVClassic300SHumidifier,
    lv_classic300s_humidifier_ns,
)

DEPENDENCIES = ["lv_classic300s_humidifier"]

# Night Light as a brightness-only `light` entity (on/off + 0-100% dimming),
# instead of a plain `number`. Backed by the same set_night_light()/
# night_light_level() pair on the hub as before -- this just gives it a
# proper light card in HA (and lets it host effects, see
# docs/night_light_notifications.md and common_entities.yaml for the
# "flash on error" / "breathe while misting" automations built on top).
LVClassic300SLight = lv_classic300s_humidifier_ns.class_(
    "LVClassic300SLight", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.light_schema(LVClassic300SLight, LightType.BRIGHTNESS_ONLY).extend(
    {
        cv.Required(CONF_LV_CLASSIC300S_HUMIDIFIER_ID): cv.use_id(LVClassic300SHumidifier),
    }
)


async def to_code(config):
    var = await light.new_light(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_LV_CLASSIC300S_HUMIDIFIER_ID])
    cg.add(var.set_parent(parent))
