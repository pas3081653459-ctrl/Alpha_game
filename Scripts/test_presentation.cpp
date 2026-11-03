#include "Rules/PresentationRules.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
int Checks = 0;
int Failures = 0;

void Check(bool Condition, const std::string& Label)
{
    ++Checks;
    if (!Condition)
    {
        ++Failures;
        std::cerr << "FAIL: " << Label << '\n';
    }
}

bool Near(double Left, double Right, double Tolerance = 1.e-10)
{
    return std::abs(Left - Right) <= Tolerance;
}

void CameraTests()
{
    using AlphaPresentation::CameraDistance;
    constexpr double Maximum = std::numeric_limits<double>::max();
    Check(Near(CameraDistance(0, 16.0 / 9), 2300), "solo standard aspect has predictable minimum distance");
    Check(Near(CameraDistance(0, 4.0 / 3), 2300), "solo narrow aspect respects minimum distance");
    Check(CameraDistance(1800, 16.0 / 9) > CameraDistance(0, 16.0 / 9), "spread players widen framing");
    Check(CameraDistance(2800, 16.0 / 9) > CameraDistance(1800, 16.0 / 9), "greater player spread increases distance");
    Check(CameraDistance(0, 32.0 / 9) > CameraDistance(0, 16.0 / 9), "ultrawide preserves more vertical space");
    Check(Near(CameraDistance(-10, 16.0 / 9), CameraDistance(0, 16.0 / 9)), "negative player span clamps to zero");
    Check(Near(CameraDistance(Maximum, 16.0 / 9), CameraDistance(2800, 16.0 / 9)), "finite huge player span clamps safely");
    Check(Near(CameraDistance(-Maximum, 16.0 / 9), CameraDistance(0, 16.0 / 9)), "finite negative huge player span clamps safely");
    Check(Near(CameraDistance(1000, Maximum), CameraDistance(1000, 4)), "extreme positive aspect clamps to four");
    Check(Near(CameraDistance(1000, -Maximum), CameraDistance(1000, .5)), "extreme negative aspect clamps to half");
    for (double Aspect : {.5, 1.0, 4.0 / 3, 16.0 / 9, 21.0 / 9, 32.0 / 9, 4.0})
    {
        double Previous = CameraDistance(0, Aspect);
        for (double Span : {0.0, 100.0, 800.0, 1800.0, 2800.0, Maximum})
        {
            const double Distance = CameraDistance(Span, Aspect);
            Check(std::isfinite(Distance) && Distance >= 2300 && Distance <= 3800,
                "camera remains finite within safe distance range");
            Check(Distance >= Previous, "camera distance never decreases as team spreads");
            Previous = Distance;
        }
    }
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()})
    {
        Check(Near(CameraDistance(Invalid, 16.0 / 9), 2800), "nonfinite span uses safe fallback");
        Check(Near(CameraDistance(1000, Invalid), 2800), "nonfinite aspect uses safe fallback");
    }
}

void BlendTests()
{
    using AlphaPresentation::BlendWeight;
    Check(Near(BlendWeight(0, 8), 0), "zero frame time leaves camera unchanged");
    Check(Near(BlendWeight(-1, 8), 0), "negative frame time cannot reverse camera");
    Check(Near(BlendWeight(.1, 0), 0), "zero speed disables blend");
    Check(Near(BlendWeight(.1, -1), 0), "negative speed disables blend");
    Check(Near(BlendWeight(.1, 8), 1 - std::exp(-.8)), "blend uses exponential decay");
    Check(Near(BlendWeight(10, 8), BlendWeight(1, 8)), "large frame stall clamps at one second");
    Check(Near(BlendWeight(std::numeric_limits<double>::max(), 8), BlendWeight(1, 8)), "extreme frame stall clamps safely");
    Check(Near(BlendWeight(.1, std::numeric_limits<double>::max()), 1), "extreme finite speed reaches target without overshoot");
    for (double Speed : {.01, 1.0, 8.0, 100.0})
    {
        for (double Delta : {.0001, 1.0 / 120, 1.0 / 60, 1.0 / 30, .1, 1.0, 10.0})
        {
            const double Weight = BlendWeight(Delta, Speed);
            Check(std::isfinite(Weight) && Weight >= 0 && Weight <= 1, "blend weight always finite and bounded");
        }
        double At60 = 0;
        double At30 = 0;
        for (int Frame = 0; Frame < 60; ++Frame) At60 += (100 - At60) * BlendWeight(1.0 / 60, Speed);
        for (int Frame = 0; Frame < 30; ++Frame) At30 += (100 - At30) * BlendWeight(1.0 / 30, Speed);
        Check(Near(At60, At30, 1.e-9), "equal-time 60fps and 30fps camera blending agrees");
        Check(Near(At60, 100 * BlendWeight(1, Speed), 1.e-9), "subdivided blend matches one-second analytic blend");
    }
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()})
    {
        Check(Near(BlendWeight(Invalid, 8), 0), "nonfinite delta has zero blend");
        Check(Near(BlendWeight(.1, Invalid), 0), "nonfinite speed has zero blend");
    }
}

void EnvelopeTests()
{
    using AlphaPresentation::EffectEnvelope;
    Check(Near(EffectEnvelope(0, 1), 1), "effect starts at full envelope");
    Check(Near(EffectEnvelope(.5, 1), .25), "effect uses squared remaining life");
    Check(Near(EffectEnvelope(1, 1), 0), "effect ends at exact lifetime");
    Check(Near(EffectEnvelope(2, 1), 0), "expired effect never becomes negative");
    Check(Near(EffectEnvelope(-1, 1), 1), "negative age clamps at start");
    Check(Near(EffectEnvelope(0, 0), 0), "zero lifetime has no effect");
    Check(Near(EffectEnvelope(0, -1), 0), "negative lifetime has no effect");
    Check(Near(EffectEnvelope(std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min()), 0),
        "extreme age over tiny lifetime expires safely");
    Check(Near(EffectEnvelope(std::numeric_limits<double>::max(), std::numeric_limits<double>::max()), 0),
        "equal extreme finite age and lifetime expire safely");
    Check(Near(EffectEnvelope(0, std::numeric_limits<double>::denorm_min()), 1), "tiny positive lifetime starts normally");
    for (double Lifetime : {.01, .2, 1.0, 100.0})
    {
        double Previous = 1;
        for (int Step = 0; Step <= 12; ++Step)
        {
            const double Value = EffectEnvelope(Lifetime * Step / 10.0, Lifetime);
            Check(std::isfinite(Value) && Value >= 0 && Value <= 1, "effect envelope remains finite and bounded");
            Check(Value <= Previous + 1.e-12, "effect envelope decays monotonically");
            Previous = Value;
        }
    }
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()})
    {
        Check(Near(EffectEnvelope(Invalid, 1), 0), "nonfinite effect age is invisible");
        Check(Near(EffectEnvelope(.1, Invalid), 0), "nonfinite effect lifetime is invisible");
    }
}
}

int main()
{
    CameraTests();
    BlendTests();
    EnvelopeTests();
    using AlphaPresentation::RouteCameraDistance;
    const double RouteBase = RouteCameraDistance(900, 100, 100, 16./9.);
    Check(RouteCameraDistance(900, 650, 100, 16./9.) >= RouteBase, "side gardens never reduce vertical framing");
    Check(RouteCameraDistance(900, 1000, 100, 16./9.) > RouteBase, "depth beyond existing camera capacity expands framing");
    Check(RouteCameraDistance(900, 650, 700, 16./9.) >= RouteCameraDistance(900, 650, 100, 16./9.), "jump height never shrinks camera framing");
    Check(RouteCameraDistance(2000, 100, 100, 16./9.) > RouteBase, "party separation expands horizontal framing");
    Check(Near(RouteCameraDistance(-1,-1,-1,-1),RouteCameraDistance(0,0,0,.5)), "negative camera inputs clamp consistently");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()})
    {
        Check(Near(RouteCameraDistance(Invalid,0,0,1),2800), "invalid width uses safe route camera default");
        Check(Near(RouteCameraDistance(0,Invalid,0,1),2800), "invalid depth uses safe route camera default");
        Check(Near(RouteCameraDistance(0,0,Invalid,1),2800), "invalid height uses safe route camera default");
        Check(Near(RouteCameraDistance(0,0,0,Invalid),2800), "invalid aspect uses safe route camera default");
    }
    for (double Span : {0., 100., 1000., 10000., std::numeric_limits<double>::max()})
    {
        const double Distance = RouteCameraDistance(Span,Span,Span,4.);
        Check(std::isfinite(Distance) && Distance>=2300. && Distance<=5200., "route camera is finite and bounded at extremes");
    }
    using AlphaPresentation::DebrisHeight;
    Check(Near(DebrisHeight(8, 100, 250, 0), 8), "debris starts on surface contact");
    Check(DebrisHeight(8, 100, 250, .2) > 8, "fragment rises during initial impulse");
    Check(Near(DebrisHeight(8, 100, 250, 1), 2), "fragment ends above contact plane");
    Check(Near(DebrisHeight(8, 100, 250, -1), 8), "negative fragment time clamps");
    Check(Near(DebrisHeight(8, 100, 250, 2), 2), "expired fragment time clamps");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        Check(Near(DebrisHeight(8, Invalid, 250, .5), 2), "invalid fragment data stays at surface");
    for (int Step = 0; Step <= 20; ++Step)
    {
        const double Height = DebrisHeight(8, 140, 250, Step / 20.0);
        Check(std::isfinite(Height) && Height >= 2 && Height <= 1200, "fragment remains finite above surface");
    }
    std::cout << "Portable presentation rules: " << (Checks - Failures) << '/' << Checks << " checks passed\n";
    std::cout << "These calculations do not validate Unreal rendering, material generation, cooking or performance.\n";
    return Failures == 0 ? 0 : 1;
}
