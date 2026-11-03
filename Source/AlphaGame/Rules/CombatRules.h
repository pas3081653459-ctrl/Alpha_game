#pragma once

#include <algorithm>
#include <cmath>

// Engine-independent rules used by authority-side components and native tests.
// No networking or presentation state belongs here.
namespace AlphaRules
{
inline bool CooldownReady(double Now, double ReadyAt)
{
    return std::isfinite(Now) && std::isfinite(ReadyAt) && Now >= ReadyAt;
}

struct DamageResult
{
    double Health;
    double Poise;
    double ActualDamage;
    bool Accepted;
    bool PoiseBroken;
};

inline DamageResult ResolveDamage(double Health, double Poise, double MaxHealth,
    double MaxPoise, double Damage, double PoiseDamage, bool Friendly, bool Invulnerable)
{
    DamageResult Result{Health, Poise, 0.0, false, false};
    if (!std::isfinite(Health) || !std::isfinite(Poise) || !std::isfinite(MaxHealth)
        || !std::isfinite(MaxPoise) || !std::isfinite(Damage) || !std::isfinite(PoiseDamage)
        || Health <= 0.0 || MaxHealth <= 0.0 || MaxPoise <= 0.0 || Friendly || Invulnerable)
    {
        return Result;
    }
    Damage = std::max(0.0, Damage);
    PoiseDamage = std::max(0.0, PoiseDamage);
    if (Damage == 0.0 && PoiseDamage == 0.0) return Result;
    Result.Health = std::clamp(Health - Damage, 0.0, MaxHealth);
    Result.ActualDamage = Health - Result.Health;
    Result.Poise = std::clamp(Poise - PoiseDamage, 0.0, MaxPoise);
    Result.PoiseBroken = Result.Poise <= 0.0 && Result.Health > 0.0;
    if (Result.PoiseBroken) Result.Poise = MaxPoise;
    Result.Accepted = true;
    return Result;
}

struct ReviveConditions
{
    bool HelperAlive = false;
    bool TargetDown = false;
    bool InRange = false;
    bool HasLineOfSight = false;
    bool HoldingInteract = false;
    bool DamageUnchanged = false;
    bool OwnsClaim = false;
};

struct ReviveStep
{
    double Elapsed;
    bool Cancelled;
    bool Completed;
};

inline ReviveStep AdvanceRevive(double Elapsed, double DeltaSeconds, double RequiredSeconds,
    const ReviveConditions& Conditions)
{
    if (!Conditions.HelperAlive || !Conditions.TargetDown || !Conditions.InRange
        || !Conditions.HasLineOfSight || !Conditions.HoldingInteract
        || !Conditions.DamageUnchanged || !Conditions.OwnsClaim
        || !std::isfinite(Elapsed) || !std::isfinite(DeltaSeconds)
        || !std::isfinite(RequiredSeconds) || Elapsed < 0.0 || DeltaSeconds < 0.0
        || RequiredSeconds <= 0.0)
    {
        return {0.0, true, false};
    }
    const double Next = std::min(RequiredSeconds, Elapsed + DeltaSeconds);
    return {Next, false, Next >= RequiredSeconds};
}

inline double ReviveDuration(int DownCount)
{
    return 3.0 + 0.75 * (std::clamp(DownCount, 1, 5) - 1);
}

inline int WaveEnemyCount(int Wave, int Players)
{
    return 2 + std::clamp(Wave, 1, 3) + std::clamp(Players, 1, 3) - 1;
}

inline double BossHealth(int Players)
{
    return 360.0 + 180.0 * (std::clamp(Players, 1, 3) - 1);
}

inline bool IsCoopFollowup(bool DifferentPlayer, double Now, double LastHit, double LastProc)
{
    return DifferentPlayer && std::isfinite(Now) && std::isfinite(LastHit)
        && std::isfinite(LastProc) && Now >= LastHit && Now - LastHit <= 2.0
        && Now >= LastProc && Now - LastProc >= 1.5;
}

struct Point3D
{
    double X;
    double Y;
    double Z;
};

enum class BossAttackType { None, Sweep, FireLine, Hammer };

inline bool IsPointInBossZone(BossAttackType Attack, Point3D Origin, Point3D Direction, Point3D Point)
{
    const auto Finite = [](Point3D Value)
    {
        return std::isfinite(Value.X) && std::isfinite(Value.Y) && std::isfinite(Value.Z);
    };
    if (!Finite(Origin) || !Finite(Direction) || !Finite(Point)) return false;
    const double X = Point.X - Origin.X;
    const double Y = Point.Y - Origin.Y;
    if (std::abs(Point.Z - Origin.Z) > 180.0) return false;
    const double DistanceSquared = X * X + Y * Y;
    if (!std::isfinite(DistanceSquared)) return false;
    if (Attack == BossAttackType::Hammer) return DistanceSquared <= 240.0 * 240.0;
    const double DirectionLength = std::hypot(Direction.X, Direction.Y);
    if (!std::isfinite(DirectionLength) || DirectionLength < 1.e-9) return false;
    const double ForwardX = Direction.X / DirectionLength;
    const double ForwardY = Direction.Y / DirectionLength;
    const double Along = X * ForwardX + Y * ForwardY;
    if (Attack == BossAttackType::FireLine)
    {
        return Along >= 0.0 && Along <= 1000.0 && std::abs(-X * ForwardY + Y * ForwardX) <= 85.0;
    }
    if (Attack == BossAttackType::Sweep)
    {
        constexpr double Cos65Degrees = 0.42261826174069944;
        return DistanceSquared <= 330.0 * 330.0
            && (DistanceSquared < 1.e-12 || Along / std::sqrt(DistanceSquared) >= Cos65Degrees);
    }
    return false;
}
}
