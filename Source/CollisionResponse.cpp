#include "Globals.h"
#include "CollisionResponse.h"
#include "ComponentRigidbody.h"
#include "ComponentTransform.h"
#include "GameObject.h"
#include <algorithm>
#include <cmath>

static constexpr float kCorrectionPercent = 0.8f;

static constexpr float kCorrectionSlop = 0.005f;

void CollisionResponse::solve(const std::vector<ContactPoint>& contacts, float ){
    for (const auto& c : contacts){
        if (!c.a || !c.b) continue;

        ComponentRigidbody* rbA = c.a->getComponent<ComponentRigidbody>();
        ComponentRigidbody* rbB = c.b->getComponent<ComponentRigidbody>();

        if (!rbA && !rbB) continue;

        const float invMassA = rbA ? rbA->getInvMass() : 0.f;
        const float invMassB = rbB ? rbB->getInvMass() : 0.f;
        const float invMassSum = invMassA + invMassB;

        if (invMassSum < 1e-8f) continue;

        const float correction = std::max(c.depth - kCorrectionSlop, 0.f)
                                 * kCorrectionPercent / invMassSum;

        ComponentTransform* tA = c.a->getTransform();
        ComponentTransform* tB = c.b->getTransform();

        if (tA && invMassA > 0.f){
            tA->position += c.normal * (correction * invMassA);
            tA->markDirty();
        }
        if (tB && invMassB > 0.f){
            tB->position -= c.normal * (correction * invMassB);
            tB->markDirty();
        }

        const Vector3 velA = rbA ? rbA->velocity : Vector3::Zero;
        const Vector3 velB = rbB ? rbB->velocity : Vector3::Zero;

        const float relVelAlongNormal = (velA - velB).Dot(c.normal);

        if (relVelAlongNormal > 0.f) continue;

        const float restitA = rbA ? rbA->restitution : 0.f;
        const float restitB = rbB ? rbB->restitution : 0.f;
        const float e = std::min(restitA, restitB);

        const float j = -(1.f + e) * relVelAlongNormal / invMassSum;

        const Vector3 impulse = c.normal * j;

        if (rbA && invMassA > 0.f) rbA->velocity += impulse * invMassA;
        if (rbB && invMassB > 0.f) rbB->velocity -= impulse * invMassB;
    }
}
