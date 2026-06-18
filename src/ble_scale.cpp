#include "ble_scale.h"

#if ENABLE_BLE_SCALE

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

namespace {
constexpr const char* kModeName = "WeighMyBru";
constexpr const char* kServiceUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kWeightUuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kBeanConquerorWeightUuid = "6E400004-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kCommandUuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr uint32_t kNotifyIntervalMs = 200UL; // 5 Hz
constexpr uint8_t kLogLineCount = 12;
constexpr size_t kLogLineLength = 96;

#ifndef BLE_SCALE_VERBOSE_LOGS
#define BLE_SCALE_VERBOSE_LOGS 0
#endif

#ifndef BLE_SCALE_LOW_POWER
#define BLE_SCALE_LOW_POWER 1
#endif

#if BLE_SCALE_VERBOSE_LOGS
#define BLE_VERBOSE_PRINTF(...) Serial.printf(__VA_ARGS__)
#define BLE_VERBOSE_PRINTLN(message) Serial.println(message)
#else
#define BLE_VERBOSE_PRINTF(...) do {} while (0)
#define BLE_VERBOSE_PRINTLN(message) do {} while (0)
#endif

BLEServer* g_server = nullptr;
BLECharacteristic* g_weightCharacteristic = nullptr;
BLECharacteristic* g_beanConquerorWeightCharacteristic = nullptr;
BLECharacteristic* g_commandCharacteristic = nullptr;
BLE2902* g_weightDescriptor = nullptr;
bool g_started = false;
bool g_connected = false;
bool g_advertising = false;
uint32_t g_lastNotifyMs = 0;
uint32_t g_lastNotifyAgeReferenceMs = 0;
uint32_t g_packetsSent = 0;
uint32_t g_commandsReceived = 0;
uint32_t g_rateWindowStartMs = 0;
uint32_t g_rateWindowPackets = 0;
float g_notifyHz = 0.0f;
float g_lastWeightG = 0.0f;
char g_lastCommand[16] = "---";

portMUX_TYPE g_commandMux = portMUX_INITIALIZER_UNLOCKED;
CoffeeBleScaleCommand g_pendingCommand = CoffeeBleScaleCommand::None;

portMUX_TYPE g_logMux = portMUX_INITIALIZER_UNLOCKED;
char g_logLines[kLogLineCount][kLogLineLength] = {{0}};
uint8_t g_logStart = 0;
uint8_t g_logCount = 0;
uint32_t g_logSequence = 0;

void appendLogFormatted(const char* format, ...)
{
  if (!format) {
    return;
  }

  char message[kLogLineLength - 16];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof(message), format, args);
  va_end(args);

  const uint32_t seconds = millis() / 1000UL;
  char line[kLogLineLength];
  snprintf(line, sizeof(line), "%02lu:%02lu  %s",
           static_cast<unsigned long>((seconds / 60UL) % 100UL),
           static_cast<unsigned long>(seconds % 60UL),
           message);

  portENTER_CRITICAL(&g_logMux);
  uint8_t index;
  if (g_logCount < kLogLineCount) {
    index = (g_logStart + g_logCount) % kLogLineCount;
    ++g_logCount;
  } else {
    index = g_logStart;
    g_logStart = (g_logStart + 1) % kLogLineCount;
  }
  strlcpy(g_logLines[index], line, sizeof(g_logLines[index]));
  ++g_logSequence;
  portEXIT_CRITICAL(&g_logMux);
}

CoffeeBleScaleCommand decodeCommand(uint8_t value)
{
  switch (value) {
    case 0x01: return CoffeeBleScaleCommand::Tare;
    case 0x02: return CoffeeBleScaleCommand::TimerStart;
    case 0x03: return CoffeeBleScaleCommand::TimerStop;
    case 0x04: return CoffeeBleScaleCommand::TimerReset;
    default: return CoffeeBleScaleCommand::None;
  }
}

CoffeeBleScaleCommand decodeCommandPayload(const std::string& value)
{
  if (value.empty()) {
    return CoffeeBleScaleCommand::None;
  }

  const uint8_t* data = reinterpret_cast<const uint8_t*>(value.data());
  const size_t length = value.length();

  if (length == 1) {
    return decodeCommand(data[0]);
  }

  // WeighMyBru/Gaggiuino SYSTEM packet:
  // [0] product 0x02/0x03, [1] type 0x0A, [2] command, [3] trigger 0x01.
  if (length >= 4 && (data[0] == 0x02 || data[0] == 0x03) && data[1] == 0x0A && data[3] == 0x01) {
    return decodeCommand(data[2]);
  }

  return CoffeeBleScaleCommand::None;
}

void queueCommand(CoffeeBleScaleCommand command)
{
  if (command == CoffeeBleScaleCommand::None) {
    return;
  }

  portENTER_CRITICAL(&g_commandMux);
  g_pendingCommand = command;
  portEXIT_CRITICAL(&g_commandMux);

  ++g_commandsReceived;
  strlcpy(g_lastCommand, coffeeBleScaleCommandName(command), sizeof(g_lastCommand));
  BLE_VERBOSE_PRINTF("[BLE] command received: %s\n", g_lastCommand);
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override
  {
    const bool wasConnected = g_connected;
    g_connected = true;
    g_advertising = false;
    if (!wasConnected) {
      appendLogFormatted("Maschine verbunden");
      BLE_VERBOSE_PRINTLN("[BLE] client connected");
    }
  }

  void onDisconnect(BLEServer* server) override
  {
    const bool wasConnected = g_connected;
    g_connected = false;
    g_lastNotifyMs = 0;
    if (wasConnected) {
      appendLogFormatted("Verbindung getrennt - Advertising neu");
      BLE_VERBOSE_PRINTLN("[BLE] client disconnected, restart advertising");
    }
    delay(50);
    server->startAdvertising();
    g_advertising = true;
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override
  {
    const std::string value = characteristic->getValue();
    if (value.empty()) {
      return;
    }
    queueCommand(decodeCommandPayload(value));
  }
};

void buildWeighMyBruWeightPacket(float weightG, uint8_t packet[20])
{
  memset(packet, 0, 20);
  packet[0] = 0x03;
  packet[1] = 0x0B;

  const bool negative = isfinite(weightG) && weightG < 0.0f;
  const float absWeight = isfinite(weightG) ? fabsf(weightG) : 0.0f;
  uint32_t scaled = static_cast<uint32_t>(lroundf(absWeight * 100.0f));
  if (scaled > 0xFFFFFFUL) {
    scaled = 0xFFFFFFUL;
  }

  packet[6] = negative ? '-' : '+';
  packet[7] = static_cast<uint8_t>((scaled >> 16) & 0xFF);
  packet[8] = static_cast<uint8_t>((scaled >> 8) & 0xFF);
  packet[9] = static_cast<uint8_t>(scaled & 0xFF);

  uint8_t checksum = 0;
  for (uint8_t i = 0; i < 19; ++i) {
    checksum ^= packet[i];
  }
  packet[19] = checksum;
}
} // namespace

void coffeeBleScaleAppendLog(const char* message)
{
  appendLogFormatted("%s", message ? message : "---");
}

void coffeeBleScaleCopyLog(char* destination, size_t destinationSize)
{
  if (!destination || destinationSize == 0) {
    return;
  }

  destination[0] = '\0';
  size_t used = 0;

  portENTER_CRITICAL(&g_logMux);
  for (uint8_t i = 0; i < g_logCount; ++i) {
    const uint8_t index = (g_logStart + i) % kLogLineCount;
    const char* line = g_logLines[index];
    const size_t lineLength = strnlen(line, kLogLineLength);
    const size_t required = lineLength + (i > 0 ? 1U : 0U);
    if (used + required + 1 > destinationSize) {
      break;
    }
    if (i > 0) {
      destination[used++] = '\n';
    }
    memcpy(destination + used, line, lineLength);
    used += lineLength;
    destination[used] = '\0';
  }
  portEXIT_CRITICAL(&g_logMux);
}

void coffeeBleScaleBegin(const char* deviceName)
{
  if (g_started) {
    BLE_VERBOSE_PRINTLN("[BLE] start skipped: already active");
    return;
  }

  const char* name = (deviceName && deviceName[0]) ? deviceName : "WeighMyBru";

  // Wichtig fuer ESP32-S3: WiFi/BLE coexistence vor BLEDevice::init beruhigen.
  WiFi.setSleep(true);
  delay(50);

  BLEDevice::init(name);
#if BLE_SCALE_LOW_POWER
  BLEDevice::setPower(ESP_PWR_LVL_N12);
#else
  BLEDevice::setPower(ESP_PWR_LVL_P3);
#endif

  g_server = BLEDevice::createServer();
  g_server->setCallbacks(new ServerCallbacks());

  BLEService* service = g_server->createService(kServiceUuid);

  g_weightCharacteristic = service->createCharacteristic(
      kWeightUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  g_weightDescriptor = new BLE2902();
  g_weightCharacteristic->addDescriptor(g_weightDescriptor);

  g_beanConquerorWeightCharacteristic = service->createCharacteristic(
      kBeanConquerorWeightUuid,
      BLECharacteristic::PROPERTY_READ);

  g_commandCharacteristic = service->createCharacteristic(
      kCommandUuid,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  g_commandCharacteristic->setCallbacks(new CommandCallbacks());

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  g_started = true;
  g_advertising = true;
  g_rateWindowStartMs = millis();
  appendLogFormatted("%s aktiv - Advertising läuft", name);
  BLE_VERBOSE_PRINTF("[BLE] %s started as '%s'\n", kModeName, name);
}

void coffeeBleScaleTick(uint32_t nowMs, float weightG, bool stable)
{
  (void)stable;
  if (!g_started || !g_connected || !g_weightCharacteristic) {
    return;
  }
  if (nowMs - g_lastNotifyMs < kNotifyIntervalMs) {
    return;
  }

  uint8_t packet[20];
  buildWeighMyBruWeightPacket(weightG, packet);
  g_weightCharacteristic->setValue(packet, sizeof(packet));

  if (g_weightDescriptor && g_weightDescriptor->getNotifications()) {
    g_weightCharacteristic->notify();
  }

  if (g_beanConquerorWeightCharacteristic) {
    union {
      float value;
      uint8_t bytes[4];
    } simpleWeight;
    simpleWeight.value = weightG;
    g_beanConquerorWeightCharacteristic->setValue(simpleWeight.bytes, sizeof(simpleWeight.bytes));
  }

  g_lastNotifyMs = nowMs;
  g_lastNotifyAgeReferenceMs = nowMs;
  g_lastWeightG = weightG;
  ++g_packetsSent;
  ++g_rateWindowPackets;

  const uint32_t windowMs = nowMs - g_rateWindowStartMs;
  if (windowMs >= 2000UL) {
    g_notifyHz = (static_cast<float>(g_rateWindowPackets) * 1000.0f) / static_cast<float>(windowMs);
    g_rateWindowPackets = 0;
    g_rateWindowStartMs = nowMs;
  }
}

bool coffeeBleScalePopCommand(CoffeeBleScaleCommand& command)
{
  command = CoffeeBleScaleCommand::None;
  portENTER_CRITICAL(&g_commandMux);
  if (g_pendingCommand != CoffeeBleScaleCommand::None) {
    command = g_pendingCommand;
    g_pendingCommand = CoffeeBleScaleCommand::None;
  }
  portEXIT_CRITICAL(&g_commandMux);
  return command != CoffeeBleScaleCommand::None;
}

CoffeeBleScaleStatus coffeeBleScaleStatus()
{
  CoffeeBleScaleStatus status;
  status.enabled = true;
  status.started = g_started;
  status.advertising = g_advertising;
  status.connected = g_connected;
  status.mode = kModeName;
  status.last_command = g_lastCommand;
  status.last_weight_g = g_lastWeightG;
  status.notify_hz = g_notifyHz;
  status.packets_sent = g_packetsSent;
  status.commands_received = g_commandsReceived;
  status.last_notify_age_ms = g_lastNotifyAgeReferenceMs > 0 ? millis() - g_lastNotifyAgeReferenceMs : 0;
  status.log_sequence = g_logSequence;
  return status;
}

const char* coffeeBleScaleCommandName(CoffeeBleScaleCommand command)
{
  switch (command) {
    case CoffeeBleScaleCommand::Tare: return "TARE";
    case CoffeeBleScaleCommand::TimerStart: return "START";
    case CoffeeBleScaleCommand::TimerStop: return "STOP";
    case CoffeeBleScaleCommand::TimerReset: return "RESET";
    case CoffeeBleScaleCommand::None:
    default: return "---";
  }
}

#else

void coffeeBleScaleBegin(const char*) {}
void coffeeBleScaleTick(uint32_t, float, bool) {}
bool coffeeBleScalePopCommand(CoffeeBleScaleCommand& command)
{
  command = CoffeeBleScaleCommand::None;
  return false;
}
CoffeeBleScaleStatus coffeeBleScaleStatus()
{
  return CoffeeBleScaleStatus{};
}
const char* coffeeBleScaleCommandName(CoffeeBleScaleCommand command)
{
  switch (command) {
    case CoffeeBleScaleCommand::Tare: return "TARE";
    case CoffeeBleScaleCommand::TimerStart: return "START";
    case CoffeeBleScaleCommand::TimerStop: return "STOP";
    case CoffeeBleScaleCommand::TimerReset: return "RESET";
    case CoffeeBleScaleCommand::None:
    default: return "---";
  }
}
void coffeeBleScaleAppendLog(const char*) {}
void coffeeBleScaleCopyLog(char* destination, size_t destinationSize)
{
  if (destination && destinationSize > 0) {
    destination[0] = '\0';
  }
}

#endif
