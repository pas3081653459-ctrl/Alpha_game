#pragma once

#include <algorithm>
#include <cmath>

namespace AlphaPlayerControl
{
constexpr double SprintMultiplier = 2.5;
constexpr double DoubleTapWindow = .28;
constexpr double ComboInputBufferSeconds = .32;
constexpr double ComboTimeoutSeconds = .65;
constexpr double AirComboMaxSeconds = 3.2;

struct TapState
{
    double LastPress = -1.;
    bool Held = false;
};

// Repeated key-down events cannot count as a second tap. Release is required.
inline bool RegisterTap(TapState& State, bool Pressed, double Now)
{
    if (!std::isfinite(Now) || Now < 0.) return false;
    if (!Pressed) { State.Held = false; return false; }
    if (State.Held) return false;
    const bool Double = State.LastPress >= 0. && Now >= State.LastPress && Now - State.LastPress <= DoubleTapWindow;
    State.LastPress = Now;
    State.Held = true;
    return Double;
}

inline int NextComboStage(int Previous)
{
    return Previous >= 1 && Previous < 5 ? Previous + 1 : 1;
}

struct ComboAttackData
{
    double Windup;
    double Duration;
    double DamageScale;
    double PoiseScale;
    double Reach;
    double HalfWidth;
    double HalfHeight;
    double VerticalOffset;
};

inline ComboAttackData GetComboAttack(int Stage, bool Airborne)
{
    // Forward cut, reverse cut, rising cut, thrust, descending finisher.
    const ComboAttackData Steps[] = {
        {.10, .28, 1.00, 1.00, 175., 65., 75., 0.},
        {.11, .30, 1.10, 1.05, 185., 70., 75., 0.},
        {.14, .34, 1.15, 1.30, 160., 70., 100., 20.},
        {.10, .28, 1.20, 1.10, 230., 50., 70., 0.},
        {.20, .48, 1.70, 1.80, 190., 100., 85., -15.},
    };
    const int SafeStage = std::clamp(Stage, 1, 5);
    ComboAttackData Result = Steps[SafeStage - 1];
    if (Airborne)
    {
        // Downward crescent, reverse wheel, lifting cross, diving thrust, meteor.
        // Broad lower coverage reaches grounded enemies without granting invulnerability.
        const ComboAttackData AirSteps[] = {
            {.09, .28, 1.00, 1.00, 190., 85., 155., -85.},
            {.10, .30, 1.10, 1.05, 200., 105., 160., -85.},
            {.13, .34, 1.15, 1.30, 180., 95., 185., -65.},
            {.09, .28, 1.20, 1.10, 255., 65., 165., -100.},
            {.20, .48, 1.70, 1.80, 190., 120., 190., -115.},
        };
        Result = AirSteps[SafeStage - 1];
    }
    return Result;
}

// The impact is local to a real floor contact, never an unbounded aerial laser.
inline double GroundImpactRadius(bool Airborne) { return Airborne ? 285. : 235.; }
inline double GroundImpactTraceDepth(bool Airborne) { return Airborne ? 520. : 210.; }
inline double ComboLiftSpeed(int Stage, bool Airborne)
{
    return Airborne && std::clamp(Stage, 1, 5) == 3 ? 390. : 0.;
}

inline bool CanCast(int Slot, double Energy, bool ActionAllowed, bool CooldownReady)
{
    if (Slot < 0 || Slot > 2 || !std::isfinite(Energy) || Energy < 0. || Energy > 100.
        || !ActionAllowed || !CooldownReady) return false;
    return Slot == 0 || (Slot == 1 ? Energy >= 10. : Energy >= 100.);
}

// Returns unchanged energy for rejected casts; caller gates timing/authority.
inline double EnergyChange(int Slot, double Energy)
{
    if (!CanCast(Slot, Energy, true, true)) return Energy;
    if (Slot == 0) return std::min(100., Energy + 10.);
    return Energy - (Slot == 1 ? 10. : 100.);
}
}
