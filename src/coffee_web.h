#pragma once

#include <ESPAsyncWebServer.h>
#include "app_state.h"

void coffeeWebBegin(AsyncWebServer& server);
void coffeeWebLoop();
void coffeeWebBroadcastState(const AppState& state);