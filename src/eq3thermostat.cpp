#include <Arduino.h>
#include <map>
#include <list>

#include "eq3thermostat.h"

static BLEUUID serviceUUID("3e135142-654f-9090-134a-a6ff5bb77046");
static BLEUUID notifyUUID("d0e8434d-cd29-0996-af41-6c90f4e0eb2a");
static BLEUUID commandUUID("3fa4585a-ce4a-3bad-db4b-b8df8179ea09");

// BLE notify callbacks don't have a pointer back to the thermostat,
// so we need to maintain this mapping ourselves :(
static std::map<BLERemoteCharacteristic*, EQ3Thermostat*> lookupMap;

class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.printf("Connected to %s\n", pclient->getPeerAddress().toString().c_str());
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.printf("Disconnected from %s\n", pclient->getPeerAddress().toString().c_str());
  }
};

char const *EQ3Thermostat::getAddress() {
  return address.toString().c_str();
}

EQ3Thermostat::EQ3Thermostat(BLEAddress add) : address(add) {
  client = BLEDevice::createClient();
}

EQ3Thermostat::EQ3Thermostat(BLEAdvertisedDevice dev) : address(dev.getAddress()) {
  client = BLEDevice::createClient();
};

void EQ3Thermostat::onNotify(uint8_t* pData, size_t length, bool isNotify) {
  if (!(pData[0] == 0x02 && pData[1] == 0x01)) {
    Serial.println("Skipping non-status notification");
   return;
  }

  char stateChr = pData[2] & 0x0f;
  switch (stateChr) {
    case 0x08:
      mode = AUTO;
      break;
    case 0x09:
      mode = MANUAL;
      break;
    default:
      mode = UNKNOWN;
  }

  valve = (((float)pData[3]) / 0x64) * 100;
  temp = ((float)pData[5]) / 2;

  if (statusUpdate) {
    statusUpdate();
  }
}

void EQ3Thermostat::connect() {
  if (client->isConnected()) {
    return;
  }

  Serial.printf("Connecting to %s\n", this->getAddress());
  client->setClientCallbacks(new ClientCallbacks());

  bool connected = client->connect(address);
  if (!connected) {
    Serial.println("Failed to connect to device.");
    return;
  }

  this->setupCharacteristics();

  // We need to setup notifications every single time,
  // even though we cache the characteristics.
  this->setupNotifications();
};

void EQ3Thermostat::setupCharacteristics() {
  // If already set up, ignore
  if (notifyCharacteristic != nullptr) {
      return;
  }

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = client->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    client->disconnect();
    return;
  }

  // // Obtain a reference to the characteristic in the service of the remote BLE server.
  notifyCharacteristic = pRemoteService->getCharacteristic(notifyUUID);
  lookupMap[notifyCharacteristic] = this;

  if (notifyCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    client->disconnect();
    return;
  }

  commandCharacteristic = pRemoteService->getCharacteristic(commandUUID);
  if (commandCharacteristic == nullptr) {
    Serial.print("Failed to find our command characteristic UUID: ");
    client->disconnect();
    return;
  }
}

void EQ3Thermostat::setupNotifications() {
  notifyCharacteristic->registerForNotify([](BLERemoteCharacteristic* chara, uint8_t* data, size_t length, bool isNotify) {
    auto thermo = lookupMap[chara];
    thermo->onNotify(data, length, isNotify);
  });
}

void EQ3Thermostat::setTemperature(float temperature) {
    this->connect();

    uint8_t command[] = { 0x41, (uint8_t)(temperature * 2) };
    commandCharacteristic->writeValue(command, 2, true);
    Serial.printf("Updated temperature to %2.1f\n", temperature);
};

void EQ3Thermostat::setMode(Mode newMode) {
    this->connect();

    uint8_t modeByte = newMode == AUTO ? 0x00 : 0x40;
    uint8_t command[] = { 0x40, modeByte };
    commandCharacteristic->writeValue(command, 2, true);
    Serial.printf("Updated mode");
};

void EQ3Thermostat::setUpdateCallback(statusUpdateCallback cb) {
  statusUpdate = cb;
}

float EQ3Thermostat::getTemperature() {
  return temp;
}

float EQ3Thermostat::getValveStatus() {
  return valve;
}

EQ3Thermostat::Mode EQ3Thermostat::getMode() {
  return mode;
}

bool EQ3Thermostat::isThermostat(BLEAdvertisedDevice &dev) {
    return  dev.haveServiceUUID() && dev.isAdvertisingService(serviceUUID);
}
