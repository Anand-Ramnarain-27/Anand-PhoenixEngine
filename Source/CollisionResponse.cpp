#include "Globals.h"
#include "CollisionResponse.h"
#include "ComponentRigidbody.h"
#include "ComponentTransform.h"
#include "GameObject.h"
#include <algorithm>
#include <cmath>

void CollisionResponse::solve(const std::vector<ContactPoint>& contacts, float /*dt*/) {
    for (const auto& c : contacts) {
        if (!c.a || !c.b) continue;

        ComponentRigidbody* rbA = c.a->getComponent<ComponentRigidbody>();
        ComponentRigidbody* rbB = c.b->getComponent<ComponentRigidbody>();

        // If neither object has a rigidbody, physics has nothing to act on.
        if (!rbA && !rbB) continue;

        const float invMassA  = rbA ? rbA->getInvMass() : 0.f;
        const float invMassB  = rbB ? rbB->getInvMass() : 0.f;
        const float invMassSum = invMassA + invMassB;

        // Both static — nothing moves.
        if (invMassSum < 1e-8f) continue;

        // ----------------------------------------------------------------
        // Pass 1 — Position correction
        // Baumgarte-style: correct only the portion beyond the slop,
        // distributed between A and B by their inverse masses.
        // ----------------------------------------------------------------
        const float correction = std::max(c.depth - correctionSlop, 0.f)
                                 * correctionPercent / invMassSum;

        ComponentTransform* tA = c.a->getTransform();
        ComponentTransform* tB = c.b->getTransform();

        // normal points from B toward A, so A moves in +normal direction.
        if (tA && invMassA > 0.f) {
            tA->position += c.normal * (correction * invMassA);
            tA->markDirty();
        }
        if (tB && invMassB > 0.f) {
            tB->position -= c.normal * (correction * invMassB);
            tB->markDirty();
        }

        // ----------------------------------------------------------------
        // Pass 2 — Velocity impulse
        //
        // Impulse j = -(1 + e) * relVelAlongNormal / (1/mA + 1/mB)
        //
        // This generalises the 1D elastic formula
        //   v1f = ((m1-m2)*v1i + 2*m2*v2i) / (m1+m2)
        //   v2f = ((m2-m1)*v2i + 2*m1*v1i) / (m1+m2)
        // to any e ∈ [0,1] and arbitrary mass ratios, projected onto the
        // contact normal.
        // ----------------------------------------------------------------
        const Vector3 velA = rbA ? rbA->velocity : Vector3::Zero;
        const Vector3 velB = rbB ? rbB->velocity : Vector3::Zero;

        const float relVelAlongNormal = (velA - velB).Dot(c.normal);

        // Objects already separating along the normal — nothing to resolve.
        if (relVelAlongNormal > 0.f) continue;

        // Use the minimum restitution of the two surfaces so a rubber ball
        // bouncing off a concrete floor uses the ball's restitution.
        const float restitA = rbA ? rbA->restitution : 0.f;
        const float restitB = rbB ? rbB->restitution : 0.f;
        const float e       = std::min(restitA, restitB);

        const float j = -(1.f + e) * relVelAlongNormal / invMassSum;

        const Vector3 impulse = c.normal * j;

        if (rbA && invMassA > 0.f) rbA->velocity += impulse * invMassA;
        if (rbB && invMassB > 0.f) rbB->velocity -= impulse * invMassB;
    }
}
