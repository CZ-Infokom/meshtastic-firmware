#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_DYP_A01)

#include "DYPA01Sensor.h"
#include "TelemetrySensor.h"
#include "Throttle.h"
#include <Arduino.h>

#ifndef DYP_A01_UART_BAUD
#define DYP_A01_UART_BAUD 9600
#endif

#ifndef DYP_A01_UART_CONTROLLED
#define DYP_A01_UART_CONTROLLED 1
#endif

#ifndef DYP_A01_TRIGGER_BYTE
#define DYP_A01_TRIGGER_BYTE 0x01
#endif

#ifndef DYP_A01_POST_TRIGGER_WAIT_MS
#define DYP_A01_POST_TRIGGER_WAIT_MS 60
#endif

#ifndef DYP_A01_MIN_TRIGGER_GAP_MS
#define DYP_A01_MIN_TRIGGER_GAP_MS 80
#endif

#ifndef DYP_A01_BURST_SAMPLES
#define DYP_A01_BURST_SAMPLES 10
#endif

#ifndef DYP_A01_AUTO_INTERVAL_MS
#define DYP_A01_AUTO_INTERVAL_MS 250
#endif

#ifndef DYP_A01_AUTO_REALTIME
#define DYP_A01_AUTO_REALTIME 0
#endif

#ifndef DYP_A01_POWER_BOOT_MS
#define DYP_A01_POWER_BOOT_MS 100
#endif

#ifndef DYP_A01_POWER_SETTLE_MS
#define DYP_A01_POWER_SETTLE_MS 50
#endif

#if defined(ARCH_ESP32)
#if defined(DYP_A01_USE_SERIAL2)
#define DYP_A01_STREAM Serial2
#else
#define DYP_A01_STREAM Serial1
#endif
#elif defined(ARCH_NRF52)
#if defined(DYP_A01_USE_SERIAL1)
#define DYP_A01_STREAM Serial1
#else
#define DYP_A01_STREAM Serial2
#endif
#endif

#if DYP_A01_UART_CONTROLLED
#ifndef DYP_A01_UART_TX
#error "DYP_A01_UART_CONTROLLED requires DYP_A01_UART_TX (sensor RX / Yellow wire)"
#endif
#endif

DYPA01Sensor::DYPA01Sensor() : TelemetrySensor(meshtastic_TelemetrySensorType_DYP_A01, "DYP_A01") {}

bool DYPA01Sensor::initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev)
{
    LOG_INFO("Init sensor: %s", sensorName);
    setPeripheralPower(false);
    setupUart();
#if DYP_A01_UART_CONTROLLED
    LOG_INFO("%s: UART controlled on-demand (%u samples/burst, gap %d ms)", sensorName, DYP_A01_BURST_SAMPLES,
             DYP_A01_MIN_TRIGGER_GAP_MS);
#ifdef DYP_A01_POWER_EN
    LOG_INFO("%s: switched rail on GPIO %d (off between bursts)", sensorName, DYP_A01_POWER_EN);
#endif
#else
    LOG_INFO("%s: UART auto-output mode (poll every %d ms)", sensorName, DYP_A01_AUTO_INTERVAL_MS);
#if DYP_A01_AUTO_REALTIME
    LOG_INFO("%s: auto real-time output (sensor RX held low)", sensorName);
#endif
#endif
#if defined(DYP_A01_USE_SERIAL2)
    LOG_INFO("%s: using Serial2 backend", sensorName);
#elif defined(DYP_A01_USE_SERIAL1)
    LOG_INFO("%s: using Serial1 backend", sensorName);
#elif defined(ARCH_ESP32)
    LOG_INFO("%s: using Serial1 backend", sensorName);
#else
    LOG_INFO("%s: using Serial2 backend", sensorName);
#endif
    status = 1;
    initialized = true;
    return true;
}

void DYPA01Sensor::setupUart()
{
#if defined(ARCH_ESP32)
    DYP_A01_STREAM.end();
    DYP_A01_STREAM.begin(DYP_A01_UART_BAUD, SERIAL_8N1, DYP_A01_UART_RX, DYP_A01_UART_TX);
#elif defined(ARCH_NRF52)
#if defined(DYP_A01_USE_SERIAL1)
    Serial1.setPins(DYP_A01_UART_RX, DYP_A01_UART_TX);
    Serial1.begin(DYP_A01_UART_BAUD);
#else
    Serial2.setPins(DYP_A01_UART_RX, DYP_A01_UART_TX);
    Serial2.begin(DYP_A01_UART_BAUD);
#endif
#else
#error "DYP-A01 UART sensor is not supported on this architecture yet"
#endif
    LOG_INFO("%s: UART %d baud RX=%d TX=%d", sensorName, DYP_A01_UART_BAUD, DYP_A01_UART_RX, DYP_A01_UART_TX);

#if !DYP_A01_UART_CONTROLLED && DYP_A01_AUTO_REALTIME
    pinMode(DYP_A01_UART_TX, OUTPUT);
    digitalWrite(DYP_A01_UART_TX, LOW);
#endif
}

void DYPA01Sensor::setPeripheralPower(bool on)
{
#ifdef DYP_A01_POWER_EN
    if (on == peripheralPowerOn) {
        return;
    }
    pinMode(DYP_A01_POWER_EN, OUTPUT);
    if (on) {
#ifdef PIN_GPS_STANDBY
        pinMode(PIN_GPS_STANDBY, OUTPUT);
        digitalWrite(PIN_GPS_STANDBY, GPS_STANDBY_ACTIVE);
#endif
#ifdef PIN_GPS_RESET
        pinMode(PIN_GPS_RESET, OUTPUT);
        digitalWrite(PIN_GPS_RESET, GPS_RESET_MODE);
#endif
        digitalWrite(DYP_A01_POWER_EN, DYP_A01_POWER_EN_ACTIVE);
        delay(DYP_A01_POWER_SETTLE_MS);
        delay(DYP_A01_POWER_BOOT_MS);
        LOG_DEBUG("%s: switched rail on (GPIO %d)", sensorName, DYP_A01_POWER_EN);
    } else {
        digitalWrite(DYP_A01_POWER_EN, !DYP_A01_POWER_EN_ACTIVE);
        LOG_DEBUG("%s: switched rail off (GPIO %d)", sensorName, DYP_A01_POWER_EN);
    }
    peripheralPowerOn = on;
#else
    (void)on;
#endif
}

void DYPA01Sensor::flushInput()
{
    while (DYP_A01_STREAM.available() > 0) {
        DYP_A01_STREAM.read();
    }
    frameLen = 0;
}

void DYPA01Sensor::triggerMeasurement()
{
    flushInput();
    DYP_A01_STREAM.write(DYP_A01_TRIGGER_BYTE);
    triggerCount++;
    awaitingResponse = true;
    triggerSentMs = millis();
    LOG_DEBUG("%s: trigger #%u byte=0x%02x", sensorName, triggerCount, DYP_A01_TRIGGER_BYTE);
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
    LOG_DEBUG("%s: frame %02x %02x %02x %02x", sensorName, frame[0], frame[1], frame[2], frame[3]);
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
    return true;
}

bool DYPA01Sensor::drainAndParse()
{
    bool updated = false;
    while (DYP_A01_STREAM.available() > 0) {
        updated = processByte((uint8_t)DYP_A01_STREAM.read()) || updated;
    }
    return updated;
}

bool DYPA01Sensor::captureDistanceSample(float &outMm)
{
    triggerMeasurement();

    while (Throttle::isWithinTimespanMs(triggerSentMs, DYP_A01_POST_TRIGGER_WAIT_MS)) {
        delay(1);
    }
    awaitingResponse = false;
    frameLen = 0;
    hasValidReading = false;

    bool gotSample = false;
    const uint32_t readDeadlineMs = millis();
    while (Throttle::isWithinTimespanMs(readDeadlineMs, 40)) {
        drainAndParse();
        if (hasValidReading) {
            outMm = lastDistanceMm;
            hasValidReading = false;
            gotSample = true;
            break;
        }
        delay(1);
    }

    while (Throttle::isWithinTimespanMs(triggerSentMs, DYP_A01_MIN_TRIGGER_GAP_MS)) {
        delay(1);
    }
    return gotSample;
}

int32_t DYPA01Sensor::runOnce()
{
    if (!initialized) {
        return DEFAULT_SENSOR_MINIMUM_WAIT_TIME_BETWEEN_READS;
    }

#if DYP_A01_UART_CONTROLLED
    // Controlled mode: measure only when getMetrics() runs (telemetry send interval).
    return INT32_MAX;
#else
    drainAndParse();
    if (!Throttle::isWithinTimespanMs(lastStatusLogMs, 5000)) {
        lastStatusLogMs = millis();
        LOG_INFO("%s: status valid=%d uart_avail=%d", sensorName, hasValidReading, DYP_A01_STREAM.available());
    }
    return DYP_A01_AUTO_INTERVAL_MS;
#endif
}

bool DYPA01Sensor::getMetrics(meshtastic_Telemetry *measurement)
{
#if DYP_A01_UART_CONTROLLED
    setPeripheralPower(true);
    float sumMm = 0;
    uint8_t validSamples = 0;

    for (uint8_t i = 0; i < DYP_A01_BURST_SAMPLES; i++) {
        float sampleMm = 0;
        if (captureDistanceSample(sampleMm)) {
            sumMm += sampleMm;
            validSamples++;
            LOG_DEBUG("%s: burst sample %u/%u = %.0f mm", sensorName, i + 1, DYP_A01_BURST_SAMPLES, sampleMm);
        }
    }

    setPeripheralPower(false);

    if (validSamples == 0) {
        if (!Throttle::isWithinTimespanMs(lastNoMetricsLogMs, 5000)) {
            lastNoMetricsLogMs = millis();
            LOG_DEBUG("%s: burst had no valid distance samples", sensorName);
        }
        return false;
    }

    lastDistanceMm = sumMm / validSamples;
    hasValidReading = true;
    LOG_INFO("%s: distance=%.0f mm (avg of %u/%u samples)", sensorName, lastDistanceMm, validSamples, DYP_A01_BURST_SAMPLES);
#else
    setPeripheralPower(true);
    drainAndParse();
    setPeripheralPower(false);
    if (!hasValidReading) {
        if (!Throttle::isWithinTimespanMs(lastNoMetricsLogMs, 5000)) {
            lastNoMetricsLogMs = millis();
            LOG_DEBUG("%s: no valid distance yet (not publishing distance)", sensorName);
        }
        return false;
    }
#endif

    measurement->variant.environment_metrics.has_distance = true;
    measurement->variant.environment_metrics.distance = lastDistanceMm;
    return true;
}

#endif
