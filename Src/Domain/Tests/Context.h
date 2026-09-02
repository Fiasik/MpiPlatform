#pragma once

#include "Domain/Mpi/Device.h"
#include "Domain/DeviceConfig.h"
#include "Storage/Telemetry.h"

namespace Domain::Tests {
    struct Context {
        Mpi::Device& device;
        Telemetry& telemetry;
        const DeviceConfig& config;
    };
}
