import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.const import CONF_ID

# Serves a custom, tablet-styled single-page dashboard at "/", driving the
# standard ESPHome web_server REST API + /events SSE stream. AUTO_LOADs
# web_server_base; requires the web_server component for the REST/SSE API.
AUTO_LOAD = ["web_server_base"]
DEPENDENCIES = ["web_server"]
CODEOWNERS = []

CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

web_dashboard_ns = cg.esphome_ns.namespace("web_dashboard")
WebDashboard = web_dashboard_ns.class_("WebDashboard", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WebDashboard),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base)
    await cg.register_component(var, config)
