#include "Rules/CompanionRules.h"

#include <array>
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

void FillTests()
{
    using AlphaCompanionRules::MissingCompanions;
    Check(MissingCompanions(0, true) == 0, "no human must not create an autonomous bot match");
    Check(MissingCompanions(1, true) == 2, "solo gets two companions");
    Check(MissingCompanions(2, true) == 1, "two humans get one companion");
    Check(MissingCompanions(3, true) == 0, "three humans get no companions");
    Check(MissingCompanions(std::numeric_limits<int>::min(), true) == 0, "negative extreme human count safely clamps to no match");
    Check(MissingCompanions(std::numeric_limits<int>::max(), true) == 0, "positive extreme human count safely clamps to full match");
    for (int Humans : {-1, 0, 1, 2, 3, 4, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
    {
        Check(MissingCompanions(Humans, false) == 0, "disabled fill never creates companions");
    }
    for (int Humans = 1; Humans <= 3; ++Humans)
    {
        const int Bots = MissingCompanions(Humans, true);
        Check(Bots >= 0 && Bots <= 2 && Bots + Humans == 3, "filled team has exactly three actors without a fourth participant");
    }
}

void HeroTests()
{
    using AlphaCompanionRules::ChooseHero;
    Check(ChooseHero(0, 0, 0) == 0, "empty hero counts use deterministic first-role tie break");
    Check(ChooseHero(1, 0, 0) == 1, "blade human is complemented with weaver first");
    Check(ChooseHero(1, 1, 0) == 2, "missing lantern is preferred");
    Check(ChooseHero(0, 1, 0) == 0, "weaver human is complemented with blade first");
    Check(ChooseHero(0, 0, 1) == 0, "lantern human is complemented with blade first");
    Check(ChooseHero(0, 2, 0) == 0, "duplicate human roles choose missing blade deterministically");
    Check(ChooseHero(2, 0, 0) == 1, "duplicate blade humans choose a different role");
    Check(ChooseHero(2, 3, 1) == 2, "least represented nonzero role is chosen");
    Check(ChooseHero(4, 1, 1) == 1, "equal minority counts choose first minority role");
    Check(ChooseHero(-1, -2, -3) == 0, "negative counts are treated as zero");
    Check(ChooseHero(std::numeric_limits<int>::max(), std::numeric_limits<int>::min(), 0) == 1,
        "integer extremes preserve valid least represented role");
    Check(ChooseHero(std::numeric_limits<int>::max(), std::numeric_limits<int>::max(), std::numeric_limits<int>::max()) == 0,
        "equal maximal counts retain deterministic tie break without arithmetic overflow");
    for (int HumanHero = 0; HumanHero < 3; ++HumanHero)
    {
        std::array<int, 3> Counts{0, 0, 0};
        ++Counts[HumanHero];
        for (int Bot = 0; Bot < 2; ++Bot)
        {
            const int Hero = ChooseHero(Counts[0], Counts[1], Counts[2]);
            Check(Hero >= 0 && Hero < 3 && Counts[Hero] == 0, "sequential companion selection fills an absent role");
            ++Counts[Hero];
        }
        Check(Counts == std::array<int, 3>{1, 1, 1}, "solo fill produces all three roles after caller updates counts");
    }
}

void DecisionTests()
{
    using AlphaCompanionRules::Action;
    using AlphaCompanionRules::ChooseAction;
    using AlphaCompanionRules::Decision;
    // Specification table indexed by bits: CanAct, RunActive, Danger, Downed, Enemy, Human.
    // This explicit 64-entry table checks all decisions, including irrelevant lower-priority flags.
    const std::array<Action, 64> Expected{
        Action::Idle, Action::Idle, Action::Idle, Action::Idle,
        Action::Idle, Action::Idle, Action::Idle, Action::Evade,
        Action::Idle, Action::Idle, Action::Idle, Action::Revive,
        Action::Idle, Action::Idle, Action::Idle, Action::Evade,
        Action::Idle, Action::Idle, Action::Idle, Action::Attack,
        Action::Idle, Action::Idle, Action::Idle, Action::Evade,
        Action::Idle, Action::Idle, Action::Idle, Action::Revive,
        Action::Idle, Action::Idle, Action::Idle, Action::Evade,
        Action::Idle, Action::Follow, Action::Idle, Action::Follow,
        Action::Idle, Action::Follow, Action::Idle, Action::Evade,
        Action::Idle, Action::Follow, Action::Idle, Action::Revive,
        Action::Idle, Action::Follow, Action::Idle, Action::Evade,
        Action::Idle, Action::Follow, Action::Idle, Action::Attack,
        Action::Idle, Action::Follow, Action::Idle, Action::Evade,
        Action::Idle, Action::Follow, Action::Idle, Action::Revive,
        Action::Idle, Action::Follow, Action::Idle, Action::Evade,
    };
    for (unsigned Mask = 0; Mask < Expected.size(); ++Mask)
    {
        const Decision State{(Mask & 1U) != 0, (Mask & 2U) != 0, (Mask & 4U) != 0,
            (Mask & 8U) != 0, (Mask & 16U) != 0, (Mask & 32U) != 0};
        Check(ChooseAction(State) == Expected[Mask], "companion priority table combination " + std::to_string(Mask));
    }
    Check(ChooseAction({true, true, false, true, true, false}) == Action::Revive,
        "living bot still rescues when all humans are down");
    Check(ChooseAction({false, true, true, true, true, true}) == Action::Idle,
        "downed or action-locked bot cannot rescue attack or evade");
    Check(ChooseAction({true, true, true, true, true, true}) == Action::Evade,
        "immediate danger takes precedence over starting rescue");
}

void RangeTests()
{
    using AlphaCompanionRules::InMeleeRange;
    Check(InMeleeRange(0, 0, 0), "melee includes coincident positions");
    for (double XSign : {-1.0, 1.0})
        for (double YSign : {-1.0, 1.0})
            for (double ZSign : {-1.0, 1.0})
                Check(InMeleeRange(145 * XSign, 65 * YSign, 95 * ZSign), "melee includes every XYZ boundary corner");
    Check(!InMeleeRange(145.001, 0, 0) && !InMeleeRange(-145.001, 0, 0), "melee excludes both horizontal outer edges");
    Check(!InMeleeRange(0, 65.001, 0) && !InMeleeRange(0, -65.001, 0), "melee excludes both depth outer edges");
    Check(!InMeleeRange(0, 0, 95.001) && !InMeleeRange(0, 0, -95.001), "melee excludes both height outer edges");
    for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()})
    {
        Check(!InMeleeRange(Invalid, 0, 0), "invalid or extreme X is outside melee");
        Check(!InMeleeRange(0, Invalid, 0), "invalid or extreme Y is outside melee");
        Check(!InMeleeRange(0, 0, Invalid), "invalid or extreme Z is outside melee");
    }
}


void FootprintTests()
{
    using AlphaCompanionRules::InSkillFootprint;
    constexpr double Reach[3][3]{{110, 240, 360}, {200, 0, 0}, {0, 0, 0}};
    constexpr double Radius[3][3]{{100, 110, 180}, {85, 210, 300}, {420, 310, 460}};
    for (int Hero = 0; Hero < 3; ++Hero)
    {
        for (int Slot = 0; Slot < 3; ++Slot)
        {
            const double Length = Reach[Hero][Slot];
            const double R = Radius[Hero][Slot];
            const double Center = Length / 2;
            Check(InSkillFootprint(Hero, Slot, 0, 0, 0), "skill footprint includes origin");
            Check(InSkillFootprint(Hero, Slot, Length, 0, 0), "skill footprint includes far segment center");
            Check(InSkillFootprint(Hero, Slot, Length + R, 0, 0), "skill footprint includes precise forward cap endpoint");
            Check(!InSkillFootprint(Hero, Slot, Length + R + .001, 0, 0), "skill footprint excludes beyond forward cap");
            Check(InSkillFootprint(Hero, Slot, -R, 0, 0), "skill footprint includes exact rear sphere-cap boundary");
            Check(!InSkillFootprint(Hero, Slot, -R - .001, 0, 0), "skill footprint excludes beyond rear cap");
            for (double Sign : {-1.0, 1.0})
            {
                Check(InSkillFootprint(Hero, Slot, Center, Sign * R, 0), "skill footprint includes exact depth boundary");
                Check(!InSkillFootprint(Hero, Slot, Center, Sign * (R + .001), 0), "skill footprint rejects depth-misaligned target");
                Check(InSkillFootprint(Hero, Slot, Center, 0, Sign * R), "skill footprint includes exact height boundary");
                Check(!InSkillFootprint(Hero, Slot, Center, 0, Sign * (R + .001)), "skill footprint rejects target outside height");
            }
            Check(InSkillFootprint(Hero, Slot, Center, R * .6, R * .8), "skill footprint includes exact depth-height circle edge");
            Check(!InSkillFootprint(Hero, Slot, Center, R * .8, R * .8), "skill footprint is round not a depth-height box");
            Check(!InSkillFootprint(Hero, Slot, Length + R * .8, R * .8, 0), "skill footprint cap does not use an axis-aligned box");
            for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()})
            {
                Check(!InSkillFootprint(Hero, Slot, Invalid, 0, 0), "nonfinite or extreme forward position fails closed");
                Check(!InSkillFootprint(Hero, Slot, Center, Invalid, 0), "nonfinite or extreme depth fails closed");
                Check(!InSkillFootprint(Hero, Slot, Center, 0, Invalid), "nonfinite or extreme height fails closed");
            }
            if (Length > 0)
            {
                Check(InSkillFootprint(Hero, Slot, Length, R * .6, R * .8), "front skill includes rounded edge at far segment endpoint");
                Check(!InSkillFootprint(Hero, Slot, -R * .8, R * .8, 0), "rear cap excludes diagonal outside sphere");
            }
            else
            {
                Check(InSkillFootprint(Hero, Slot, -R * .6, R * .8, 0), "radial skill has symmetric rear coverage");
                Check(!InSkillFootprint(Hero, Slot, -R * .8, R * .8, 0), "radial skill excludes rear diagonal outside sphere");
            }
        }
    }
    for (int Invalid : {-1, 3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        Check(!InSkillFootprint(Invalid, 0, 0, 0, 0), "invalid hero cannot index footprint table");
    for (int Invalid : {-1, 3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        Check(!InSkillFootprint(0, Invalid, 0, 0, 0), "invalid slot cannot index footprint table");
}

void SkillTests()
{
    using AlphaCompanionRules::ShouldUseSkill;
    constexpr double Ranges[3][3]{{190, 300, 500}, {240, 200, 290}, {400, 290, 450}};
    for (int Hero = 0; Hero < 3; ++Hero)
    {
        for (int Slot = 0; Slot < 3; ++Slot)
        {
            const double Range = Ranges[Hero][Slot];
            Check(ShouldUseSkill(Hero, Slot, 0, true, 0, true, 100), "skill accepts coincident enemy");
            Check(ShouldUseSkill(Hero, Slot, Range - .001, true, 0, true, 100), "skill accepts just inside range");
            Check(ShouldUseSkill(Hero, Slot, Range, true, 0, true, 100), "skill accepts exact range edge");
            Check(!ShouldUseSkill(Hero, Slot, Range + .001, true, 0, true, 100), "skill excludes outside range");
            Check(ShouldUseSkill(Hero, Slot, Range, true, -1, true, 100), "expired cooldown permits skill");
            Check(!ShouldUseSkill(Hero, Slot, Range, true, .001, true, 100), "positive remaining cooldown blocks skill");
            Check(!ShouldUseSkill(Hero, Slot, Range, false, 0, true, 100), "no enemy blocks skill");
            Check(!ShouldUseSkill(Hero, Slot, Range, true, 0, false, 100), "inability to act blocks skill");
            Check(!ShouldUseSkill(Hero, Slot, -.001, true, 0, true, 100), "negative distance blocks skill");
            Check(!ShouldUseSkill(Hero, Slot, std::numeric_limits<double>::max(), true, 0, true, 100), "extreme finite distance blocks skill");
            for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity()})
            {
                Check(!ShouldUseSkill(Hero, Slot, Invalid, true, 0, true, 100), "nonfinite distance blocks skill");
                Check(!ShouldUseSkill(Hero, Slot, Range, true, Invalid, true, 100), "nonfinite cooldown blocks skill");
            }
        }
    }
    for (int Invalid : {-1, 3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        Check(!ShouldUseSkill(Invalid, 0, 0, true, 0, true, 100), "invalid hero cannot index skill range table");
    for (int Invalid : {-1, 3, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        Check(!ShouldUseSkill(0, Invalid, 0, true, 0, true, 100), "invalid slot cannot index skill range table");
}

void SkillEnergyTests()
{
    using AlphaCompanionRules::ShouldUseSkill;
    for (int Hero = 0; Hero < 3; ++Hero)
    {
        Check(ShouldUseSkill(Hero, 0, 0, true, 0, true), "default zero energy permits U");
        Check(!ShouldUseSkill(Hero, 1, 0, true, 0, true) && !ShouldUseSkill(Hero, 2, 0, true, 0, true),
            "default zero energy blocks I and O");
        for (int Slot = 0; Slot < 3; ++Slot)
        {
            for (double Energy : {0.0, 9.0, 10.0, 99.0, 100.0})
            {
                const bool Allowed = Slot == 0 || (Slot == 1 ? Energy >= 10 : Energy == 100);
                Check(ShouldUseSkill(Hero, Slot, 0, true, 0, true, Energy) == Allowed,
                    "AI resource gate matches U I O energy contract");
                Check(!ShouldUseSkill(Hero, Slot, 0, true, .01, true, Energy), "AI cannot bypass cooldown at any energy");
                Check(!ShouldUseSkill(Hero, Slot, 0, true, 0, false, Energy), "AI cannot bypass action lock at any energy");
            }
            for (double Invalid : {-1.0, 100.001, std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
                Check(!ShouldUseSkill(Hero, Slot, 0, true, 0, true, Invalid), "AI cannot cast from invalid resource state");
        }
    }
}

}

int main()
{
    FillTests();
    HeroTests();
    DecisionTests();
    RangeTests();
    SkillTests();
    FootprintTests();
    SkillEnergyTests();
    std::cout << "Portable companion rules: " << Checks - Failures << '/' << Checks << " checks passed\n";
    std::cout << "Unreal spawning, AI movement, replication, revival and Windows gameplay remain separate tests.\n";
    return Failures == 0 ? 0 : 1;
}
