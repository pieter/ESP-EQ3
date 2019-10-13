#include <Arduino.h>
#include <list>

#include "BLEDevice.h"
#include "eq3thermostat.h"

static std::list<EQ3Thermostat> thermostats;

uint scanState = 0;

// Use this if you already know the addresses of your devices
void start() {
  thermostats.push_back(BLEAddress("00:1a:22:0e:cb:d4"));
  thermostats.push_back(BLEAddress("00:1a:22:0e:d6:4a"));
  scanState = 2;
}

void startWithScan() {
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, [](BLEScanResults results) {
    for (uint32_t i = 0; i < results.getCount(); i++) {
      BLEAdvertisedDevice dev = results.getDevice(i);
      if (EQ3Thermostat::isThermostat(dev)) {
        thermostats.push_back(dev);
      }
    }

    scanState = 2;
  }, false);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");

  BLEDevice::init("");
}

void printStatus(EQ3Thermostat &thermo) {
  Serial.printf("Status report for %s: ", thermo.getAddress());
  switch(thermo.getMode()) {
    case EQ3Thermostat::AUTO:
      Serial.print("auto");
      break;
    case EQ3Thermostat::MANUAL:
      Serial.print("manual");
      break;
    default:
      Serial.print("HMM???");
  }

  Serial.printf(". Valve status: %02.0f%%. Temp: %2.1f\n", thermo.getValveStatus(), thermo.getTemperature());
}

void loop() {
  if (scanState == 0) {
    scanState = 1;
    start();
  }

  if (scanState == 2) {
    for (auto &thermo : thermostats) {
      Serial.printf("Found thermostat: %s\n", thermo.getAddress());
      thermo.setUpdateCallback([&]() {
        printStatus(thermo);
      });
    }
    scanState = 3;
  }

  if (scanState == 3 && Serial.available() >= 2) {
    String newTemp = Serial.readStringUntil('\n');
    if (newTemp.length() > 0) {
      float tempVal = newTemp.toFloat();
      Serial.printf("Setting new temp to %f\n", tempVal);
      for (auto &thermo : thermostats) {
        thermo.setMode(EQ3Thermostat::AUTO);
        thermo.setTemperature(tempVal);
      }
    }
  }

  delay(1000); // Delay a second between loops.
}