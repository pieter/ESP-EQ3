#include <Arduino.h>
#include <functional>
#include "BLEDevice.h"

// Based on https://reverse-engineering-ble-devices.readthedocs.io/en/latest/protocol_description/00_protocol_description.html

using statusUpdateCallback = std::function<void()>;

class EQ3Thermostat {

  public:

    enum Mode {
      AUTO, MANUAL, UNKNOWN
    };

    EQ3Thermostat(BLEAdvertisedDevice dev);
    EQ3Thermostat(BLEAddress address);

    void setUpdateCallback(statusUpdateCallback cb);
    char const *getAddress();

    void setTemperature(float temperature);
    void setMode(Mode mode);

    // Note: These states can only read after a mutation.
    // If you don't have anything to modify, easiest is to update the
    // time (not implemented yet)
    float getValveStatus();
    float getTemperature();
    Mode getMode();

    // Can be used to filter results from BLEScan for only
    // valid devices
    static bool isThermostat(BLEAdvertisedDevice &dev);

  private:
    // Status
    Mode mode = UNKNOWN;
    float temp = 0.0;
    float valve = 0.0;

    // BLE stuff
    BLEAddress address;
    BLEClient *client = nullptr;
    BLERemoteCharacteristic *commandCharacteristic = nullptr;
    BLERemoteCharacteristic *notifyCharacteristic = nullptr;
    statusUpdateCallback statusUpdate = nullptr;

    void onNotify(uint8_t* pData, size_t length, bool isNotify);
    void connect();
    void setupCharacteristics();
    void setupNotifications();
};
