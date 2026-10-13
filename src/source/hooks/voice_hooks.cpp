#include "hooks/voice_hooks.h"

#include "config/settings.h"
#include "debug/cover_probe.h"
#include "effects/voice_cover.h"
#include "engine/addresses.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <cstdint>
#include <iterator>

namespace sea::hooks {

	namespace {

		using VoiceModulationCheckFn = bool(__thiscall*)(void* actor);

		// Classifies the face cover of a speaking actor and returns its sound flags. Logs it with `[Debug] bLogVoiceCover`.
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
		// Adds the cover flags to the caller's sound flags ([ebp-0x240]) and returns false.
		// The engine then does not add Modulated by itself.
		__declspec(naked) void Hook_VoiceModulationCheck() {
			__asm {
				call g_speakerCoverFlags
				or dword ptr [ebp - 0x240], eax
				xor eax, eax
				ret
			}
		}

	}

	// Replaces the vanilla voice modulation check in `Actor::VoiceSoundFunction` with the cover classification.
	// Removes the engine's skip of that check for the player, so player voice lines get a cover too.
	// Does nothing if `[Vocals] bEnabled=0`. Returns false if a patch site does not hold the expected code.
	//
	// Called once from `NVSEPlugin_Load` after `config::Load`.
	//
	// Thread: Main (load time only)
	bool InstallVoiceHooks() {
		constexpr std::uint8_t kPlayerSkipBytes[] = {0x75, 0x23};
		constexpr std::uint8_t kNopBytes[] = {0x90, 0x90};

		if (!config::Get().vocals.enabled) {
			return true;
		}

		if (!mem::PatchCall(engine::kVoiceModulationCall, engine::kVoiceModulationCheck,
				reinterpret_cast<void*>(&Hook_VoiceModulationCheck))) {
			SEA_LOG("Error: Voice modulation check at %08X is not the expected call. Another loaded plugin may have patched the same location.",
				static_cast<unsigned>(engine::kVoiceModulationCall));

			return false;
		}

		SEA_LOG("Hooks: Actor::VoiceSoundFunction modulation check %08X -> %p",
			static_cast<unsigned>(engine::kVoiceModulationCheck), &Hook_VoiceModulationCheck);

		// NPC voices keep their cover if this patch fails.
		if (!mem::PatchBytes(engine::kVoiceModulationPlayerSkip, kPlayerSkipBytes, kNopBytes, std::size(kNopBytes))) {
			SEA_LOG("Error: Player skip of the voice modulation check at %08X is not the expected jump. "
					"Another loaded plugin may have patched the same location.",
				static_cast<unsigned>(engine::kVoiceModulationPlayerSkip));

			return false;
		}

		SEA_LOG("Hooks: Actor::VoiceSoundFunction player skip at %08X removed",
			static_cast<unsigned>(engine::kVoiceModulationPlayerSkip));

		return true;
	}

}
