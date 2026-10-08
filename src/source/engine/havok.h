#pragma once

#include "utils/vector3.h"

#include <cstdint>

namespace sea::engine {

	// Havok collision layers (JIP `CollisionLayerTypes`).
	enum CollisionLayer : std::uint8_t {
		kLayer_Static = 1,
		kLayer_AnimStatic = 2,
		kLayer_Transparent = 3,
		kLayer_Clutter = 4,
		kLayer_Weapon = 5,
		kLayer_Projectile = 6,
		kLayer_Biped = 8,
		kLayer_Trees = 9,
		kLayer_Props = 10,
		kLayer_Terrain = 13,
		kLayer_DebrisSmall = 19,
		kLayer_DebrisLarge = 20,
		kLayer_ShellCasing = 25,
		kLayer_TransparentSmall = 26,
		kLayer_TransparentSmallAnim = 28,
		kLayer_DeadBip = 29,
		kLayer_CharController = 30,
		kLayer_CameraPick = 35,
		kLayer_LineOfSight = 37,
		kLayer_PathPick = 38,
	};

	const char* CollisionLayerName(std::uint8_t layer);

	// Result properties of a raycast.
	struct RaycastResult {
		bool hit = false;
		float fraction = 1.0f;  // Position of the hit along the ray, 0..1
		Vector3 point;  // Game units
		void* object = nullptr;  // NiAVObject*
		void* reference = nullptr;  // TESObjectREFR*, null for terrain and objects without a reference
		std::uint8_t layer = 0;  // Collision layer of the object that was hit
	};

	RaycastResult CastRay(const Vector3& from, const Vector3& to, std::uint8_t layer);

	bool IsInsidePluginCast();

}
