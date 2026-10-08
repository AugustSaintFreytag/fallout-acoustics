#include "engine/scene.h"

#include "engine/addresses.h"
#include "utils/memory.h"

#include <cstdint>

namespace sea::engine {

	using mem::Field;

	namespace {

		constexpr int kMaxParentDepth = 64;

	}

	// Reads position and view direction (unit length) of the main camera. 
	// Returns false if called before the scene graph exists.
	//
	// Thread: Main
	bool GetCameraTransform(Vector3& position, Vector3& forward) {
		void* sceneGraph = mem::Global<void*>(kSceneGraphSingleton);

		if (!sceneGraph) {
			return false;
		}

		void* camera = Field<void*>(sceneGraph, kSceneGraph_Camera);

		if (!camera) {
			return false;
		}

		// The camera looks along its local X axis, column 0 of the world rotation.
		const float* rotation = &Field<float>(camera, kNiObject_WorldRotate);

		position = Field<Vector3>(camera, kNiObject_WorldTranslate);
		forward = Normalize({rotation[0], rotation[3], rotation[6]});

		return true;
	}

	// Returns the reference that owns a scene graph object. 
	// Returns null if there is none, for example for terrain.
	//
	// Walks up its parents to the first `BSFadeNode` that has a reference.
	void* GetParentReference(void* niObject) {
		void* node = niObject;

		for (int depth = 0; node && depth < kMaxParentDepth; ++depth) {
			const std::uintptr_t vtable = Field<std::uintptr_t>(node, 0);

			if (vtable == kVtbl_BSFadeNode) {
				void* reference = Field<void*>(node, kFadeNode_Reference);

				if (reference) {
					return reference;
				}
			}

			node = Field<void*>(node, kNiObject_Parent);
		}

		return nullptr;
	}

}
