#include "Globals.h"
#include "AnimationController.h"
#include "ResourceAnimation.h"
#include "ModuleResources.h"
#include "Application.h"
#include <algorithm>
#include <cmath>

AnimationController::~AnimationController() {
    if (m_animation) {
        app->getResources()->ReleaseResource(m_animation);
        m_animation = nullptr;
    }
}

void AnimationController::Play(UID uid, bool loop) {
    // Release previous animation only when switching to a different one.
    if (m_animation && Resource != uid) {
        app->getResources()->ReleaseResource(m_animation);
        m_animation = nullptr;
    }

    Resource    = uid;
    Loop        = loop;
    CurrentTime = 0.f;
    m_playing   = true;

    if (!m_animation)
        m_animation = app->getResources()->RequestAnimation(uid);

    if (!m_animation) {
        LOG("AnimationController: Failed to load animation uid=%llu", uid);
        m_playing = false;
    }
}

void AnimationController::Stop() {
    m_playing   = false;
    CurrentTime = 0.f;
}

void AnimationController::Update(float deltaTime) {
    if (!m_playing || !m_animation) return;

    CurrentTime += deltaTime;

    const float duration = m_animation->getDuration();
    if (duration <= 0.f) return;

    if (CurrentTime >= duration) {
        if (Loop)
            CurrentTime = std::fmod(CurrentTime, duration);
        else {
            CurrentTime = duration;
            m_playing   = false;
        }
    }
}

bool AnimationController::GetTransform(const char* name, Vector3& pos, Quaternion& rot) const {
    if (!m_animation) return false;

    const auto& channels = m_animation->getChannels();
    const auto  it       = channels.find(name);
    if (it == channels.end()) return false;

    const ResourceAnimation::Channel& ch = it->second;

    // ---- Position ----
    if (ch.posCount > 0) {
        const float* tFirst = ch.posTimeStamps.get();
        const float* tLast  = tFirst + ch.posCount;

        // First timestamp strictly greater than CurrentTime.
        const float* upper = std::upper_bound(tFirst, tLast, CurrentTime);

        if (upper == tFirst) {
            // Before the first keyframe — clamp to start.
            pos = ch.positions[0];
        } else if (upper == tLast) {
            // After the last keyframe — clamp to end.
            pos = ch.positions[ch.posCount - 1];
        } else {
            // Keyframes bracket CurrentTime: [i] <= CurrentTime < [i+1].
            int   i      = (int)(upper - tFirst) - 1;
            float t0     = tFirst[i];
            float t1     = tFirst[i + 1];
            float denom  = t1 - t0;
            float lambda = denom > 0.f ? (CurrentTime - t0) / denom : 0.f;
            pos = Vector3::Lerp(ch.positions[i], ch.positions[i + 1], lambda);
        }
    }

    // ---- Rotation ----
    if (ch.rotCount > 0) {
        const float* tFirst = ch.rotTimeStamps.get();
        const float* tLast  = tFirst + ch.rotCount;

        const float* upper = std::upper_bound(tFirst, tLast, CurrentTime);

        if (upper == tFirst) {
            rot = ch.rotations[0];
        } else if (upper == tLast) {
            rot = ch.rotations[ch.rotCount - 1];
        } else {
            int   i      = (int)(upper - tFirst) - 1;
            float t0     = tFirst[i];
            float t1     = tFirst[i + 1];
            float denom  = t1 - t0;
            float lambda = denom > 0.f ? (CurrentTime - t0) / denom : 0.f;
            rot = Quaternion::Lerp(ch.rotations[i], ch.rotations[i + 1], lambda);
        }
    }

    return true;
}
