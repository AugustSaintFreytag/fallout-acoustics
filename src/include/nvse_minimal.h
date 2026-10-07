#pragma once
// Layouts copied from xNVSE (nvse/PluginAPI.h, nvse_version.h).

#include <cstdint>

using PluginHandle = std::uint32_t;

// Constants

constexpr PluginHandle kPluginHandle_Invalid = 0xFFFFFFFF;

constexpr std::uint32_t MakeNewVegasVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t build) {
	return (major << 24) | (minor << 16) | (build << 4);
}

constexpr std::uint32_t RUNTIME_VERSION_1_4_0_525 = MakeNewVegasVersion(4, 0, 525);

// Library

enum NVSEInterfaceID : std::uint32_t {
	kInterface_Serialization = 0,
	kInterface_Console,
	kInterface_Messaging,
	kInterface_CommandTable,
	kInterface_StringVar,
	kInterface_ArrayVar,
	kInterface_Script,
	kInterface_Data,
	kInterface_EventManager,
	kInterface_Logging,
	kInterface_PlayerControls,
};

struct NVSEInterface {
	std::uint32_t nvseVersion;
	std::uint32_t runtimeVersion;
	std::uint32_t editorVersion;
	std::uint32_t isEditor;

	void* RegisterCommand;
	void* SetOpcodeBase;
	void* (*QueryInterface)(std::uint32_t id);
	
	PluginHandle (*GetPluginHandle)();
	void* RegisterTypedCommand;
	
	const char* (*GetRuntimeDirectory)();
	std::uint32_t isNogore;
	
	// NVSE has more members on the interface.
};

struct NVSEMessagingInterface {
	struct Message {
		const char* sender;
		std::uint32_t type;
		std::uint32_t dataLen;
		void* data;
	};

	using EventCallback = void (*)(Message* message);

	enum : std::uint32_t {
		kMessage_PostLoad,
		kMessage_ExitGame,
		kMessage_ExitToMainMenu,
		kMessage_LoadGame,
		kMessage_SaveGame,
		kMessage_ScriptPrecompile,
		kMessage_PreLoadGame,
		kMessage_ExitGame_Console,
		kMessage_PostLoadGame,
		kMessage_PostPostLoad,
		kMessage_RuntimeScriptError,
		kMessage_DeleteGame,
		kMessage_RenameGame,
		kMessage_RenameNewGame,
		kMessage_NewGame,
		kMessage_DeleteGameName,
		kMessage_RenameGameName,
		kMessage_RenameNewGameName,
		kMessage_DeferredInit,
		kMessage_ClearScriptDataCache,
		kMessage_MainGameLoop,
	};

	std::uint32_t version;

	bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
	bool (*Dispatch)(PluginHandle sender, std::uint32_t messageType, void* data, std::uint32_t dataLen, const char* receiver);
};

struct PluginInfo {
	enum { kInfoVersion = 1 };

	std::uint32_t infoVersion;
	const char* name;
	std::uint32_t version;
};
