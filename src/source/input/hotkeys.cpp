#include "input/hotkeys.h"

#include "config/settings.h"
#include "debug/occlusion_test.h"
#include "debug/ray_probe.h"
#include "engine/ui.h"
#include "occlusion/object_lists.h"
#include "occlusion/occlusion.h"
#include "reverb/reverb.h"

#include <Windows.h>

namespace sea::input {

	namespace {

		// Thread: Main
		bool g_bypassKeyWasDown = false;
		bool g_rayProbeKeyWasDown = false;
		bool g_occlusionTestKeyWasDown = false;
		bool g_occlusionBypassKeyWasDown = false;
		bool g_reloadKeyWasDown = false;

		bool GameHasFocus() {
			DWORD foregroundProcessId = 0;
			GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcessId);

			return foregroundProcessId == GetCurrentProcessId();
		}

		// Converts a DirectInput scan code (DIK_*, as listed on the GECK wiki) to a Windows virtual-key code.
		// DIK codes above 0x7F are E0-prefixed (extended) keys like Page Up (0xC9) or Insert (0xD2).
		int ToVirtualKey(int scanCode) {
			const UINT code = (scanCode & 0x80) ? (0xE000 | (scanCode & 0x7F)) : static_cast<UINT>(scanCode);
			return static_cast<int>(MapVirtualKeyW(code, MAPVK_VSC_TO_VK_EX));
		}

		// Checks if the key (DirectInput scan code) was pressed on this frame.
		bool WasPressed(int scanCode, bool& keyWasPressedInPriorFrame) {
			const int key = ToVirtualKey(scanCode);
			const bool keyIsPressed = key != 0 && (GetAsyncKeyState(key) & 0x8000) != 0 && GameHasFocus();
			const bool keyIsPressedInThisFrame = keyIsPressed && !keyWasPressedInPriorFrame;

			keyWasPressedInPriorFrame = keyIsPressed;

			return keyIsPressedInThisFrame;
		}

		// Toggles the reverb bypass on `[Spatialization] iBypassKey` and shows the new state.
		void PollBypassKey() {
			const config::SpatializationSettings& settings = config::Get().spatialization;
			const int bypassKey = settings.bypassKey;

			if (!bypassKey || !settings.enabled) {
				return;
			}

			if (!WasPressed(bypassKey, g_bypassKeyWasDown)) {
				return;
			}

			const bool isReverbActive = reverb::ToggleBypass();

			if (isReverbActive) {
				engine::ShowNotification("Acoustics Processing: ON");
			} else {
				engine::ShowNotification("Acoustics Processing: OFF");
			}
		}

		// Casts the debug rays on `[Debug] iRayProbeKey`.
		void PollRayProbeKey() {
			const int rayProbeKey = config::Get().debug.rayProbeKey;

			if (!rayProbeKey) {
				return;
			}

			if (WasPressed(rayProbeKey, g_rayProbeKeyWasDown)) {
				debug::RunRayProbe();
			}
		}

		// Toggles the test occlusion on `[Debug] iOcclusionTestKey` and shows the new state.
		void PollOcclusionTestKey() {
			const config::DebugSettings& debug = config::Get().debug;

			if (!debug.occlusionTestKey || debug.testOcclusion == 0) {
				return;
			}

			if (!WasPressed(debug.occlusionTestKey, g_occlusionTestKeyWasDown)) {
				return;
			}

			const bool isOcclusionActive = debug::ToggleTestOcclusion();

			if (isOcclusionActive) {
				engine::ShowNotification("Test Occlusion: ON");
			} else {
				engine::ShowNotification("Test Occlusion: OFF");
			}
		}

		// Toggles the occlusion bypass on `[Occlusion] iBypassKey` and shows the new state.
		void PollOcclusionBypassKey() {
			const config::OcclusionSettings& settings = config::Get().occlusion;

			if (!settings.bypassKey || !settings.enabled) {
				return;
			}

			if (!WasPressed(settings.bypassKey, g_occlusionBypassKeyWasDown)) {
				return;
			}

			const bool isOcclusionActive = occlusion::ToggleBypass();

			if (isOcclusionActive) {
				engine::ShowNotification("Occlusion: ON");
			} else {
				engine::ShowNotification("Occlusion: OFF");
			}
		}

		// Reloads the INI on `[Debug] iReloadKey` and shows a notification.
		void PollReloadKey() {
			const int reloadKey = config::Get().debug.reloadKey;

			if (!reloadKey) {
				return;
			}

			if (!WasPressed(reloadKey, g_reloadKeyWasDown)) {
				return;
			}

			config::Reload();
			occlusion::LoadObjectLists();
			engine::ShowNotification("Acoustics Settings: Reloaded");
		}

	}

	// Polls the hotkeys and runs the action of each key that went down. Only while the game window has focus.
	//
	// Thread: Main (each frame)
	void PollHotkeys() {
		PollBypassKey();
		PollOcclusionBypassKey();
		PollRayProbeKey();
		PollOcclusionTestKey();
		PollReloadKey();
	}

}
