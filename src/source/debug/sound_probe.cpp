#include "debug/sound_probe.h"

#include "audio/directsound.h"
#include "config/settings.h"
#include "engine/addresses.h"
#include "engine/environment.h"
#include "engine/objects.h"
#include "engine/sound_flags.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <atomic>
#include <cstdio>

namespace sea::debug {
	using mem::Field;

	namespace {
		std::atomic<int> g_layoutProbesLeft{0};

		struct InterfaceName {
			const IID* iid;
			const char* name;
		};

		const InterfaceName kInterfaceNames[] = {
			{&IID_IDirectSound8, "DS8"},
			{&IID_IDirectSoundBuffer8, "Buffer8"},
			{&IID_IDirectSound3DBuffer, "3DBuffer"},
			{&IID_IDirectSound3DListener, "3DListener"},
			{&IID_IKsPropertySet, "KsPropertySet"},
		};

		// Writes names of DirectSound interfaces the given object supports to given buffer.
		void DescribeDSoundObject(IUnknown* object, char* buffer, std::size_t size) {
			buffer[0] = '\0';

			std::size_t usedLength = 0;

			for (const auto& [iid, name] : kInterfaceNames) {
				if (!audio::Supports(object, *iid)) {
					continue;
				}

				const int written = std::snprintf(buffer + usedLength, size - usedLength, " %s", name);

				if (written < 0 || usedLength + written >= size) {
					return;
				}

				usedLength += written;
			}
		}

		// Logs each DirectSound object in a sound's infrastructure fields with its supported interfaces.
		void ProbeLayout(void* sound) {
			SEA_LOG("[Layout] Sound %p: DirectSound objects in +%03X..+%03X", sound,
				static_cast<unsigned>(engine::kWin32Sound_ProbeBegin), static_cast<unsigned>(engine::kWin32Sound_ProbeEnd));

			for (std::uintptr_t offset = engine::kWin32Sound_ProbeBegin; offset < engine::kWin32Sound_ProbeEnd; offset += 4) {
				const std::uint32_t value = Field<std::uint32_t>(sound, offset);

				if (!audio::IsDSoundObject(value)) {
					continue;
				}

				char interfaces[96];
				DescribeDSoundObject(reinterpret_cast<IUnknown*>(value), interfaces, sizeof(interfaces));
				SEA_LOG("[Layout]   +%03X = %08X%s", static_cast<unsigned>(offset), value, interfaces);
			}
		}

		// Returns "3D" for a buffer with a 3D interface and "2D" without. Returns "?" if not a DirectSound object.
		const char* DescribeBufferKind(std::uint32_t buffer) {
			if (!audio::IsDSoundObject(buffer)) {
				return "?";
			}

			if (audio::Supports(reinterpret_cast<IUnknown*>(buffer), IID_IDirectSound3DBuffer)) {
				return "3D";
			}

			return "2D";
		}

		// Logs one `[Play]` line with flags, environment, attenuation, buffer kind, route, sound form and path.
		void LogPlay(void* sound, bool loop, const char* route) {
			const std::uint32_t flags = Field<std::uint32_t>(sound, engine::kSound_TypeFlags);
			char flagText[160];
			engine::DescribeSoundFlags(flags, flagText, sizeof(flagText));

			char sourceText[96] = "-";
			void* sourceSound = engine::GetSourceSoundChecked(sound);

			if (sourceSound) {
				std::snprintf(sourceText, sizeof(sourceText), "%08X '%s'", engine::GetFormID(sourceSound),
					engine::GetEditorID(sourceSound));
			}

			const std::uint32_t environment = Field<std::uint32_t>(sound, engine::kSound_EnvironmentType);
			const std::uint32_t buffer = Field<std::uint32_t>(sound, engine::kWin32Sound_Buffer);

			SEA_LOG("[Play] %p ID=%u Loop=%d Flags=%08X [%s] Env=%u(%s) Att=%u/%u Buf=%s Route=%s Sound=%s Path=\"%.200s\"",
				sound, Field<std::uint32_t>(sound, engine::kSound_ID), loop, flags, flagText, environment,
				engine::EnvironmentTypeName(environment), Field<std::uint16_t>(sound, engine::kSound_StaticAttenuation),
				Field<std::uint16_t>(sound, engine::kSound_ReverbAttenuation), DescribeBufferKind(buffer), route, sourceText,
				&Field<char>(sound, engine::kSound_FilePath));
		}
	}

	// Arms the layout probe for the first `[Debug] iLayoutProbeCount` sounds. Called once after `config::Load`.
	void InitializeSoundProbe() {
		g_layoutProbesLeft = static_cast<int>(config::Get().debug.layoutProbeCount);
	}

	// Logs a played sound (`[Debug] bLogSoundPlay`) and scans the first sounds for DirectSound objects.
	// Called after the original `Play`.
	//
	// Thread: Audio
	void OnSoundPlayed(void* sound, bool loop, const char* route) {
		// A race can make the count negative. Harmless.
		if (g_layoutProbesLeft.load(std::memory_order_relaxed) > 0 && g_layoutProbesLeft.fetch_sub(1) > 0) {
			ProbeLayout(sound);
		}

		if (config::Get().debug.logSoundPlay) {
			LogPlay(sound, loop, route);
		}
	}

	// Logs a change of the engine's environment type on a sound (`[Debug] bLogSoundEnvironment`).
	// Called after the original `SetEnvironmentType`. Engine calls it from several threads.
	//
	// Thread: Any
	void OnEnvironmentTypeChanged(void* sound, std::uint32_t previous, std::uint32_t type) {
		if (!config::Get().debug.logSoundEnvironment || previous == type) {
			return;
		}

		SEA_LOG("[Env] %p ID=%u %u(%s) -> %u(%s) Path=\"%.200s\"", sound, Field<std::uint32_t>(sound, engine::kSound_ID),
			previous, engine::EnvironmentTypeName(previous), type, engine::EnvironmentTypeName(type),
			&Field<char>(sound, engine::kSound_FilePath));
	}
}
