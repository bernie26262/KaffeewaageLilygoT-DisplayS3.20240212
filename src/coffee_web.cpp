#include "coffee_web.h"
#include <ArduinoJson.h>

static AsyncWebSocket ws("/ws");

void coffeeWebBegin(AsyncWebServer& server)
{
  ws.onEvent([](AsyncWebSocket *server,
                AsyncWebSocketClient *client,
                AwsEventType type,
                void *arg,
                uint8_t *data,
                size_t len)
  {
    if (type == WS_EVT_CONNECT) {
      Serial.println("[WS] Client connected");
    }
  });

  server.addHandler(&ws);
}

void coffeeWebLoop()
{
  ws.cleanupClients();
}

static String buildStateJson(const AppState& s)
{
  StaticJsonDocument<1024> doc;

  doc["type"] = "state";

  doc["weight"]["actual_g"] = s.weight.actual_g;
  doc["weight"]["set_g"] = s.weight.set_g;
  doc["weight"]["stable"] = s.weight.stable;

  doc["status"]["mode"] = s.status.mode;
  doc["status"]["save_ready"] = s.status.save_ready;

  doc["stopwatch"]["ms"] = s.stopwatch.ms;
  doc["stopwatch"]["running"] = s.stopwatch.running;

  doc["selection"]["siebtraeger"] = s.selection.siebtraeger;
  doc["selection"]["gefaess"] = s.selection.gefaess;
  doc["selection"]["autodetect"] = s.selection.autodetect;

  doc["stats"]["ground"]["total_g"] = s.stats.ground.total_g;
  doc["stats"]["ground"]["since_grinder_clean_g"] = s.stats.ground.since_grinder_clean_g;
  doc["stats"]["ground"]["since_machine_clean_g"] = s.stats.ground.since_machine_clean_g;
  doc["stats"]["ground"]["since_filter_change_g"] = s.stats.ground.since_filter_change_g;

  doc["stats"]["shots"]["total"] = s.stats.shots.total;
  doc["stats"]["shots"]["since_grinder_clean"] = s.stats.shots.since_grinder_clean;
  doc["stats"]["shots"]["since_machine_clean"] = s.stats.shots.since_machine_clean;
  doc["stats"]["shots"]["since_filter_change"] = s.stats.shots.since_filter_change;

  doc["time"]["valid"] = s.time.valid;
  doc["time"]["epoch"] = s.time.epoch;

  doc["system"]["wifi"] = s.system.wifi_connected;
  doc["system"]["ip"] = s.system.ip;
  doc["system"]["uptime_ms"] = s.system.uptime_ms;

  String out;
  serializeJson(doc, out);
  return out;
}

void coffeeWebBroadcastState(const AppState& state)
{
  ws.textAll(buildStateJson(state));
}