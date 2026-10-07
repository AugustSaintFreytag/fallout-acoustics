cmake_minimum_required(VERSION 3.25)
project(SaintsAcoustics LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "FalloutNV is 32-bit: configure with -A Win32")
endif()

set(SEA_DEPLOY_DIR "F:/Games/Tale of Two Wastelands/mods/Saint's Experimental Acoustics"
	CACHE PATH "MO2 mod folder to deploy into (empty = no deploy)")

add_library(SaintsAcoustics SHARED
	source/main.cpp

	source/audio/directsound.cpp
	source/audio/eax_property.cpp

	source/config/settings.cpp

	source/debug/cell_probe.cpp
	source/debug/eax_readback.cpp
	source/debug/route_timing.cpp
	source/debug/sound_probe.cpp

	source/engine/environment.cpp
	source/engine/objects.cpp
	source/engine/sound_flags.cpp
	source/engine/ui.cpp

	source/hooks/sound_hooks.cpp

	source/input/hotkeys.cpp

	source/reverb/listener.cpp
	source/reverb/reverb.cpp
	source/reverb/routing.cpp

	source/utils/log.cpp
	source/utils/memory.cpp
)

target_include_directories(SaintsAcoustics PRIVATE include source)
target_compile_features(SaintsAcoustics PRIVATE cxx_std_20)
target_compile_definitions(SaintsAcoustics PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(SaintsAcoustics PRIVATE /W4 /permissive- /utf-8 /Zi)
target_link_options(SaintsAcoustics PRIVATE /DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF)
target_link_libraries(SaintsAcoustics PRIVATE dxguid psapi)

# Static CRT, so users do not need the VC++ redistributable.
set_property(TARGET SaintsAcoustics PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(SEA_DEPLOY_DIR)
	add_custom_command(TARGET SaintsAcoustics POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${SEA_DEPLOY_DIR}/nvse/plugins"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:SaintsAcoustics>" "${SEA_DEPLOY_DIR}/nvse/plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:SaintsAcoustics>" "${SEA_DEPLOY_DIR}/nvse/plugins/"
		COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/data/SaintsAcoustics.ini" "-DDST=${SEA_DEPLOY_DIR}/nvse/plugins/SaintsAcoustics.ini" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/deploy_ini.cmake"
		COMMENT "Deploying to ${SEA_DEPLOY_DIR}")
endif()
