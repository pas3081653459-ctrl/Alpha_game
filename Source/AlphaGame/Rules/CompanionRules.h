#pragma once

#include <algorithm>
#include <cmath>
#include "Rules/PlayerControlRules.h"

namespace AlphaCompanionRules
{
inline int MissingCompanions(int HumanPlayers, bool FillEnabled)
{
    const int Humans = std::clamp(HumanPlayers, 0, 3);
    return FillEnabled && Humans > 0 ? 3 - Humans : 0;
}

// Choose the least represented hero. Every assigned companion updates the
// caller's counts before the next choice, preserving three distinct roles.
inline int ChooseHero(int Blades, int Weavers, int Lanterns)
{
    const int Counts[] = {std::max(0, Blades), std::max(0, Weavers), std::max(0, Lanterns)};
    int Result = 0;
    for (int Index = 1; Index < 3; ++Index) if (Counts[Index] < Counts[Result]) Result = Index;
    return Result;
}

enum class Action { Idle, Follow, Attack, Revive, Evade };

struct Decision
{
    bool CanAct = false;
    bool RunActive = false;
    bool InDanger = false;
    bool HasDownedAlly = false;
    bool HasEnemy = false;
    bool HasHuman = false;
};

inline Action ChooseAction(const Decision& State)
{
    if (!State.CanAct) return Action::Idle;
    if (!State.RunActive) return State.HasHuman ? Action::Follow : Action::Idle;
    if (State.InDanger) return Action::Evade;
    if (State.HasDownedAlly) return Action::Revive;
    if (State.HasEnemy) return Action::Attack;
    return State.HasHuman ? Action::Follow : Action::Idle;
}

inline bool InMeleeRange(double DeltaX, double DeltaY, double DeltaZ)
{
    return std::isfinite(DeltaX) && std::isfinite(DeltaY) && std::isfinite(DeltaZ)
        && std::abs(DeltaX) <= 145.0 && std::abs(DeltaY) <= 65.0 && std::abs(DeltaZ) <= 95.0;
}

inline bool ShouldUseSkill(int Hero, int Slot, double Distance, bool HasEnemy,
    double Cooldown, bool CanAct, double Energy = 0.)
{
    if (!CanAct || !HasEnemy || Hero < 0 || Hero > 2 || Slot < 0 || Slot > 2
        || !std::isfinite(Distance) || !std::isfinite(Cooldown) || Distance < 0.0 || Cooldown > 0.0) return false;
    const double Ranges[3][3] = {{190., 300., 500.}, {240., 200., 290.}, {400., 290., 450.}};
    return Distance <= Ranges[Hero][Slot] && AlphaPlayerControl::CanCast(Slot, Energy, CanAct, Cooldown <= 0.);
}

// Target-center coverage of the same sphere/segment-sphere skill shapes used
// by the ability component. This prevents depth-misaligned cooldown waste.
inline bool InSkillFootprint(int Hero, int Slot, double Forward, double Depth, double Height)
{
    if (Hero < 0 || Hero > 2 || Slot < 0 || Slot > 2 || !std::isfinite(Forward)
        || !std::isfinite(Depth) || !std::isfinite(Height)) return false;
    const double Reach[3][3] = {{110., 240., 360.}, {200., 0., 0.}, {0., 0., 0.}};
    const double Radius[3][3] = {{100., 110., 180.}, {85., 210., 300.}, {420., 310., 460.}};
    const double SegmentX = std::clamp(Forward, 0.0, Reach[Hero][Slot]);
    return std::hypot(Forward - SegmentX, Depth, Height) <= Radius[Hero][Slot];
}
}
