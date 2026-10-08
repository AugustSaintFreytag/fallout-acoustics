#include "fixes/open_close_sounds.h"

#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/objects.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <cstdint>

namespace sea::fixes {

	using mem::Field;

	namespace {

		using PlayFn = bool(__thiscall*)(void* handle, bool loop);
		using SetVolumeFn = bool(__thiscall*)(void* handle, float volume);

		constexpr int kMaxControllers = 16;

		// Queues volume 0 on the handle before the play message. The engine still plays and frees the sound as usual.
		void Mute(void* handle) {
			reinterpret_cast<SetVolumeFn>(engine::kSoundHandle_SetVolume)(handle, 0.0f);
		}

		bool Play(void* handle, bool loop) {
			return reinterpret_cast<PlayFn>(engine::kSoundHandle_Play)(handle, loop);
		}

		void LogMuted(const char* kind, void* reference) {
			if (!config::Get().debug.logSoundPlay) {
				return;
			}

			SEA_LOG("[OpenClose] Muted the 2D %s sound of %08X '%s'.", kind, engine::GetFormID(reference),
				engine::GetEditorID(engine::GetBaseForm(reference)));
		}

		// Checks if the loaded model of a reference has a `NiControllerManager` (open/close animation sequences).
		bool HasControllerManager(void* reference) {
			void* renderState = Field<void*>(reference, engine::kRefr_RenderState);

			if (!renderState) {
				return false;
			}

			void* rootNode = Field<void*>(renderState, engine::kRenderState_RootNode);

			if (!rootNode) {
				return false;
			}

			void* controller = Field<void*>(rootNode, engine::kNiObjectNET_Controller);

			for (int index = 0; controller && index < kMaxControllers; ++index) {
				if (Field<std::uintptr_t>(controller, 0) == engine::kVtbl_NiControllerManager) {
					return true;
				}

				controller = Field<void*>(controller, engine::kTimeController_Next);
			}

			return false;
		}

		// Replaces the `Play` call in `HandleActivate`. Mutes the copy if the player opened the door.
		bool __fastcall OpenCloseSoundPlay(void* handle, void* actionRef, bool loop, void* door) {
			if (actionRef && actionRef == engine::GetPlayer()) {
				Mute(handle);
				LogMuted("door", door);
			}

			return Play(handle, loop);
		}

		// Replaces the `Play` call of the container menu. Mutes the copy for containers with an animation.
		bool __fastcall ContainerMenuSoundPlay(void* handle, void* container, bool loop) {
			void* baseForm = engine::GetBaseForm(container);

			if (engine::GetFormType(baseForm) == engine::kFormType_TESObjectCONT && HasControllerManager(container)) {
				Mute(handle);
				LogMuted("container", container);
			}

			return Play(handle, loop);
		}

		// Inline assembly calls through these variables. It cannot name a function in a namespace.
		bool(__fastcall* g_openCloseSoundPlay)(void* handle, void* actionRef, bool loop, void* door) = &OpenCloseSoundPlay;
		bool(__fastcall* g_containerMenuSoundPlay)(void* handle, void* container, bool loop) = &ContainerMenuSoundPlay;

		// Replaces `call BSSoundHandle::Play` in BGSOpenCloseForm::HandleActivate. ecx = handle, [esp+4] = loop.
		// ebp is still the frame of HandleActivate: [ebp+8] = activated door, [ebp+0xC] = action ref.
		__declspec(naked) void Hook_OpenCloseSoundPlay() {
			__asm {
				push dword ptr [ebp + 8]
				push dword ptr [esp + 8]
				mov edx, dword ptr [ebp + 0xC]
				call g_openCloseSoundPlay
				ret 4
			}
		}

		// Replaces `call BSSoundHandle::Play` in the container menu sound function. [ebp+8] = container ref.
		__declspec(naked) void Hook_ContainerMenuSoundPlay() {
			__asm {
				mov edx, dword ptr [ebp + 8]
				push dword ptr [esp + 4]
				call g_containerMenuSoundPlay
				ret 4
			}
		}

		// Points a `call BSSoundHandle::Play` at given hook and logs the result.
		bool PatchPlayCall(std::uintptr_t callAddress, void* hook, const char* name) {
			if (!mem::PatchCall(callAddress, engine::kSoundHandle_Play, hook)) {
				SEA_LOG("Error: %s sound call at %08X is not the expected call. Another plugin may patch it.", name,
					static_cast<unsigned>(callAddress));

				return false;
			}

			SEA_LOG("Hooks: %s sound call %08X -> %p", name, static_cast<unsigned>(callAddress), hook);

			return true;
		}

	}

	// Mutes the 2D copy of door and container open/close sounds where the object's animation plays a positioned copy.
	// Patches two `BSSoundHandle::Play` calls if `[Fixes] bFixDoubleOpenCloseSounds` is on.
	// Returns false if a call site is not the expected call.
	//
	// - Doors activated by the player: always. Every non-load door that makes a sound animates.
	//   Load doors use another path and keep their 2D sound into the loading screen.
	// - Containers in the container menu: when the container has a `NiControllerManager` (lid animation).
	//
	// Background in `docs/10 History.md`, "Open/Close Sound Fix".
	//
	// Thread: Main (load time only)
	bool InstallOpenCloseSoundFix() {
		if (!config::Get().fixes.openCloseSounds) {
			return true;
		}

		const bool doorPatched = PatchPlayCall(engine::kOpenCloseSoundPlayCall,
			reinterpret_cast<void*>(&Hook_OpenCloseSoundPlay), "Door open/close");
		const bool containerPatched = PatchPlayCall(engine::kContainerMenuSoundPlayCall,
			reinterpret_cast<void*>(&Hook_ContainerMenuSoundPlay), "Container menu");

		return doorPatched && containerPatched;
	}

}
