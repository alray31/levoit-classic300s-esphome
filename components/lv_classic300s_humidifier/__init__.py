import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import uart, sensor, binary_sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
    STATE_CLASS_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
)

CODEOWNERS = ["@alray31"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]
MULTI_CONF = True

lv_classic300s_humidifier_ns = cg.esphome_ns.namespace("lv_classic300s_humidifier")
LVClassic300SHumidifier = lv_classic300s_humidifier_ns.class_(
    "LVClassic300SHumidifier", cg.Component, uart.UARTDevice
)

CONF_LV_CLASSIC300S_HUMIDIFIER_ID = "lv_classic300s_humidifier_id"

CONF_STATUS_INTERVAL = "status_interval"

# sensor:
CONF_CURRENT_HUMIDITY = "current_humidity"
CONF_TARGET_HUMIDITY = "target_humidity"
CONF_NIGHT_LIGHT = "night_light"
CONF_OUTPUT_LEVEL = "output_level"

# binary_sensor:
CONF_POWER = "power"
CONF_TANK_REMOVED = "tank_removed"
CONF_WATER_EMPTY = "water_empty"
CONF_MIST_ACTIVE = "mist_active"
CONF_DISPLAY = "display"
CONF_TARGET_STOP_ACTIVE = "target_stop_active"

# text_sensor:
CONF_MODE = "mode"
CONF_ERROR = "error"
CONF_LAST_STATUS_FRAME = "last_status_frame"

# automation triggers:
CONF_ON_POWER_BUTTON_HOLD_5S = "on_power_button_hold_5s"
CONF_ON_POWER_BUTTON_HOLD_15S = "on_power_button_hold_15s"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LVClassic300SHumidifier),
            cv.Optional(CONF_STATUS_INTERVAL, default="15s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CURRENT_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TARGET_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_NIGHT_LIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon="mdi:led-outline",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_OUTPUT_LEVEL): sensor.sensor_schema(
                icon="mdi:water",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_POWER): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_POWER,
            ),
            cv.Optional(CONF_TANK_REMOVED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_WATER_EMPTY): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
            ),
            cv.Optional(CONF_MIST_ACTIVE): binary_sensor.binary_sensor_schema(
                icon="mdi:water-outline",
            ),
            cv.Optional(CONF_DISPLAY): binary_sensor.binary_sensor_schema(
                icon="mdi:monitor",
            ),
            cv.Optional(CONF_TARGET_STOP_ACTIVE): binary_sensor.binary_sensor_schema(
                icon="mdi:target",
            ),
            cv.Optional(CONF_MODE): text_sensor.text_sensor_schema(
                icon="mdi:cog",
            ),
            cv.Optional(CONF_ERROR): text_sensor.text_sensor_schema(
                icon="mdi:alert-circle-outline",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_LAST_STATUS_FRAME): text_sensor.text_sensor_schema(
                icon="mdi:code-braces",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_ON_POWER_BUTTON_HOLD_5S): automation.validate_automation(
                single=True
            ),
            cv.Optional(CONF_ON_POWER_BUTTON_HOLD_15S): automation.validate_automation(
                single=True
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add(var.set_status_interval(config[CONF_STATUS_INTERVAL]))

    if CONF_CURRENT_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_CURRENT_HUMIDITY])
        cg.add(var.set_current_humidity_sensor(sens))
    if CONF_TARGET_HUMIDITY in config:
        sens = await sensor.new_sensor(config[CONF_TARGET_HUMIDITY])
        cg.add(var.set_target_humidity_sensor(sens))
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_NIGHT_LIGHT in config:
        sens = await sensor.new_sensor(config[CONF_NIGHT_LIGHT])
        cg.add(var.set_night_light_sensor(sens))
    if CONF_OUTPUT_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_OUTPUT_LEVEL])
        cg.add(var.set_output_level_sensor(sens))

    if CONF_POWER in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_POWER])
        cg.add(var.set_power_binary_sensor(sens))
    if CONF_TANK_REMOVED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TANK_REMOVED])
        cg.add(var.set_tank_removed_sensor(sens))
    if CONF_WATER_EMPTY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_WATER_EMPTY])
        cg.add(var.set_water_empty_sensor(sens))
    if CONF_MIST_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_MIST_ACTIVE])
        cg.add(var.set_mist_active_sensor(sens))
    if CONF_DISPLAY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DISPLAY])
        cg.add(var.set_display_binary_sensor(sens))
    if CONF_TARGET_STOP_ACTIVE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TARGET_STOP_ACTIVE])
        cg.add(var.set_target_stop_active_sensor(sens))

    if CONF_MODE in config:
        sens = await text_sensor.new_text_sensor(config[CONF_MODE])
        cg.add(var.set_mode_text_sensor(sens))
    if CONF_ERROR in config:
        sens = await text_sensor.new_text_sensor(config[CONF_ERROR])
        cg.add(var.set_error_text_sensor(sens))
    if CONF_LAST_STATUS_FRAME in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_STATUS_FRAME])
        cg.add(var.set_last_status_frame_text_sensor(sens))

    if CONF_ON_POWER_BUTTON_HOLD_5S in config:
        await automation.build_automation(
            var.get_power_hold_5s_trigger(), [], config[CONF_ON_POWER_BUTTON_HOLD_5S]
        )
    if CONF_ON_POWER_BUTTON_HOLD_15S in config:
        await automation.build_automation(
            var.get_power_hold_15s_trigger(), [], config[CONF_ON_POWER_BUTTON_HOLD_15S]
        )
