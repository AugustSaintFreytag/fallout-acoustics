#include "hooks/voice_hooks.h"

#include "config/settings.h"
#include "debug/cover_probe.h"
#include "effects/voice_cover.h"
#include "engine/addresses.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <cstdint>

namespace sea::hooks {
	namespace {
		using VoiceModulationCheckFn = bool(__thiscall*)(void* actor);

		std::uint32_t __fastcall SpeakerCoverFlags(void* actor) {
			const effects::VoiceCover cover = effects::ClassifySpeaker(actor);

			if (config::Get().debug.logVoiceCover) {
				const auto vanillaCheck = reinterpret_cast<VoiceModulationCheckFn>(engine::kVoiceModulationCheck);
				debug::LogVoiceCover(actor, cover, vanillaCheck(actor));
			}

			return effects::VoiceCoverFlags(cover);
		}

		// Inline assembly calls through this variable. It cannot name a function in a namespace.
		std::uint32_t(__fastcall* g_speakerCoverFlags)(void* actor) = &SpeakerCoverFlags;

		// Replaces `call kVoiceModulationCheck`. ecx = speaking actor.
		// Adds the cover flags to the sound flags of the caller ([ebp-0x240]) and returns false,
		// so that the engine does not add Modulated by itself.
		__declspec(naked) void Hook_VoiceModulationCheck() {
			__asm {
				call g_speakerCoverFlags
				or dword ptr [ebp - 0x240], eax
				xor eax, eax
				ret
			}
		}
	}

	bool InstallVoiceHooks() {
		if (!config::Get().voiceFilters.enabled) {
			return true;
		}

		if (!mem::PatchCall(engine::kVoiceModulationCall, engine::kVoiceModulationCheck,
				reinterpret_cast<void*>(&Hook_VoiceModulationCheck))) {
			SEA_LOG("Error: Voice modulation check at %08X is not the expected call. Another plugin may patch it.",
				static_cast<unsigned>(engine::kVoiceModulationCall));

			return false;
		}

		SEA_LOG("Hooks: Actor::VoiceSoundFunction modulation check %08X -> %p",
			static_cast<unsigned>(engine::kVoiceModulationCheck), &Hook_VoiceModulationCheck);

		return true;
	}
}
