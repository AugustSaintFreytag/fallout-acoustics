#include "engine/environment.h"

#include <cstring>
#include <iterator>

namespace sea::engine {

	namespace {
		constexpr const char* kEnvironmentNames[] = {
			"None", "Default", "Generic", "PaddedCell", "Room", "Bathroom", "Livingroom",
			"StoneRoom", "Auditorium", "ConcertHall", "Cave", "Arena", "Hangar",
			"CarpetedHallway", "Hallway", "StoneCorridor", "Alley", "Forest", "City",
			"Mountains", "Quarry", "Plain", "ParkingLot", "SewerPipe", "Underwater",
			"SmallRoom", "MediumRoom", "LargeRoom", "MediumHall", "LargeHall", "Plate",
		};
	}

	// Returns the name of an environment type (ANAM value), such as "MediumRoom".
	// Is set to string "?" for unknown values.
	//
	// ANAM value = DSFX_I3DL2_ENVIRONMENT_PRESET_* + 1 (JIP BGSAcousticSpace).
	const char* EnvironmentTypeName(std::uint32_t type) {
		if (type >= std::size(kEnvironmentNames)) {
			return "?";
		}

		return kEnvironmentNames[type];
	}

	// Returns the environment type for a name, case insensitive, or `fallback` for an unknown name.
	std::uint32_t EnvironmentTypeFromName(const char* name, std::uint32_t fallback) {
		for (std::uint32_t type = 1; type < std::size(kEnvironmentNames); ++type) {
			if (_stricmp(name, kEnvironmentNames[type]) == 0) {
				return type;
			}
		}

		return fallback;
	}

}
