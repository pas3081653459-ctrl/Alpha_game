#include "Rules/PlayerControlRules.h"

#include <array>
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
bool Near(double Left, double Right) { return std::abs(Left - Right) <= 1.e-10; }

void TapTests()
{
    using AlphaPlayerControl::RegisterTap;
    using AlphaPlayerControl::TapState;
    TapState Exact;
    Check(!RegisterTap(Exact, true, 0), "first press is not a sprint tap");
    Check(!RegisterTap(Exact, false, .1), "release alone does not sprint");
    Check(RegisterTap(Exact, true, .28), "second press exactly at window triggers sprint");
    TapState Inside;
    RegisterTap(Inside, true, 0);
    RegisterTap(Inside, false, .1);
    Check(RegisterTap(Inside, true, .279), "second press within window triggers sprint");
    TapState Outside;
    RegisterTap(Outside, true, 0);
    RegisterTap(Outside, false, .1);
    Check(!RegisterTap(Outside, true, .281), "second press outside window does not sprint");
    TapState Held;
    Check(!RegisterTap(Held, true, 0), "initial held direction does not sprint");
    Check(!RegisterTap(Held, true, .1) && !RegisterTap(Held, true, .2), "key repeat cannot count as second press");
    RegisterTap(Held, false, .21);
    Check(RegisterTap(Held, true, .25), "real release permits a fresh press inside original window");
    TapState LongHeld;
    RegisterTap(LongHeld, true, 0);
    RegisterTap(LongHeld, false, 1);
    Check(!RegisterTap(LongHeld, true, 1.1), "long press followed by quick repress is not a double tap");
    TapState Releases;
    RegisterTap(Releases, true, 0);
    RegisterTap(Releases, false, .05);
    RegisterTap(Releases, false, .25);
    Check(!RegisterTap(Releases, true, .4), "duplicate release does not refresh previous press timestamp");
    TapState NoPress;
    RegisterTap(NoPress, false, 0);
    RegisterTap(NoPress, false, .1);
    Check(!RegisterTap(NoPress, true, .2), "releases without initial press cannot arm sprint");
    std::array<TapState, 4> Directions;
    RegisterTap(Directions[0], true, 0);
    RegisterTap(Directions[0], false, .05);
    Check(!RegisterTap(Directions[1], true, .1), "another direction does not inherit first direction's tap");
    Check(RegisterTap(Directions[0], true, .2), "same direction keeps its own independent press history");
    TapState Backward;
    RegisterTap(Backward, true, 1);
    RegisterTap(Backward, false, 1.1);
    Check(!RegisterTap(Backward, true, .9), "backward timestamp cannot trigger double tap");
    for (double Invalid : {-1.0, std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        TapState State;
        Check(!RegisterTap(State, true, Invalid) && !State.Held, "invalid press time does not arm or hold direction");
        RegisterTap(State, true, 0);
        Check(!RegisterTap(State, false, Invalid) && State.Held, "invalid release time cannot release held direction");
    }
    Check(Near(AlphaPlayerControl::SprintMultiplier, 2.5), "sprint speed contract is two and a half times walk");
    Check(Near(AlphaPlayerControl::DoubleTapWindow, .28), "double-tap contract is point two eight seconds");
}

void ComboTests()
{
    using AlphaPlayerControl::GetComboAttack;
    using AlphaPlayerControl::NextComboStage;
    int Stage = 0;
    for (int Index = 0; Index < 15; ++Index)
    {
        Stage = NextComboStage(Stage);
        Check(Stage == Index % 5 + 1, "accepted attacks repeat the five-stage sequence");
    }
    for (int Invalid : {std::numeric_limits<int>::min(), -1, 0, 5, 6, std::numeric_limits<int>::max()})
        Check(NextComboStage(Invalid) == 1, "invalid or completed combo restarts at first stage safely");
    constexpr std::array<double, 5> Damage{1, 1.1, 1.15, 1.2, 1.7};
    constexpr std::array<double, 5> Poise{1, 1.05, 1.3, 1.1, 1.8};
    double TotalDuration = 0;
    for (int Index = 1; Index <= 5; ++Index)
    {
        const auto Ground = GetComboAttack(Index, false);
        const auto Air = GetComboAttack(Index, true);
        Check(std::isfinite(Ground.Windup) && Ground.Windup > 0 && Ground.Duration > Ground.Windup,
            "each combo stage has finite positive windup and recovery");
        Check(Near(Ground.DamageScale, Damage[Index - 1]) && Near(Ground.PoiseScale, Poise[Index - 1]),
            "combo damage and poise scale follow the five-stage contract");
        Check(Ground.Reach > 0 && Ground.HalfWidth > 0 && Ground.HalfHeight > 0,
            "each stage has a nonempty attack volume");
        Check(Near(Air.Windup, Ground.Windup) && Near(Air.Duration, Ground.Duration)
            && Near(Air.DamageScale, Ground.DamageScale) && Near(Air.PoiseScale, Ground.PoiseScale),
            "airborne attacks preserve stage timing and power");
        Check(Near(Air.Reach, Ground.Reach) && Near(Air.HalfWidth, Ground.HalfWidth) && Near(Air.HalfHeight, Ground.HalfHeight),
            "airborne attacks retain intended horizontal and height volume");
        Check(Index == 5 ? Near(Air.VerticalOffset, -30) : Near(Air.VerticalOffset, Ground.VerticalOffset),
            "only air finisher lowers its hit volume");
        TotalDuration += Ground.Duration;
    }
    Check(GetComboAttack(5, false).DamageScale > GetComboAttack(1, false).DamageScale,
        "finisher is stronger than opening strike");
    Check(GetComboAttack(4, false).Reach > GetComboAttack(1, false).Reach,
        "fourth-stage thrust reaches farther than opener");
    for (int Invalid : {std::numeric_limits<int>::min(), -1, 0, 6, std::numeric_limits<int>::max()})
    {
        const int Expected = Invalid < 1 ? 1 : 5;
        for (bool Air : {false, true})
        {
            const auto Actual = GetComboAttack(Invalid, Air);
            const auto Clamped = GetComboAttack(Expected, Air);
            Check(Near(Actual.Windup, Clamped.Windup) && Near(Actual.Duration, Clamped.Duration)
                && Near(Actual.DamageScale, Clamped.DamageScale) && Near(Actual.VerticalOffset, Clamped.VerticalOffset),
                "invalid stage clamps consistently including airborne finisher offset");
        }
    }
    Check(AlphaPlayerControl::AirComboMaxSeconds > TotalDuration && AlphaPlayerControl::AirComboMaxSeconds <= 4,
        "finite air cap accommodates one five-hit chain without permitting indefinite hold");
    Check(AlphaPlayerControl::ComboInputBufferSeconds > 0 && AlphaPlayerControl::ComboTimeoutSeconds > AlphaPlayerControl::ComboInputBufferSeconds,
        "input buffer is finite and shorter than combo reset gap");
}

void EnergyTests()
{
    using AlphaPlayerControl::CanCast;
    using AlphaPlayerControl::EnergyChange;
    for (int Energy = 0; Energy <= 100; ++Energy)
    {
        Check(CanCast(0, Energy, true, true), "U is available at any valid energy");
        Check(CanCast(1, Energy, true, true) == (Energy >= 10), "I requires at least ten energy");
        Check(CanCast(2, Energy, true, true) == (Energy == 100), "O requires exactly full energy");
        for (int Slot = 0; Slot < 3; ++Slot)
        {
            Check(!CanCast(Slot, Energy, false, true) && !CanCast(Slot, Energy, true, false),
                "action lock and cooldown independently reject all skills");
            const double After = EnergyChange(Slot, Energy);
            Check(std::isfinite(After) && After >= 0 && After <= 100, "all valid-energy transactions remain bounded");
            if (!CanCast(Slot, Energy, true, true)) Check(Near(After, Energy), "rejected energy transaction leaves resource unchanged");
        }
    }
    Check(Near(EnergyChange(0, 0), 10) && Near(EnergyChange(0, 95), 100)
        && Near(EnergyChange(0, 100), 100), "U adds ten and clamps at full energy");
    Check(Near(EnergyChange(1, 10), 0) && Near(EnergyChange(1, 100), 90), "I consumes ten on successful cast");
    Check(Near(EnergyChange(2, 100), 0), "O consumes all one hundred energy");
    Check(!CanCast(1, 9.999, true, true) && !CanCast(2, 99.999, true, true), "fractional energy below exact thresholds is rejected");
    double Energy = 0;
    for (int Cast = 0; Cast < 10; ++Cast) Energy = EnergyChange(0, Energy);
    Check(Near(Energy, 100) && CanCast(2, Energy, true, true), "ten accepted U casts charge one ultimate");
    Energy = EnergyChange(2, Energy);
    Check(Near(Energy, 0) && !CanCast(2, Energy, true, true), "ultimate cannot be repeated without charging again");
    for (int Slot = 0; Slot < 3; ++Slot)
    {
        for (double Invalid : {-1.0, 100.001, std::numeric_limits<double>::max(),
                 -std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity()})
        {
            Check(!CanCast(Slot, Invalid, true, true), "invalid energy cannot cast");
            Check(EnergyChange(Slot, Invalid) == Invalid, "invalid energy is not silently rewarded or spent");
        }
        const double NaN = std::numeric_limits<double>::quiet_NaN();
        Check(!CanCast(Slot, NaN, true, true) && std::isnan(EnergyChange(Slot, NaN)), "NaN resource is rejected and unchanged");
    }
    for (int Invalid : {-1, 3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        Check(!CanCast(Invalid, 100, true, true) && Near(EnergyChange(Invalid, 100), 100), "invalid skill slot cannot spend resource");
}
}

int main()
{
    TapTests();
    ComboTests();
    EnergyTests();
    std::cout << "Portable player-control rules: " << Checks - Failures << '/' << Checks << " checks passed\n";
    std::cout << "Unreal input, movement prediction, aerial timers, attack volumes and networking remain separate tests.\n";
    return Failures == 0 ? 0 : 1;
}
