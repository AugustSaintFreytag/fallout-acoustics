#include "debug/eax_readback.h"

#include "audio/eax.h"
#include "audio/eax_property.h"
#include "config/settings.h"
#include "utils/log.h"

#include <cstdint>

namespace sea::debug {
	namespace {
		std::uint32_t g_sourceReadbacksDone = 0;  // Audio thread only.

		const char* SlotName(const GUID& slotId) {
			if (slotId == eax::kNull) {
				return "null";
			}

			if (slotId == eax::kFXSlot0) {
				return "FXSlot0";
			}

			return "other";
		}
	}

	void ReadBackSlot(IKsPropertySet* propertySet) {
		if (config::Get().debug.readbackCount == 0) {
			return;
		}

		eax::ReverbProperties reverb{};
		LONG volume = 0;

		if (!eax::GetProperty(propertySet, eax::kFXSlot0, eax::kReverb_AllParameters, &reverb, sizeof(reverb))) {
			return;
		}

		if (!eax::GetProperty(propertySet, eax::kFXSlot0, eax::kFXSlot_Volume, &volume, sizeof(volume))) {
			return;
		}

		SEA_LOG("[Readback] Slot 0: Volume %ld mB, Env %u, Size %.1f, Room %ld, RoomHF %ld, Decay %.2fs, Reflections %ld, Reverb %ld",
			volume, reverb.environment, reverb.environmentSize, reverb.room, reverb.roomHF, reverb.decayTime,
			reverb.reflections, reverb.reverb);
	}

	void ReadBackSource(IKsPropertySet* propertySet, const char* label) {
		if (g_sourceReadbacksDone >= config::Get().debug.readbackCount) {
			return;
		}

		++g_sourceReadbacksDone;

		eax::ActiveFXSlots slots{};
		eax::SourceSendProperties sends[4]{};  // OpenAL Soft writes EAX_MAX_FXSLOTS (4) entries

		if (!eax::GetProperty(propertySet, eax::kSource, eax::kSource_ActiveFXSlotID, &slots, sizeof(slots))) {
			return;
		}

		if (!eax::GetProperty(propertySet, eax::kSource, eax::kSource_SendParameters, sends, sizeof(sends))) {
			return;
		}

		SEA_LOG("[Readback] Source (%s): Active {%s, %s}, Send[0] %s %ld mB", label, SlotName(slots.slots[0]),
			SlotName(slots.slots[1]), SlotName(sends[0].receivingFXSlotID), sends[0].send);
	}
}
