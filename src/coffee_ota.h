#pragma once

#include <ESPAsyncWebServer.h>

void coffeeOtaBegin(AsyncWebServer& server);
void coffeeOtaLoop();
void coffeeOtaRequestReboot();
