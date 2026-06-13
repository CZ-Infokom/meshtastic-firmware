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
    bool drainAndParse();
    bool processByte(uint8_t byte);

    float lastDistanceMm = -1;
    bool hasValidReading = false;
    uint8_t frame[4] = {0};
    uint8_t frameLen = 0;
};

#endif
