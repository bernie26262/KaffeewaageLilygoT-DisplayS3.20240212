#pragma once

#include <ESPAsyncWebServer.h>
#include "app_state.h"

typedef bool (*CoffeeWebCommandHandler)(const char* cmd);

void coffeeWebBegin(AsyncWebServer& server);
void coffeeWebHandleRoot(AsyncWebServerRequest* request);
void coffeeWebLoop();
void coffeeWebBroadcastState(const AppState& state);
bool coffeeWebHasClients();
void coffeeWebSetCommandHandler(CoffeeWebCommandHandler handler);
void coffeeWebSetPwaAssetsAvailable(bool available);