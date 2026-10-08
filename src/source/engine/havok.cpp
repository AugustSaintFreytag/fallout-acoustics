#include "engine/havok.h"

#include "engine/addresses.h"
#include "engine/objects.h"
#include "engine/scene.h"
#include "utils/log.h"
#include "utils/memory.h"

#include <Windows.h>

#include <cstddef>
#include <cstring>
#include <iterator>

namespace sea::engine {

	using mem::Field;

	namespace {
		// Pick data collection used for a Havok physics engine ray (like `bhkPickData`). 
		// Unnamed fields are set as JIP's `_GetRayCastObject` sets them.

		struct alignas(16) PickProperties {
			float from[4];  // 00, Havok units
			float to[4];  // 10, Havok units
			std::uint8_t byte20;  // 20
			std::uint8_t pad21[3];  // 21
			std::uint32_t filterInfo;  // 24: layer (byte), filter flags (byte), group (word)
			std::uint32_t unk28[6];  // 28
			float hitFraction;  // 40, 1 = no hit
			std::int32_t unk44[15];  // 44
			void* cdBody;  // 80, hkCdBody of the hit
			std::uint32_t unk84[3];  // 84
			float vector90[4];  // 90
			std::uint32_t unkA0[3];  // A0
			std::uint8_t byteAC;  // AC
			std::uint8_t padAD[3];  // AD
		};

		static_assert(sizeof(PickProperties) == 0xB0);
		static_assert(offsetof(PickProperties, filterInfo) == 0x24);
		static_assert(offsetof(PickProperties, hitFraction) == 0x40);
		static_assert(offsetof(PickProperties, cdBody) == 0x80);

		using PickObjectFn = void*(__thiscall*)(void* tes, PickProperties* pickData, bool unknown);

		constexpr std::uint32_t kFilterGroupMask = 0xFFFF0000;
		constexpr int kMaxCdBodyDepth = 16;

		const char* const kLayerNames[] = {
			"None", "Static", "AnimStatic", "Transparent", "Clutter", "Weapon", "Projectile", "Spell", "Biped",
			"Trees", "Props", "Water", "Trigger", "Terrain", "Trap", "NonCollidable", "CloudTrap", "Ground",
			"Portal", "DebrisSmall", "DebrisLarge", "AcousticSpace", "ActorZone", "ProjectileZone", "GasTrap",
			"ShellCasing", "TransparentSmall", "InvisibleWall", "TransparentSmallAnim", "DeadBip",
			"CharController", "AvoidBox", "CollisionBox", "CameraSphere", "DoorDetection", "CameraPick",
			"ItemPick", "LineOfSight", "PathPick", "CustomPick1", "CustomPick2", "SpellExplosion", "DroppingPick",
		};

		// Thread: Main
		bool g_pickFailureLogged = false;

		thread_local bool t_insidePluginCast = false;

		// Returns the collision group of the player's character controller. 
		// A ray with this group ignores the player.
		// Returns 0 if the chain is not available (e.g., before the player has loaded in).
		std::uint32_t PlayerCollisionGroup() {
			const std::uintptr_t chain[] = {
				kActor_BaseProcess,
				kProcess_CharController,
				kCharController_Unknown594,
				kHavokRef_Object,
			};

			std::uint32_t address = reinterpret_cast<std::uint32_t>(GetPlayer());

			for (const std::uintptr_t offset : chain) {
				if (address == 0 || !mem::SafeRead32(reinterpret_cast<void*>(address + offset), address)) {
					return 0;
				}
			}

			std::uint32_t filterInfo = 0;

			if (address == 0 || !mem::SafeRead32(reinterpret_cast<void*>(address + kWorldObject_FilterInfo), filterInfo)) {
				return 0;
			}

			return filterInfo & kFilterGroupMask;
		}

		// Runs an engine object pick with the given pick properties and returns hits, or null if nothing was hit.
		// Property `hitFraction` is less than 1 if the ray hit an object or geometry.
		// An exception in the engine is counted as no hit.
		void* PickObject(PickProperties* pickProperties) {
			void* tes = mem::Global<void*>(kTESSingleton);

			if (!tes) {
				return nullptr;
			}

			const auto pickObject = reinterpret_cast<PickObjectFn>(kTES_PickObject);

			__try {
				return pickObject(tes, pickProperties, true);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				pickProperties->hitFraction = 1.0f;

				return nullptr;
			}
		}

		// Returns the collision layer of a hit object. 
		// Returns 0 if unreadable.
		// Read from the root of its `hkCdBody` parents. 
		std::uint8_t HitLayer(void* cdBody) {
			std::uint32_t body = reinterpret_cast<std::uint32_t>(cdBody);

			for (int depth = 0; body != 0 && depth < kMaxCdBodyDepth; ++depth) {
				std::uint32_t parent = 0;

				if (!mem::SafeRead32(reinterpret_cast<void*>(body + kCdBody_Parent), parent)) {
					return 0;
				}

				if (parent == 0) {
					std::uint32_t layerWord = 0;

					if (!mem::SafeRead32(reinterpret_cast<void*>(body + kRootCdBody_Layer), layerWord)) {
						return 0;
					}

					return static_cast<std::uint8_t>(layerWord & 0xFF);
				}

				body = parent;
			}

			return 0;
		}
	}

	// Checks if raycasting runs on the calling thread. 
	// Hooks on the engine's pick function use it to skip plugin casts.
	//
	// Thread: Any
	bool IsInsidePluginCast() {
		return t_insidePluginCast;
	}

	// Returns the name of a collision layer for the log. 
	// Returns string "?" is returned for unknown values.
	const char* CollisionLayerName(std::uint8_t layer) {
		if (layer >= std::size(kLayerNames)) {
			return "?";
		}

		return kLayerNames[layer];
	}

	// Casts a ray from an origin in a direction (in game units) and returns the nearest hit. 
	// Casting automatically ignores the player's own collision geometry.
	//
	// Thread: Main
	RaycastResult CastRay(const Vector3& origin, const Vector3& direction, std::uint8_t layer) {
		PickProperties pickProperties;
		std::memset(&pickProperties, 0, sizeof(pickProperties));

		pickProperties.from[0] = origin.x * kHavokScale;
		pickProperties.from[1] = origin.y * kHavokScale;
		pickProperties.from[2] = origin.z * kHavokScale;
		pickProperties.to[0] = direction.x * kHavokScale;
		pickProperties.to[1] = direction.y * kHavokScale;
		pickProperties.to[2] = direction.z * kHavokScale;

		pickProperties.hitFraction = 1.0f;
		pickProperties.unk44[0] = -1;  // +0x44
		pickProperties.unk44[3] = -1;  // +0x50
		
		pickProperties.filterInfo = PlayerCollisionGroup() | (layer & 0x7F);

		t_insidePluginCast = true;
		void* object = PickObject(&pickProperties);
		t_insidePluginCast = false;

		RaycastResult result;

		if (pickProperties.hitFraction >= 1.0f) {
			return result;
		}

		if (pickProperties.hitFraction < 0.0f && !g_pickFailureLogged) {
			g_pickFailureLogged = true;
			SEA_LOG("[Havok] PickObject returned a negative hit fraction (%f).", pickProperties.hitFraction);
		}

		result.hit = true;
		result.fraction = pickProperties.hitFraction;
		result.point = origin + (direction - origin) * pickProperties.hitFraction;
		result.object = object;
		result.reference = GetParentReference(object);
		result.layer = HitLayer(pickProperties.cdBody);

		return result;
	}

}
