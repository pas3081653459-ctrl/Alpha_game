#pragma once

#include <algorithm>
#include <cmath>

namespace AlphaPresentation
{
// The camera uses a horizontal FOV of 55 degrees. Include jump/depth space on
// ultrawide displays and keep a predictable minimum view for solo play.
inline double CameraDistance(double PlayerSpan, double AspectRatio)
{
    if (!std::isfinite(PlayerSpan) || !std::isfinite(AspectRatio)) return 2800.0;
    const double Span = std::clamp(PlayerSpan, 0.0, 2800.0);
    const double Aspect = std::clamp(AspectRatio, 0.5, 4.0);
    const double Width = std::max({2200.0, Span + 650.0, 1000.0 * Aspect});
    constexpr double TanHalfFOV = 0.5205670505517462;
    return std::clamp(Width / (2.0 * TanHalfFOV), 2300.0, 3800.0);
}

inline double BlendWeight(double DeltaSeconds, double Speed)
{
    if (!std::isfinite(DeltaSeconds) || !std::isfinite(Speed) || DeltaSeconds <= 0.0 || Speed <= 0.0) return 0.0;
    return 1.0 - std::exp(-std::min(DeltaSeconds, 1.0) * Speed);
}
inline double RouteCameraDistance(double HalfWidth, double HalfDepth, double HeightSpan, double AspectRatio)
{
    if (!std::isfinite(HalfWidth) || !std::isfinite(HalfDepth) || !std::isfinite(HeightSpan)
        || !std::isfinite(AspectRatio)) return 2800.;
    const double Width = std::clamp(HalfWidth, 0., 5000.);
    const double Depth = std::clamp(HalfDepth, 0., 2000.);
    const double Height = std::clamp(HeightSpan, 0., 2000.);
    const double Aspect = std::clamp(AspectRatio, .5, 4.);
    const double VerticalSpan = Depth * .726 + std::max(800., Height + 550.);
    const double FramedWidth = std::max(Width * 2. + 650., VerticalSpan * Aspect);
    constexpr double TanHalfFOV = .5205670505517462;
    return std::clamp(std::max(CameraDistance(Width * 2., Aspect), FramedWidth / (2. * TanHalfFOV)), 2300., 5200.);
}

inline double EffectEnvelope(double Age, double Lifetime)
{
    if (!std::isfinite(Age) || !std::isfinite(Lifetime) || Lifetime <= 0.0) return 0.0;
    const double Remaining = 1.0 - std::clamp(Age / Lifetime, 0.0, 1.0);
    return Remaining * Remaining;
}
// Normalized lifetime ballistic fragment height. Fragments stop above their
// confirmed contact plane and never become simulated collision geometry.
inline double DebrisHeight(double Start, double Lift, double Gravity, double Progress)
{
    if (!std::isfinite(Start) || !std::isfinite(Lift) || !std::isfinite(Gravity)
        || !std::isfinite(Progress)) return 2.0;
    const double T = std::clamp(Progress, 0.0, 1.0);
    return std::clamp(std::clamp(Start, 2.0, 600.0) + std::clamp(Lift, 0.0, 600.0) * T
        - std::clamp(Gravity, 0.0, 1200.0) * T * T, 2.0, 1200.0);
}

}
