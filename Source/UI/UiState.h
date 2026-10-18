// Key Compass - animated view state shared by the display layers
#pragma once

#include "../Engine/KeyEngine.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace kcui
{
    struct UiState
    {
        kc::EngineSnapshot snap;
        bool flats = false;
        kc::AnalysisMode mode = kc::AnalysisMode::Track;
        bool locked = false;

        float needleAngle = 0.0f, needleVelocity = 0.0f;      // radians, clockwise from 12 o'clock
        float wedgeAngle = 0.0f, wedgeVelocity = 0.0f;
        float wedgeAlpha = 0.0f;
        float keyGlow = 0.0f;
        float shownConfidence = 0.0f;
        float liveAlpha = 0.5f;                                 // 1 while music is playing, dimmer when holding
        float shownCents = 0.0f;
        std::array<float, 12> noteGlow {};
        float time = 0.0f;
        bool initialised = false;

        static float wrapAngle (float a) noexcept
        {
            constexpr float twoPi = 6.28318530718f;
            a = std::fmod (a + 3.14159265359f, twoPi);
            if (a < 0) a += twoPi;
            return a - 3.14159265359f;
        }

        static float tonicAngle (const kc::KeyId& k) noexcept
        {
            return (float) kc::circlePosition (k.tonic) * 0.52359877559f;   // 30 degrees per step
        }

        // Hash of everything in a snapshot that is visible on screen
        static uint64_t signature (const kc::EngineSnapshot& s) noexcept
        {
            uint64_t h = 1469598103934665603ull;
            auto mix = [&h] (long long v) { h ^= (uint64_t) v; h *= 1099511628211ull; };
            mix (s.hasKey); mix (s.key.index()); mix (std::lround (s.confidence * 400.0f));
            mix (s.showRunnerUp); mix (s.runnerUp.index()); mix (s.signalPresent); mix (s.sourceIsMidi);
            mix (s.chord.root); mix ((int) s.chord.type); mix (s.chord.bass); mix (s.chord.isSingleNote);
            mix (s.monoVoiced); mix (s.monoNote); mix (std::lround (s.monoCents)); mix (std::lround (s.monoFrequency * 10.0f));
            mix (s.historySize);
            if (s.historySize > 0)
            {
                const auto& e = s.history[(size_t) s.historySize - 1];
                mix ((int) e.kind); mix (e.chord.root); mix ((int) e.chord.type); mix (e.midiNote); mix (e.key.index());
            }
            for (auto v : s.noteLevels) mix (std::lround (v * 20.0f));
            return h;
        }

        // Advances the animation; returns true if anything visible changed
        bool advance (const kc::EngineSnapshot& s, float dt)
        {
            const uint64_t sig = signature (s);
            const bool newData = sig != lastSignature || s.signalPresent;
            lastSignature = sig;
            snap = s;
            time += dt;

            float needleTarget;
            if (snap.hasKey)
                needleTarget = tonicAngle (snap.key);
            else if (snap.signalPresent)
                needleTarget = 0.45f * std::sin (1.1f * time) + 0.18f * std::sin (2.9f * time);   // searching...
            else
                needleTarget = 0.0f;

            if (! initialised)
            {
                needleAngle = needleTarget;
                wedgeAngle = snap.hasKey ? needleTarget + (snap.key.minor ? -0.5236f : 1.0472f) : 0.0f;
                wedgeAlpha = snap.hasKey ? 1.0f : 0.0f;
                keyGlow = wedgeAlpha;
                shownConfidence = snap.confidence;
                initialised = true;
            }

            const float before = needleAngle + wedgeAngle * 3.1f + wedgeAlpha * 7.0f + keyGlow * 11.0f
                                 + shownConfidence * 13.0f + liveAlpha * 17.0f + shownCents * 0.1f;
            float glowSum = 0.0f;

            // needle: damped spring (a little overshoot like a real compass)
            {
                const float diff = wrapAngle (needleTarget - needleAngle);
                const float acc = 40.0f * diff - 7.0f * needleVelocity;
                needleVelocity += acc * dt;
                needleAngle = wrapAngle (needleAngle + needleVelocity * dt);
            }

            if (snap.hasKey)
            {
                const float wedgeTarget = tonicAngle (snap.key) + (snap.key.minor ? -0.5236f : 1.0472f);
                const float diff = wrapAngle (wedgeTarget - wedgeAngle);
                const float acc = 22.0f * diff - 8.5f * wedgeVelocity;
                wedgeVelocity += acc * dt;
                wedgeAngle = wrapAngle (wedgeAngle + wedgeVelocity * dt);
            }

            auto approach = [dt] (float& v, float target, float tau)
            {
                v += (target - v) * (1.0f - std::exp (-dt / tau));
            };

            approach (wedgeAlpha, snap.hasKey ? 1.0f : 0.0f, 0.25f);
            approach (keyGlow, snap.hasKey ? 1.0f : 0.0f, 0.4f);
            approach (shownConfidence, snap.hasKey ? snap.confidence : 0.0f, 0.25f);
            approach (liveAlpha, snap.signalPresent ? 1.0f : 0.55f, 0.3f);
            approach (shownCents, snap.monoCents, 0.08f);

            for (size_t i = 0; i < 12; ++i)
            {
                const float target = snap.noteLevels[i];
                approach (noteGlow[i], target, target > noteGlow[i] ? 0.03f : 0.25f);
                glowSum += noteGlow[i] * (float) (i + 1);
            }

            const float after = needleAngle + wedgeAngle * 3.1f + wedgeAlpha * 7.0f + keyGlow * 11.0f
                                + shownConfidence * 13.0f + liveAlpha * 17.0f + shownCents * 0.1f;

            const bool moving = std::abs (after - before) > 1.0e-4f || std::abs (needleVelocity) > 1.0e-3f
                                || std::abs (glowSum - lastGlowSum) > 1.0e-4f;
            lastGlowSum = glowSum;
            return newData || moving;
        }

    private:
        float lastGlowSum = 0.0f;
        uint64_t lastSignature = 0;
    };
}
