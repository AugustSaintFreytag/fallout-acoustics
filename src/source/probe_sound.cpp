#include "probe_sound.h"

#include "dsound_util.h"
#include "game.h"
#include "log.h"
#include "memory.h"
#include "reverb.h"

#include <atomic>
#include <cstdio>

namespace sea::probe {
	using mem::Field;

	namespace {
		using PlayFn = bool(__thiscall*)(void* sound, bool loop);
		using SetEnvironmentTypeFn = void(__thiscall*)(void* sound, std::uint32_t type);

		PlayFn g_originalPlay = nullptr;
		SetEnvironmentTypeFn g_originalSetEnvironmentType = nullptr;
		SoundProbeConfig g_config;
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

		void DescribeDSoundObject(IUnknown* object, char* buffer, std::size_t size) {
			buffer[0] = '\0';

			std::size_t usedLength = 0;

			for (const auto& [iid, name] : kInterfaceNames) {
				if (!ds::Supports(object, *iid)) {
					continue;
				}

				const int written = std::snprintf(buffer + usedLength, size - usedLength, " %s", name);

				if (written < 0 || usedLength + written >= size) {
					return;
				}

				usedLength += written;
			}
		}

		void ProbeLayout(void* sound) {
			SEA_LOG("[Layout] Sound %p: DirectSound objects in +%03X..+%03X", sound,
				static_cast<unsigned>(game::kWin32Sound_ProbeBegin), static_cast<unsigned>(game::kWin32Sound_ProbeEnd));

			for (std::uintptr_t offset = game::kWin32Sound_ProbeBegin; offset < game::kWin32Sound_ProbeEnd; offset += 4) {
				const std::uint32_t value = Field<std::uint32_t>(sound, offset);

				if (!ds::IsDSoundObject(value)) {
					continue;
				}

				char interfaces[96];
				DescribeDSoundObject(reinterpret_cast<IUnknown*>(value), interfaces, sizeof(interfaces));
				SEA_LOG("[Layout]   +%03X = %08X%s", static_cast<unsigned>(offset), value, interfaces);
			}
		}

		const char* DescribeBufferKind(std::uint32_t buffer) {
			if (!ds::IsDSoundObject(buffer)) {
				return "?";
			}

			if (ds::Supports(reinterpret_cast<IUnknown*>(buffer), IID_IDirectSound3DBuffer)) {
				return "3D";
			}

			return "2D";
		}

		void LogPlay(void* sound, bool loop, const char* route) {
			const std::uint32_t flags = Field<std::uint32_t>(sound, game::kSound_TypeFlags);
			char flagText[160];
			game::DescribeSoundFlags(flags, flagText, sizeof(flagText));

			char sourceText[96] = "-";
			void* sourceSound = game::GetSourceSoundChecked(sound);

			if (sourceSound) {
				std::snprintf(sourceText, sizeof(sourceText), "%08X '%s'", game::GetFormID(sourceSound),
					game::GetEditorID(sourceSound));
			}

			const std::uint32_t environment = Field<std::uint32_t>(sound, game::kSound_EnvironmentType);
			const std::uint32_t buffer = Field<std::uint32_t>(sound, game::kWin32Sound_Buffer);

			SEA_LOG("[Play] %p ID=%u Loop=%d Flags=%08X [%s] Env=%u(%s) Att=%u/%u Buf=%s Route=%s Sound=%s Path=\"%.200s\"",
				sound, Field<std::uint32_t>(sound, game::kSound_ID), loop, flags, flagText, environment,
				game::EnvironmentTypeName(environment), Field<std::uint16_t>(sound, game::kSound_StaticAttenuation),
				Field<std::uint16_t>(sound, game::kSound_ReverbAttenuation), DescribeBufferKind(buffer), route, sourceText,
				&Field<char>(sound, game::kSound_FilePath));
		}

		bool __fastcall Hook_Play(void* sound, void* /*edx*/, bool loop) {
			// Route before the original `Play` call, let first samples go to reverb immediately.
			const char* route = reverb::OnSoundPlay(sound);
			const bool result = g_originalPlay(sound, loop);

			// A race can make the count negative. This is safe.
			if (g_layoutProbesLeft.load(std::memory_order_relaxed) > 0 && g_layoutProbesLeft.fetch_sub(1) > 0) {
				ProbeLayout(sound);
			}

			if (g_config.logPlay) {
				LogPlay(sound, loop, route);
			}

			return result;
		}

		void __fastcall Hook_SetEnvironmentType(void* sound, void* /*edx*/, std::uint32_t type) {
			const std::uint32_t previous = Field<std::uint32_t>(sound, game::kSound_EnvironmentType);
			g_originalSetEnvironmentType(sound, type);

			if (!g_config.logEnvironmentChange || previous == type) {
				return;
			}

			SEA_LOG("[Env] %p ID=%u %u(%s) -> %u(%s) Path=\"%.200s\"", sound, Field<std::uint32_t>(sound, game::kSound_ID),
				previous, game::EnvironmentTypeName(previous), type, game::EnvironmentTypeName(type),
				&Field<char>(sound, game::kSound_FilePath));
		}
	}

	bool InstallSoundHooks(const SoundProbeConfig& config) {
		g_config = config;
		g_layoutProbesLeft = static_cast<int>(config.layoutProbeCount);

		const std::uintptr_t vtable = game::kVtbl_BSWin32GameSound;

		g_originalPlay = static_cast<PlayFn>(
			mem::PatchPointer(vtable + game::kSoundVtbl_Play, reinterpret_cast<void*>(&Hook_Play)));
		g_originalSetEnvironmentType = static_cast<SetEnvironmentTypeFn>(
			mem::PatchPointer(vtable + game::kSoundVtbl_SetEnvironmentType, reinterpret_cast<void*>(&Hook_SetEnvironmentType)));

		SEA_LOG("Hooks: BSWin32GameSound::Play %p -> %p, SetEnvironmentType %p -> %p, dsound.dll %08X..%08X",
			g_originalPlay, &Hook_Play, g_originalSetEnvironmentType, &Hook_SetEnvironmentType,
			static_cast<unsigned>(ds::DSoundBegin()), static_cast<unsigned>(ds::DSoundEnd()));

		return g_originalPlay && g_originalSetEnvironmentType;
	}
}
