#pragma once

#include <ESPAsyncWebServer.h>

void coffeeOtaBegin(AsyncWebServer& server);
void coffeeOtaLoop();
void coffeeOtaRequestReboot();
// B4c.1: one-way, cross-task inhibit from accepted OTA start or reboot request.
// This remains latched until the ESP restarts; not an SSR control function.
bool coffeeOtaMotorInhibitRequested();
