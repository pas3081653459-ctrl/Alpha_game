#include "Rules/CombatRules.h"

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

bool Near(double Left, double Right)
{
    return std::abs(Left - Right) < 1e-9;
}

void DamageTests()
{
    using AlphaRules::ResolveDamage;
    const auto Normal = ResolveDamage(100, 60, 100, 60, 25, 15, false, false);
    Check(Normal.Accepted && Near(Normal.Health, 75) && Near(Normal.Poise, 45)
        && Near(Normal.ActualDamage, 25) && !Normal.PoiseBroken, "enemy hit applies damage and poise");
    for (const auto Flags : {std::array<bool, 2>{true, false}, {false, true}, {true, true}})
    {
        const auto Result = ResolveDamage(100, 60, 100, 60, 25, 15, Flags[0], Flags[1]);
        Check(!Result.Accepted && Near(Result.Health, 100) && Near(Result.Poise, 60)
            && Near(Result.ActualDamage, 0), "friendly or invulnerable target rejects all damage");
    }
    const auto Dead = ResolveDamage(0, 12, 100, 60, 25, 15, false, false);
    Check(!Dead.Accepted && Near(Dead.Health, 0) && Near(Dead.Poise, 12), "dead target is unchanged");
    const auto Negative = ResolveDamage(75, 45, 100, 60, -25, -15, false, false);
    Check(!Negative.Accepted && Near(Negative.Health, 75) && Near(Negative.Poise, 45), "negative damage cannot heal or repair poise");
    const auto MixedNegative = ResolveDamage(75, 45, 100, 60, -25, 10, false, false);
    Check(MixedNegative.Accepted && Near(MixedNegative.Health, 75) && Near(MixedNegative.Poise, 35), "negative health damage does not suppress valid poise damage");
    const auto Zero = ResolveDamage(100, 60, 100, 60, 0, 0, false, false);
    Check(!Zero.Accepted, "zero hit does not award a confirmed damage event");
    const auto Break = ResolveDamage(100, 35, 100, 60, 0, 35, false, false);
    Check(Break.Accepted && Break.PoiseBroken && Near(Break.Poise, 60)
        && Near(Break.Health, 100) && Near(Break.ActualDamage, 0), "exact poise break refills meter without health damage");
    const auto Overkill = ResolveDamage(30, 5, 100, 60, 1000, 1000, false, false);
    Check(Overkill.Accepted && Near(Overkill.Health, 0) && Near(Overkill.ActualDamage, 30)
        && Near(Overkill.Poise, 0) && !Overkill.PoiseBroken, "overkill clamps damage and does not stagger a corpse");
    const auto Max = std::numeric_limits<double>::max();
    const auto Huge = ResolveDamage(100, 60, 100, 60, Max, Max, false, false);
    Check(Huge.Accepted && Near(Huge.Health, 0) && Near(Huge.ActualDamage, 100), "finite huge damage remains bounded");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        for (std::size_t Index = 0; Index < 6; ++Index)
        {
            std::array<double, 6> Values{100, 60, 100, 60, 25, 15};
            Values[Index] = Invalid;
            const auto Result = ResolveDamage(Values[0], Values[1], Values[2], Values[3], Values[4], Values[5], false, false);
            Check(!Result.Accepted && Near(Result.ActualDamage, 0) && !Result.PoiseBroken,
                "nonfinite argument rejected at index " + std::to_string(Index));
        }
    }
    for (double InvalidMax : {0.0, -1.0})
    {
        Check(!ResolveDamage(100, 60, InvalidMax, 60, 10, 10, false, false).Accepted, "invalid max health rejected");
        Check(!ResolveDamage(100, 60, 100, InvalidMax, 10, 10, false, false).Accepted, "invalid max poise rejected");
    }
}

void ReviveTests()
{
    AlphaRules::ReviveConditions Good{true, true, true, true, true, true, true};
    const auto Partial = AlphaRules::AdvanceRevive(0, 0.5, 3, Good);
    Check(!Partial.Cancelled && !Partial.Completed && Near(Partial.Elapsed, .5), "revive accumulates valid progress");
    const auto Exact = AlphaRules::AdvanceRevive(2.5, .5, 3, Good);
    Check(!Exact.Cancelled && Exact.Completed && Near(Exact.Elapsed, 3), "revive completes exactly at required time");
    const auto Overshoot = AlphaRules::AdvanceRevive(2.5, 8, 3, Good);
    Check(!Overshoot.Cancelled && Overshoot.Completed && Near(Overshoot.Elapsed, 3), "revive overshoot clamps to completion");
    const auto Zero = AlphaRules::AdvanceRevive(1, 0, 3, Good);
    Check(!Zero.Cancelled && !Zero.Completed && Near(Zero.Elapsed, 1), "zero delta preserves progress");
    const std::array<bool AlphaRules::ReviveConditions::*, 7> Conditions{
        &AlphaRules::ReviveConditions::HelperAlive, &AlphaRules::ReviveConditions::TargetDown,
        &AlphaRules::ReviveConditions::InRange, &AlphaRules::ReviveConditions::HasLineOfSight,
        &AlphaRules::ReviveConditions::HoldingInteract, &AlphaRules::ReviveConditions::DamageUnchanged,
        &AlphaRules::ReviveConditions::OwnsClaim};
    for (std::size_t Index = 0; Index < Conditions.size(); ++Index)
    {
        auto Broken = Good;
        Broken.*Conditions[Index] = false;
        const auto Result = AlphaRules::AdvanceRevive(2.5, .5, 3, Broken);
        Check(Result.Cancelled && !Result.Completed && Near(Result.Elapsed, 0),
            "revive condition cancels and clears progress " + std::to_string(Index));
    }
    for (const auto Values : {std::array<double, 3>{-1, .1, 3}, {1, -.1, 3}, {1, .1, 0}, {1, .1, -3}})
    {
        const auto Result = AlphaRules::AdvanceRevive(Values[0], Values[1], Values[2], Good);
        Check(Result.Cancelled && !Result.Completed && Near(Result.Elapsed, 0), "invalid revive duration or delta cancels");
    }
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        for (std::size_t Index = 0; Index < 3; ++Index)
        {
            std::array<double, 3> Values{1, .1, 3};
            Values[Index] = Invalid;
            const auto Result = AlphaRules::AdvanceRevive(Values[0], Values[1], Values[2], Good);
            Check(Result.Cancelled && !Result.Completed && Near(Result.Elapsed, 0), "nonfinite revive argument rejected");
        }
    }
    Check(Near(AlphaRules::ReviveDuration(1), 3), "first down takes three seconds");
    Check(Near(AlphaRules::ReviveDuration(2), 3.75), "second down increases revive duration");
    Check(Near(AlphaRules::ReviveDuration(5), 6), "fifth down reaches duration cap");
    Check(Near(AlphaRules::ReviveDuration(std::numeric_limits<int>::min()), 3), "negative extreme down count clamps safely");
    Check(Near(AlphaRules::ReviveDuration(std::numeric_limits<int>::max()), 6), "positive extreme down count clamps safely");
}


void BossZoneTests()
{
    using AlphaRules::BossAttackType;
    using AlphaRules::IsPointInBossZone;
    using AlphaRules::Point3D;
    const Point3D Origin{0, 0, 0};
    const Point3D Forward{1, 0, 0};
    const auto InZone = [&](BossAttackType Attack, Point3D Point)
    {
        return IsPointInBossZone(Attack, Origin, Forward, Point);
    };
    Check(!InZone(BossAttackType::None, Origin), "no boss attack has no damage zone");
    Check(!InZone(static_cast<BossAttackType>(999), Origin), "unknown boss attack has no damage zone");
    Check(InZone(BossAttackType::Hammer, Origin), "hammer includes center");
    Check(InZone(BossAttackType::Hammer, {240, 0, 180}), "hammer includes radius and upper height boundary");
    Check(InZone(BossAttackType::Hammer, {0, -240, -180}), "hammer includes opposite radius and lower height boundary");
    Check(!InZone(BossAttackType::Hammer, {240.001, 0, 0}), "hammer excludes outside radius");
    Check(!InZone(BossAttackType::Hammer, {200, 200, 0}), "hammer is circular not square");
    Check(!InZone(BossAttackType::Hammer, {0, 0, 180.001}), "hammer excludes above height");
    Check(!InZone(BossAttackType::Hammer, {0, 0, -180.001}), "hammer excludes below height");
    Check(IsPointInBossZone(BossAttackType::Hammer, Origin, {0, 0, 0}, {1, 1, 0}),
        "hammer circle does not require a direction");
    Check(InZone(BossAttackType::FireLine, {0, -85, -180}), "line includes start width and height boundary");
    Check(InZone(BossAttackType::FireLine, {1000, 85, 180}), "line includes far width and height boundary");
    Check(InZone(BossAttackType::FireLine, {500, 0, 0}), "line includes interior");
    Check(!InZone(BossAttackType::FireLine, {-0.001, 0, 0}), "line excludes behind origin");
    Check(!InZone(BossAttackType::FireLine, {1000.001, 0, 0}), "line excludes beyond length");
    Check(!InZone(BossAttackType::FireLine, {500, 85.001, 0}), "line excludes beyond positive width");
    Check(!InZone(BossAttackType::FireLine, {500, -85.001, 0}), "line excludes beyond negative width");
    Check(!InZone(BossAttackType::FireLine, {500, 0, 180.001}), "line excludes above height");
    Check(!InZone(BossAttackType::FireLine, {500, 0, -180.001}), "line excludes below height");
    Check(InZone(BossAttackType::Sweep, Origin), "sweep includes origin");
    Check(InZone(BossAttackType::Sweep, {330, 0, 180}), "sweep includes radial and height boundaries");
    Check(!InZone(BossAttackType::Sweep, {330.001, 0, 0}), "sweep excludes outside radius");
    Check(!InZone(BossAttackType::Sweep, {-1, 0, 0}), "sweep excludes rear");
    Check(!InZone(BossAttackType::Sweep, {0, 1, 0}), "sweep excludes perpendicular side");
    Check(!InZone(BossAttackType::Sweep, {100, 0, -180.001}), "sweep excludes below height");
    Check(!InZone(BossAttackType::Sweep, {100, 0, 180.001}), "sweep excludes above height");
    const double Radians = std::acos(-1.0) / 180.0;
    for (double Sign : {-1.0, 1.0})
    {
        Check(InZone(BossAttackType::Sweep, {100 * std::cos(64.999 * Radians),
                  Sign * 100 * std::sin(64.999 * Radians), 0}), "sweep includes just inside each angular edge");
        Check(!InZone(BossAttackType::Sweep, {100 * std::cos(65.001 * Radians),
                  Sign * 100 * std::sin(65.001 * Radians), 0}), "sweep excludes just outside each angular edge");
    }
    constexpr double Cos65 = 0.42261826174069944;
    Check(IsPointInBossZone(BossAttackType::Sweep, Origin, {Cos65, std::sqrt(1 - Cos65 * Cos65), 0}, {100, 0, 0}),
        "sweep includes exact angular edge");
    const Point3D Shifted{10, -20, 30};
    Check(IsPointInBossZone(BossAttackType::Hammer, Shifted, Forward, {250, -20, 30}),
        "hammer zone is relative to translated origin");
    Check(IsPointInBossZone(BossAttackType::FireLine, Shifted, {0, 10, 100}, {95, 980, 210}),
        "line rotates and normalizes planar direction independently of Z");
    Check(!IsPointInBossZone(BossAttackType::FireLine, Shifted, {0, 10, 0}, {95.001, 980, 30}),
        "rotated line enforces perpendicular width");
    Check(IsPointInBossZone(BossAttackType::Sweep, Shifted, {-3, 0, 0}, {-320, -20, -150}),
        "sweep supports reversed unnormalized direction and translated lower height boundary");
    Check(!IsPointInBossZone(BossAttackType::Sweep, Shifted, {-3, 0, 0}, {20, -20, 30}),
        "reversed sweep excludes opposite direction");
    for (BossAttackType Attack : {BossAttackType::FireLine, BossAttackType::Sweep})
    {
        Check(!IsPointInBossZone(Attack, Origin, {0, 0, 1}, {1, 0, 0}), "directional attack rejects zero planar direction");
        Check(!IsPointInBossZone(Attack, Origin, {1e-12, 0, 0}, {1, 0, 0}), "directional attack rejects near-zero direction");
    }
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()})
    {
        for (BossAttackType Attack : {BossAttackType::Hammer, BossAttackType::FireLine, BossAttackType::Sweep})
        {
            for (std::size_t Vector = 0; Vector < 3; ++Vector)
            {
                for (std::size_t Axis = 0; Axis < 3; ++Axis)
                {
                    std::array<Point3D, 3> Values{Origin, Forward, Point3D{1, 0, 0}};
                    double Point3D::* Field = Axis == 0 ? &Point3D::X : (Axis == 1 ? &Point3D::Y : &Point3D::Z);
                    Values[Vector].*Field = Invalid;
                    Check(!IsPointInBossZone(Attack, Values[0], Values[1], Values[2]),
                        "boss zone rejects every nonfinite coordinate");
                }
            }
        }
    }
    const double Extreme = std::numeric_limits<double>::max();
    for (BossAttackType Attack : {BossAttackType::Hammer, BossAttackType::FireLine, BossAttackType::Sweep})
    {
        Check(!IsPointInBossZone(Attack, {-Extreme, 0, 0}, Forward, {Extreme, 0, 0}),
            "extreme coordinate subtraction overflow fails closed");
        Check(!IsPointInBossZone(Attack, Origin, Forward, {Extreme, 0, 0}),
            "extreme squared distance overflow fails closed");
        Check(!IsPointInBossZone(Attack, {0, 0, -Extreme}, Forward, {0, 0, Extreme}),
            "extreme height fails closed");
    }
    Check(IsPointInBossZone(BossAttackType::Hammer, {Extreme, Extreme, Extreme}, Forward, {Extreme, Extreme, Extreme}),
        "identical finite extreme coordinates retain zero relative offset");
    Check(IsPointInBossZone(BossAttackType::FireLine, Origin, {Extreme, 0, 0}, {500, 0, 0}),
        "finite extreme single-axis direction normalizes safely");
    Check(!IsPointInBossZone(BossAttackType::FireLine, Origin, {Extreme, Extreme, 0}, {500, 0, 0}),
        "overflowing direction magnitude fails closed");
}

void ScalingAndCoopTests()
{
    Check(AlphaRules::WaveEnemyCount(1, 1) == 3, "first solo wave has three enemies");
    Check(AlphaRules::WaveEnemyCount(3, 3) == 7, "third trio wave has seven enemies");
    Check(AlphaRules::WaveEnemyCount(-10, 0) == 3, "wave and player lower bounds clamp");
    Check(AlphaRules::WaveEnemyCount(100, 100) == 7, "wave and player upper bounds clamp");
    Check(AlphaRules::WaveEnemyCount(std::numeric_limits<int>::min(), std::numeric_limits<int>::min()) == 3,
        "extreme negative wave and players clamp without overflow");
    Check(Near(AlphaRules::BossHealth(1), 600) && Near(AlphaRules::BossHealth(2), 900)
        && Near(AlphaRules::BossHealth(3), 1200), "boss health scales for one to three players");
    Check(Near(AlphaRules::BossHealth(std::numeric_limits<int>::min()), 600)
        && Near(AlphaRules::BossHealth(std::numeric_limits<int>::max()), 1200), "boss players clamp at integer extremes");
    Check(AlphaRules::CooldownReady(5, 5), "cooldown accepts exact deadline");
    Check(!AlphaRules::CooldownReady(4.999, 5), "cooldown rejects early action");
    Check(AlphaRules::IsCoopFollowup(true, 10, 8, 8.5), "co-op accepts exact followup and cooldown boundaries");
    Check(!AlphaRules::IsCoopFollowup(false, 10, 9, 0), "same player cannot trigger co-op bonus");
    Check(!AlphaRules::IsCoopFollowup(true, 10, 7.999, 0), "expired followup rejected");
    Check(!AlphaRules::IsCoopFollowup(true, 10, 10.001, 0), "future hit rejected");
    Check(!AlphaRules::IsCoopFollowup(true, 10, 9, 8.501), "co-op proc cooldown enforced");
    Check(!AlphaRules::IsCoopFollowup(true, 10, 9, 10.001), "future proc rejected");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        Check(!AlphaRules::CooldownReady(Invalid, 5) && !AlphaRules::CooldownReady(5, Invalid), "nonfinite cooldown rejected");
        Check(!AlphaRules::IsCoopFollowup(true, Invalid, 8, 0), "nonfinite co-op current time rejected");
        Check(!AlphaRules::IsCoopFollowup(true, 10, Invalid, 0), "nonfinite co-op hit time rejected");
        Check(!AlphaRules::IsCoopFollowup(true, 10, 8, Invalid), "nonfinite co-op proc time rejected");
    }
}
}

int main()
{
    DamageTests();
    ReviveTests();
    ScalingAndCoopTests();
    BossZoneTests();
    std::cout << "Portable shared rules: " << (Checks - Failures) << '/' << Checks << " checks passed\n";
    std::cout << "Unreal integration, replication, rendering, Windows builds and LAN remain separate tests.\n";
    return Failures == 0 ? 0 : 1;
}
