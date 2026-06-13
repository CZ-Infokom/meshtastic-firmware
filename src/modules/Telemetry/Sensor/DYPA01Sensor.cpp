#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_DYP_A01)

#include "DYPA01Sensor.h"
#include "TelemetrySensor.h"
#include <Arduino.h>

#ifndef DYP_A01_UART_BAUD
#define DYP_A01_UART_BAUD 9600
#endif

DYPA01Sensor::DYPA01Sensor() : TelemetrySensor(meshtastic_TelemetrySensorType_DYP_A01, "DYP_A01") {}

bool DYPA01Sensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    LOG_INFO("Init sensor: %s", sensorName);
    setupUart();
    status = 1;
    initialized = true;
    return true;
}

void DYPA01Sensor::setupUart()
{
#if defined(ARCH_ESP32)
    Serial1.end();
    Serial1.begin(DYP_A01_UART_BAUD, SERIAL_8N1, DYP_A01_UART_RX, DYP_A01_UART_TX);
#elif defined(ARCH_NRF52)
    Serial2.setPins(DYP_A01_UART_RX, DYP_A01_UART_TX);
    Serial2.begin(DYP_A01_UART_BAUD);
#else
#error "DYP-A01 UART sensor is not supported on this architecture yet"
#endif
    LOG_INFO("%s: UART %d baud RX=%d TX=%d", sensorName, DYP_A01_UART_BAUD, DYP_A01_UART_RX, DYP_A01_UART_TX);
}

bool DYPA01Sensor::processByte(uint8_t byte)
{
    if (frameLen == 0) {
        if (byte != 0xFF) {
            return false;
        }
        frame[0] = byte;
        frameLen = 1;
        return false;
    }

    frame[frameLen++] = byte;
    if (frameLen < 4) {
        return false;
    }

    frameLen = 0;
    const uint8_t dataH = frame[1];
    const uint8_t dataL = frame[2];
    const uint8_t sum = frame[3];
    const uint8_t expectedSum = (uint8_t)((0xFF + dataH + dataL) & 0xFF);
    if (sum != expectedSum) {
        LOG_DEBUG("%s: checksum fail got=0x%02x expected=0x%02x", sensorName, sum, expectedSum);
        return false;
    }

    const uint16_t raw = ((uint16_t)dataH << 8) | dataL;
    lastDistanceMm = (float)raw;
    hasValidReading = true;
    LOG_INFO("%s: distance=%.0f mm", sensorName, lastDistanceMm);
    return true;
}

bool DYPA01Sensor::drainAndParse()
{
    bool updated = false;
#if defined(ARCH_ESP32)
    while (Serial1.available() > 0) {
        updated = processByte((uint8_t)Serial1.read()) || updated;
    }
#elif defined(ARCH_NRF52)
    while (Serial2.available() > 0) {
        updated = processByte((uint8_t)Serial2.read()) || updated;
    }
#endif
    return updated;
}

int32_t DYPA01Sensor::runOnce()
{
    if (!initialized) {
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
    }
    drainAndParse();
    return 250; // sensor auto-output cycle is ~250ms
}

bool DYPA01Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
    drainAndParse();
    if (!hasValidReading) {
        return false;
    }
    measurement->variant.environment_metrics.has_distance = true;
    measurement->variant.environment_metrics.distance = lastDistanceMm;
    return true;
}

#endif
