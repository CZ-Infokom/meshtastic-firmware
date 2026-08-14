#include "configuration.h"

#if !MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR && defined(HAS_DYP_A01)

#pragma once

#include "../mesh/generated/meshtastic/telemetry.pb.h"
#include "TelemetrySensor.h"

class DYPA01Sensor : public TelemetrySensor
{
  public:
    DYPA01Sensor();
    bool initDevice(TwoWire *bus, ScanI2C::FoundDevice *dev) override;
    int32_t runOnce() override;
    bool getMetrics(meshtastic_Telemetry *measurement) override;

  private:
    void setupUart();
    void flushInput();
    void triggerMeasurement();
    bool drainAndParse();
    bool processByte(uint8_t byte);
    bool captureDistanceSample(float &outMm);
    uint8_t captureBurst(float *samples);
    void setPeripheralPower(bool on);

    float lastDistanceMm = -1;
    bool hasValidReading = false;
    bool peripheralPowerOn = false;
    uint8_t frame[4] = {0};
    uint8_t frameLen = 0;
    uint32_t triggerCount = 0;
    uint32_t lastNoMetricsLogMs = 0;
    uint32_t lastStatusLogMs = 0;
    bool awaitingResponse = false;
    uint32_t triggerSentMs = 0;
};

#endif
