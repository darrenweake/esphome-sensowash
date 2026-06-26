#pragma once
#include "esphome/core/defines.h"
#ifdef USE_ESP32
#include "esphome/core/component.h"
#include "esphome/components/web_server_base/web_server_base.h"

namespace esphome {
namespace web_dashboard {

// AsyncWebHandler that serves the custom dashboard HTML at "/" (and /index.html).
// Registered ahead of the web_server component (higher setup priority) so it wins
// the root path while web_server keeps the REST API and /events SSE endpoints.
class DashboardHandler : public AsyncWebHandler {
 public:
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;
  bool isRequestHandlerTrivial() const override { return true; }
};

class WebDashboard : public Component {
 public:
  explicit WebDashboard(web_server_base::WebServerBase *base) : base_(base) {}
  void setup() override;
  void dump_config() override;
  // Run before web_server (WIFI - 1) so our root handler is registered first.
  float get_setup_priority() const override { return setup_priority::WIFI; }

 protected:
  web_server_base::WebServerBase *base_;
};

}  // namespace web_dashboard
}  // namespace esphome
#endif  // USE_ESP32
