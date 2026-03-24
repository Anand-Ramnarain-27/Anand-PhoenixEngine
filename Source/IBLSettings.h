#pragma once
#include <cstdint>
#include "EnvironmentMap.h"

struct IBLSettings {
	static constexpr uint32_t IrradianceSize = 32;
	static constexpr uint32_t PrefilterSize = 1024;
	static constexpr uint32_t BRDFLUTSize = 512;
	static constexpr uint32_t NumRoughnessLevels = EnvironmentMap::NUM_ROUGHNESS_LEVELS;
};