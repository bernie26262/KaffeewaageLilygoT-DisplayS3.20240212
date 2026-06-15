#include "ble_scale.h"

#if ENABLE_BLE_SCALE

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <math.h>
#include <esp_gatts_api.h>

namespace {
constexpr const char* kModeName = "WeighMyBru";
constexpr const char* kServiceUuid = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kWeightUuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kBeanConquerorWeightUuid = "6E400004-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kCommandUuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr uint32_t kNotifyIntervalMs = 200UL; // 5 Hz: Stabilitaetstest gegen BLE-GATT-Congestion
constexpr uint32_t kNotifyLogIntervalMs = 2000UL; // nur bei BLE_SCALE_VERBOSE_LOGS
// Gaggiuino abonniert nur 6E400002. Notifications auf 6E400004 erzeugen ohne Subscriber
// ESP-IDF-Fehlerlogs (esp_ble_gatts_send_notify rc=-1). Der Wert bleibt lesbar, wird aber nicht gepusht.
#ifndef BLE_SCALE_NOTIFY_BEANCONQUEROR
#define BLE_SCALE_NOTIFY_BEANCONQUEROR 0
#endif
constexpr size_t kCommandHexLogMaxBytes = 32;

#ifndef BLE_SCALE_LOW_POWER
#define BLE_SCALE_LOW_POWER 1
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
uint32_t g_lastNotifyLogMs = 0;
uint32_t g_packetsSent = 0;
uint32_t g_commandsReceived = 0;
uint32_t g_rateWindowStartMs = 0;
uint32_t g_rateWindowPackets = 0;
float g_notifyHz = 0.0f;
float g_lastWeightG = 0.0f;
char g_lastCommand[16] = "---";

portMUX_TYPE g_commandMux = portMUX_INITIALIZER_UNLOCKED;
CoffeeBleScaleCommand g_pendingCommand = CoffeeBleScaleCommand::None;

void printHexBytes(const uint8_t* data, size_t length, size_t maxBytes = kCommandHexLogMaxBytes)
{
  const size_t bytesToPrint = length < maxBytes ? length : maxBytes;
  for (size_t i = 0; i < bytesToPrint; ++i) {
    Serial.printf("%s%02X", i == 0 ? "" : " ", static_cast<unsigned>(data[i]));
  }
  if (length > bytesToPrint) {
    Serial.printf(" ...(+%u)", static_cast<unsigned>(length - bytesToPrint));
  }
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

  // Einfacher BeanConqueror-/Debug-Fallback: nur Command-Byte.
  if (length == 1) {
    return decodeCommand(data[0]);
  }

  // WeighMyBru/Gaggiuino-Client sendet SYSTEM-Pakete:
  // [0] product 0x02/0x03, [1] type 0x0A, [2] command, [3] trigger=0x01, ...
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
  Serial.printf("[BLE] command received: %s\n", g_lastCommand);
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*)
  {
    const bool wasConnected = g_connected;
    g_connected = true;
    g_advertising = false;
    if (!wasConnected) {
      Serial.println("[BLE] client connected");
    }
  }

  void onDisconnect(BLEServer* server)
  {
    const bool wasConnected = g_connected;
    g_connected = false;
    g_lastNotifyMs = 0;
    if (wasConnected) {
      Serial.println("[BLE] client disconnected, restart advertising");
    }
    delay(50);
    server->startAdvertising();
    g_advertising = true;
  }

  void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t*)
  {
    onConnect(server);
  }

  void onDisconnect(BLEServer* server, esp_ble_gatts_cb_param_t*)
  {
    onDisconnect(server);
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic)
  {
    const auto value = characteristic->getValue();
    if (value.length() == 0) {
      return;
    }
    const CoffeeBleScaleCommand command = decodeCommandPayload(value);
#if BLE_SCALE_VERBOSE_LOGS
    Serial.printf("[BLE] command write: len=%u data=", static_cast<unsigned>(value.length()));
    printHexBytes(reinterpret_cast<const uint8_t*>(value.data()), value.length());
    Serial.printf(" decoded=%s\n", coffeeBleScaleCommandName(command));
#endif
    queueCommand(command);
  }

  void onWrite(BLECharacteristic* characteristic, esp_ble_gatts_cb_param_t*)
  {
    onWrite(characteristic);
  }
};

void buildWeighMyBruWeightPacket(float weightG, uint8_t packet[20])
{
  memset(packet, 0, 20);
  packet[0] = 0x03; // product number, wie WeighMyBru
  packet[1] = 0x0B; // message type: weight

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

void coffeeBleScaleBegin(const char* deviceName)
{
  if (g_started) {
    Serial.println("[BLE] begin skipped: already started");
    return;
  }

  const char* name = (deviceName && deviceName[0]) ? deviceName : "SingleDose-WeighMyBru";
#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.printf("[BLE] begin: name='%s', freeHeap=%lu, minHeap=%lu\n",
                name,
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()));
  Serial.println("[BLE] pre-step: enable WiFi modem sleep for BLE coexistence");
  Serial.flush();
#endif

  WiFi.setSleep(true);
  delay(50);

#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 1/8: BLEDevice::init");
  Serial.flush();
#endif
  BLEDevice::init(name);

#if BLE_SCALE_LOW_POWER
#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 2/8: setPower low ESP_PWR_LVL_N12");
  Serial.flush();
#endif
  BLEDevice::setPower(ESP_PWR_LVL_N12);
#else
#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 2/8: setPower normal ESP_PWR_LVL_P3");
  Serial.flush();
#endif
  BLEDevice::setPower(ESP_PWR_LVL_P3);
#endif

#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 3/8: createServer");
  Serial.flush();
#endif
  g_server = BLEDevice::createServer();
  g_server->setCallbacks(new ServerCallbacks());

#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 4/8: createService");
  Serial.flush();
#endif
  BLEService* service = g_server->createService(kServiceUuid);

#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 5/8: create characteristics");
  Serial.flush();
#endif
  // Gaggiuino/esp-arduino-ble-scales liest dieses Characteristic als WeighMyBru-Gewicht.
  g_weightCharacteristic = service->createCharacteristic(
      kWeightUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  g_weightDescriptor = new BLE2902();
  g_weightCharacteristic->addDescriptor(g_weightDescriptor);

  // WeighMyBru bietet zusaetzlich 6E400004 fuer BeanConqueror/simple-float an.
  // Gaggiuino braucht es nicht zwingend, aber es macht unsere GATT-Tabelle naeher am Original.
  g_beanConquerorWeightCharacteristic = service->createCharacteristic(
      kBeanConquerorWeightUuid,
      BLECharacteristic::PROPERTY_READ);

  g_commandCharacteristic = service->createCharacteristic(
      kCommandUuid,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  g_commandCharacteristic->setCallbacks(new CommandCallbacks());

#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 6/8: service->start");
  Serial.flush();
#endif
  service->start();

#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 7/8: configure advertising");
  Serial.flush();
#endif
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);

#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.println("[BLE] step 8/8: startAdvertising");
  Serial.flush();
#endif
  BLEDevice::startAdvertising();

  g_started = true;
  g_advertising = true;
  g_rateWindowStartMs = millis();
  Serial.printf("[BLE] %s started as '%s'\n", kModeName, name);
#if BLE_SCALE_STARTUP_DIAGNOSTICS
  Serial.printf("[BLE] startup heap: free=%lu, min=%lu\n",
                static_cast<unsigned long>(ESP.getFreeHeap()),
                static_cast<unsigned long>(ESP.getMinFreeHeap()));
#endif
  Serial.flush();
}

void coffeeBleScaleTick(uint32_t nowMs, float weightG, bool stable)
{
  (void)stable; // reserviert fuer spaetere Paket-/Statusvarianten
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
#if BLE_SCALE_VERBOSE_LOGS
  else if (nowMs - g_lastNotifyLogMs >= kNotifyLogIntervalMs) {
    g_lastNotifyLogMs = nowMs;
    Serial.println("[BLE] notify skipped: client has not enabled weight notifications");
  }
#endif

  if (g_beanConquerorWeightCharacteristic) {
    union {
      float value;
      uint8_t bytes[4];
    } simpleWeight;
    simpleWeight.value = weightG;
    g_beanConquerorWeightCharacteristic->setValue(simpleWeight.bytes, sizeof(simpleWeight.bytes));
#if BLE_SCALE_NOTIFY_BEANCONQUEROR
    g_beanConquerorWeightCharacteristic->notify();
#endif
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

#if BLE_SCALE_VERBOSE_LOGS
  if (nowMs - g_lastNotifyLogMs >= kNotifyLogIntervalMs) {
    g_lastNotifyLogMs = nowMs;
    Serial.printf("[BLE] notify weight=%.2fg stable=%u packets=%lu hz=%.1f packet=",
                  static_cast<double>(weightG),
                  stable ? 1U : 0U,
                  static_cast<unsigned long>(g_packetsSent),
                  static_cast<double>(g_notifyHz));
    printHexBytes(packet, sizeof(packet), sizeof(packet));
    Serial.println();
  }
#endif
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
  status.advertising = g_advertising;
  status.connected = g_connected;
  status.mode = kModeName;
  status.last_command = g_lastCommand;
  status.last_weight_g = g_lastWeightG;
  status.notify_hz = g_notifyHz;
  status.packets_sent = g_packetsSent;
  status.commands_received = g_commandsReceived;
  status.last_notify_age_ms = g_lastNotifyAgeReferenceMs > 0 ? millis() - g_lastNotifyAgeReferenceMs : 0;
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

#endif
