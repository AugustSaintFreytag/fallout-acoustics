#include "debug/ray_probe.h"

#include "config/settings.h"
#include "engine/havok.h"
#include "engine/objects.h"
#include "engine/scene.h"
#include "engine/ui.h"
#include "utils/log.h"
#include "utils/timing.h"
#include "utils/vector3.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace sea::debug {

	namespace {

		constexpr int kMaxHitsPerDirection = 6;

		// Distance in game units to skip past a hit before next cast.
		constexpr float kStepPastHit = 1.0f;

		struct CastStats {
			int casts = 0;
			double totalMicroseconds = 0.0;
			double maxMicroseconds = 0.0;
		};

		// Writes reference, base form and model of a hit to given buffer.
		void DescribeHitObject(const engine::RaycastResult& hit, char* buffer, std::size_t size) {
			if (!hit.reference) {
				std::snprintf(buffer, size, "no reference (terrain?) Object=%p", hit.object);

				return;
			}

			void* baseForm = engine::GetBaseForm(hit.reference);
			const std::uint8_t baseType = engine::GetFormType(baseForm);

			std::snprintf(buffer, size, "Ref=%08X '%s' Base=%s(%02X) %08X '%s' Model=\"%s\"",
				engine::GetFormID(hit.reference), engine::GetEditorID(hit.reference), engine::FormTypeName(baseType),
				baseType, engine::GetFormID(baseForm), engine::GetEditorID(baseForm), engine::GetModelPath(baseForm));
		}

		// Casts one ray and adds its time to given stats.
		engine::RaycastResult TimedCast(const Vector3& from, const Vector3& to, std::uint8_t layer, CastStats& stats,
			double& microseconds) {
			const std::int64_t startTime = timing::Now();
			const engine::RaycastResult hit = engine::CastRay(from, to, layer);
			microseconds = timing::TicksToMicroseconds(static_cast<double>(timing::Now() - startTime));

			++stats.casts;
			stats.totalMicroseconds += microseconds;
			stats.maxMicroseconds = std::max(stats.maxMicroseconds, microseconds);

			return hit;
		}

		// Casts between two points and again from just past each hit. Logs each hit.
		// Distances in the log are from the camera to compare forward and back hits.
		void CastThrough(const char* direction, const Vector3& camera, const Vector3& from, const Vector3& to,
			std::uint8_t layer, CastStats& stats) {
			const Vector3 step = Normalize(to - from) * kStepPastHit;
			const float totalLength = Distance(from, to);
			Vector3 start = from;

			for (int hitIndex = 0; hitIndex < kMaxHitsPerDirection; ++hitIndex) {
				double microseconds = 0.0;
				const engine::RaycastResult hit = TimedCast(start, to, layer, stats, microseconds);

				if (!hit.hit) {
					SEA_LOG("[Ray]   %-7s end after %d hit(s) (%.1f us)", direction, hitIndex, microseconds);

					return;
				}

				char objectText[320];
				DescribeHitObject(hit, objectText, sizeof(objectText));

				SEA_LOG("[Ray]   %-7s #%d at %7.1f, Layer %u (%s), %.1f us, %s", direction, hitIndex + 1,
					Distance(camera, hit.point), hit.layer, engine::CollisionLayerName(hit.layer), microseconds, objectText);

				start = hit.point + step;

				if (Distance(from, start) >= totalLength) {
					return;
				}
			}

			SEA_LOG("[Ray]   %-7s stopped at %d hits", direction, kMaxHitsPerDirection);
		}

	}

	// Casts rays from the camera along its view and logs hits. One pass per layer in `[Debug] sRayProbeLayers`.
	//
	// Each pass goes forward to `fRayProbeRange` and back to the camera. It continues past each hit.
	// Logs distance, layer, reference, base form and model of each hit. Also logs time of each cast.
	//
	// Thread: Main
	void RunRayProbe() {
		const config::DebugSettings& settings = config::Get().debug;

		Vector3 camera;
		Vector3 forward;

		if (!engine::GetCameraTransform(camera, forward)) {
			SEA_LOG("[Ray] No camera.");

			return;
		}

		const Vector3 end = camera + forward * settings.rayProbeRange;

		SEA_LOG("[Ray] Probe from (%.1f, %.1f, %.1f), direction (%.3f, %.3f, %.3f), range %.0f", camera.x, camera.y,
			camera.z, forward.x, forward.y, forward.z, settings.rayProbeRange);

		for (const std::uint8_t layer : settings.rayProbeLayers) {
			SEA_LOG("[Ray] Ray layer %u (%s):", layer, engine::CollisionLayerName(layer));

			CastStats stats;
			CastThrough("Forward", camera, camera, end, layer, stats);
			CastThrough("Back", camera, end, camera, layer, stats);

			SEA_LOG("[Ray]   %d casts, Avg %.1f us, Max %.1f us", stats.casts,
				stats.totalMicroseconds / std::max(stats.casts, 1), stats.maxMicroseconds);
		}

		engine::ShowNotification("Ray probe: see log");
	}

}
