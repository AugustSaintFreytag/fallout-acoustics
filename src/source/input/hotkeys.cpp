#include "input/hotkeys.h"

#include "config/settings.h"
#include "engine/ui.h"
#include "reverb/reverb.h"

#include <Windows.h>

namespace sea::input {
	namespace {
		bool g_bypassKeyWasDown = false;

		bool GameHasFocus() {
			DWORD foregroundProcessId = 0;
			GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcessId);

			return foregroundProcessId == GetCurrentProcessId();
		}

		void PollBypassKey() {
			const config::Settings& settings = config::Get();
			const int bypassKey = settings.hotkeys.bypassKey;

			if (!bypassKey || !settings.reverb.enabled) {
				return;
			}

			const bool keyDown = (GetAsyncKeyState(bypassKey) & 0x8000) != 0 && GameHasFocus();

			if (keyDown && !g_bypassKeyWasDown) {
				const bool reverbActive = reverb::ToggleBypass();

				if (reverbActive) {
					engine::ShowNotification("Acoustics Processing: ON");
				} else {
					engine::ShowNotification("Acoustics Processing: OFF");
				}
			}

			g_bypassKeyWasDown = keyDown;
		}
	}

	void PollHotkeys() {
		PollBypassKey();
	}
}
