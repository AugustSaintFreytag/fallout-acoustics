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

	const char* EnvironmentTypeName(std::uint32_t type) {
		if (type >= std::size(kEnvironmentNames)) {
			return "?";
		}

		return kEnvironmentNames[type];
	}

	std::uint32_t EnvironmentTypeFromName(const char* name, std::uint32_t fallback) {
		for (std::uint32_t type = 1; type < std::size(kEnvironmentNames); ++type) {
			if (_stricmp(name, kEnvironmentNames[type]) == 0) {
				return type;
			}
		}

		return fallback;
	}
}
