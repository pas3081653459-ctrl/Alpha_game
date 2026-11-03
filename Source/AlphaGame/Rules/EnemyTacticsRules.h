#pragma once

#include <algorithm>
#include <cmath>

namespace AlphaEnemyRules
{
constexpr double GunnerWindup = .85;
constexpr double GunnerRecovery = .8;
constexpr double GunnerLength = 760.;
constexpr double GunnerHalfWidth = 48.;
constexpr double GunnerDamage = 10.;
constexpr double ShieldOpenSeconds = 2.;
inline bool ValidRole(int Role) { return Role >= 0 && Role <= 2; }
inline int RoleForEncounter(int Chapter, int Wave, int Index)
{
    if (Chapter < 0 || Chapter > 2 || Wave < 1 || Wave > (Chapter == 1 ? 3 : 2) || Index < 0) return 0;
    if (Chapter == 0) return Wave == 1 ? 0 : (Index == 0 ? 1 : (Index == 2 ? 2 : 0));
    if (Chapter == 1)
    {
        if (Wave == 1) return Index == 0 ? 1 : (Index == 3 ? 2 : 0);
        return Index == 0 || (Wave == 3 && Index == 4) ? 1 : (Index == 2 || Index == 5 ? 2 : 0);
    }
    return Index == 0 ? 1 : (Index == 3 || (Wave == 2 && Index == 5) ? 2 : 0);
}
inline bool HasAttackSlot(int ActiveAttackers) { return ActiveAttackers >= 0 && ActiveAttackers < 2; }
inline bool ShieldBlocks(bool Active, double ForwardDot, double HorizontalDistance, double HeightDifference)
{
    return Active && std::isfinite(ForwardDot) && std::isfinite(HorizontalDistance)
        && std::isfinite(HeightDifference) && HorizontalDistance > 1.
        && std::abs(HeightDifference) <= 160. && ForwardDot >= .5 && ForwardDot <= 1.000001;
}
inline double GuardedDamage(double Damage, bool Blocked)
{
    if (!std::isfinite(Damage) || Damage < 0.) return Damage;
    return Blocked ? Damage * .25 : Damage;
}
inline bool InGunnerLine(double Along, double Depth, double FeetHeightDifference)
{
    return std::isfinite(Along) && std::isfinite(Depth) && std::isfinite(FeetHeightDifference)
        && Along >= 0. && Along <= GunnerLength && std::abs(Depth) <= GunnerHalfWidth
        && std::abs(FeetHeightDifference) <= 90.;
}
inline bool ShouldShoot(double HorizontalDistance, double HeightDifference, bool ClearLine)
{
    return ClearLine && std::isfinite(HorizontalDistance) && std::isfinite(HeightDifference)
        && HorizontalDistance >= 250. && HorizontalDistance <= 700. && std::abs(HeightDifference) <= 160.;
}
inline double TellProgress(double Now, double StartedAt, double ResolveAt)
{
    if (!std::isfinite(Now) || !std::isfinite(StartedAt) || !std::isfinite(ResolveAt) || ResolveAt <= StartedAt) return 0.;
    return std::clamp((Now - StartedAt) / (ResolveAt - StartedAt), 0., 1.);
}
}
