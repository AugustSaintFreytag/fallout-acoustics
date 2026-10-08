#include "hooks/request_hooks.h"

#include "config/settings.h"
#include "debug/request_probe.h"
#include "engine/addresses.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <Windows.h>
#include <intrin.h>

#include <cstdint>
#include <cstring>

namespace sea::hooks {

	using mem::Field;

	namespace {

		// Prologue of all five handle functions (EXE): push ebp / mov ebp, esp / push ecx / mov [ebp-4], ecx
		constexpr std::uint8_t kHandlePrologue[] = {0x55, 0x8B, 0xEC, 0x51, 0x89, 0x4D, 0xFC};

		// Prologue of the BSAudioManager position function (EXE): push ebp / mov ebp, esp / sub esp, 0x20
		constexpr std::uint8_t kManagerSetPositionPrologue[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x20};

		constexpr std::uint8_t kJumpOpcode = 0xE9;
		constexpr std::size_t kJumpSize = 5;

		using PlayFn = bool(__thiscall*)(void* handle, bool loop);
		using PlayAfterFn = bool(__thiscall*)(void* handle, std::uint32_t delay, std::uint32_t flags);
		using SetPositionFn = bool(__thiscall*)(void* handle, float x, float y, float z);
		using FadeInPlayFn = bool(__thiscall*)(void* handle, std::uint32_t milliseconds);
		using SetObjectToFollowFn = void(__thiscall*)(void* handle, void* object);
		using ManagerSetPositionFn = bool(__thiscall*)(void* manager, std::uint32_t soundId, float x, float y, float z);

		PlayFn g_originalPlay = nullptr;
		PlayAfterFn g_originalPlayAfter = nullptr;
		SetPositionFn g_originalSetPosition = nullptr;
		FadeInPlayFn g_originalFadeInPlay = nullptr;
		SetObjectToFollowFn g_originalSetObjectToFollow = nullptr;
		ManagerSetPositionFn g_originalManagerSetPosition = nullptr;

		std::uint32_t HandleSoundId(void* handle) {
			return Field<std::uint32_t>(handle, engine::kSoundHandle_ID);
		}

		// Reports a sound request to the probe and calls the original.
		//
		// The detour jumps into these functions. `_ReturnAddress` is then the return address into the caller.
		// It must be read in the hook itself and not in a helper function.
		bool __fastcall Hook_Play(void* handle, void* /*edx*/, bool loop) {
			debug::OnSoundRequested(HandleSoundId(handle), debug::SoundRequest::Play,
				reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

			return g_originalPlay(handle, loop);
		}

		bool __fastcall Hook_PlayAfter(void* handle, void* /*edx*/, std::uint32_t delay, std::uint32_t flags) {
			debug::OnSoundRequested(HandleSoundId(handle), debug::SoundRequest::PlayAfter,
				reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

			return g_originalPlayAfter(handle, delay, flags);
		}

		bool __fastcall Hook_SetPosition(void* handle, void* /*edx*/, float x, float y, float z) {
			debug::OnSoundRequested(HandleSoundId(handle), debug::SoundRequest::SetPosition,
				reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

			return g_originalSetPosition(handle, x, y, z);
		}

		bool __fastcall Hook_FadeInPlay(void* handle, void* /*edx*/, std::uint32_t milliseconds) {
			debug::OnSoundRequested(HandleSoundId(handle), debug::SoundRequest::FadeInPlay,
				reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

			return g_originalFadeInPlay(handle, milliseconds);
		}

		void __fastcall Hook_SetObjectToFollow(void* handle, void* /*edx*/, void* object) {
			debug::OnSoundRequested(HandleSoundId(handle), debug::SoundRequest::SetObjectToFollow,
				reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

			g_originalSetObjectToFollow(handle, object);
		}

		// Reports a position request queued through `BSAudioManager`.
		// Also reached when another plugin replaces `BSSoundHandle::SetPosition` but still queues the message.
		bool __fastcall Hook_ManagerSetPosition(void* manager, void* /*edx*/, std::uint32_t soundId, float x, float y, float z) {
			debug::OnSoundRequested(soundId, debug::SoundRequest::SetPosition,
				reinterpret_cast<std::uintptr_t>(_ReturnAddress()));

			return g_originalManagerSetPosition(manager, soundId, x, y, z);
		}

		// Logs the first bytes of a function that could not be detoured. Also logs where a jump at its start leads.
		void LogUnexpectedCode(std::uintptr_t address, const char* name) {
			const auto* code = reinterpret_cast<const std::uint8_t*>(address);

			std::uintptr_t jumpTarget = 0;
			char modulePath[MAX_PATH] = "";
			const char* moduleName = "-";

			// A jump at the start shows where another plugin sends the function.
			if (code[0] == kJumpOpcode) {
				std::int32_t relativeTarget = 0;
				std::memcpy(&relativeTarget, code + 1, sizeof(relativeTarget));
				jumpTarget = address + kJumpSize + relativeTarget;
				moduleName = mem::ModuleNameAt(jumpTarget, modulePath, sizeof(modulePath));
			}

			SEA_LOG("Error: %s at %08X has unexpected code %02X %02X %02X %02X %02X %02X %02X "
					"(jump target %08X in %s). Another plugin may patch it.",
				name, static_cast<unsigned>(address), code[0], code[1], code[2], code[3], code[4], code[5], code[6],
				static_cast<unsigned>(jumpTarget), moduleName);
		}

		// Detours a function that starts with given prologue and logs the result. Returns the trampoline or `nullptr`.
		void* Detour(std::uintptr_t address, const std::uint8_t* prologue, std::size_t prologueLength, void* hook,
			const char* name) {
			void* trampoline = mem::DetourFunction(address, prologue, prologueLength, hook);

			if (!trampoline) {
				LogUnexpectedCode(address, name);

				return nullptr;
			}

			SEA_LOG("Hooks: %s %08X -> %p (trampoline %p)", name, static_cast<unsigned>(address), hook, trampoline);

			return trampoline;
		}

		void* DetourHandle(std::uintptr_t address, void* hook, const char* name) {
			return Detour(address, kHandlePrologue, sizeof(kHandlePrologue), hook, name);
		}

	}

	// Detours `BSSoundHandle` play and position functions and `BSAudioManager::SetPosition`.
	// Only if `[Debug] bProbeSoundRequests` is on. Request probe then sees each request with its thread and caller.
	// Returns false if a function has unexpected code. Other functions are still hooked.
	//
	// Thread: Main (load time only)
	bool InstallRequestHooks() {
		if (!config::Get().debug.probeSoundRequests) {
			return true;
		}

		g_originalPlay = static_cast<PlayFn>(
			DetourHandle(engine::kSoundHandle_Play, reinterpret_cast<void*>(&Hook_Play), "BSSoundHandle::Play"));
		g_originalPlayAfter = static_cast<PlayAfterFn>(
			DetourHandle(engine::kSoundHandle_PlayAfter, reinterpret_cast<void*>(&Hook_PlayAfter), "BSSoundHandle::PlayAfter"));
		g_originalSetPosition = static_cast<SetPositionFn>(
			DetourHandle(engine::kSoundHandle_SetPosition, reinterpret_cast<void*>(&Hook_SetPosition), "BSSoundHandle::SetPosition"));
		g_originalFadeInPlay = static_cast<FadeInPlayFn>(
			DetourHandle(engine::kSoundHandle_FadeInPlay, reinterpret_cast<void*>(&Hook_FadeInPlay), "BSSoundHandle::FadeInPlay"));
		g_originalSetObjectToFollow = static_cast<SetObjectToFollowFn>(DetourHandle(engine::kSoundHandle_SetObjectToFollow,
			reinterpret_cast<void*>(&Hook_SetObjectToFollow), "BSSoundHandle::SetObjectToFollow"));

		g_originalManagerSetPosition = static_cast<ManagerSetPositionFn>(Detour(engine::kAudioManager_SetPosition,
			kManagerSetPositionPrologue, sizeof(kManagerSetPositionPrologue), reinterpret_cast<void*>(&Hook_ManagerSetPosition),
			"BSAudioManager::SetPosition"));

		// One of the two position hooks is enough.
		const bool hasPositionHook = g_originalSetPosition || g_originalManagerSetPosition;

		return g_originalPlay && g_originalPlayAfter && g_originalFadeInPlay && g_originalSetObjectToFollow && hasPositionHook;
	}

}
