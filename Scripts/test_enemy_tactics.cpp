#include "Rules/EnemyTacticsRules.h"
#include <iostream>
#include <limits>

namespace { int Checks=0,Failures=0; void Check(bool Condition,const char* Name){++Checks;if(!Condition){++Failures;std::cerr<<"FAIL: "<<Name<<'\n';}} }
int main()
{
    using namespace AlphaEnemyRules;
    for(int Chapter=-1;Chapter<4;++Chapter)
        for(int Wave=0;Wave<5;++Wave)
        {
            int Guns=0;
            for(int Index=0;Index<12;++Index)
            {
                const int Role=RoleForEncounter(Chapter,Wave,Index);
                Check(ValidRole(Role),"encounter roles remain in supported range");
                if(Role==2)++Guns;
                if(Chapter==0&&Wave==1)Check(Role==0,"tutorial first wave contains only melee enemies");
            }
            Check(Guns<=2,"each encounter caps gunner count at two");
        }
    Check(!ValidRole(-1)&&!ValidRole(3),"invalid role rejected");
    Check(RoleForEncounter(0,2,-1)==0,"negative spawn index is safe");
    Check(HasAttackSlot(0)&&HasAttackSlot(1),"first two attackers may claim a target");
    Check(!HasAttackSlot(-1)&&!HasAttackSlot(2)&&!HasAttackSlot(100),"invalid or saturated target denies attack");
    Check(ShieldBlocks(true,.5,100,160),"shield front cone and height exact boundary");
    Check(!ShieldBlocks(true,.49999,100,0),"flank bypasses shield");
    Check(!ShieldBlocks(true,-1,100,0),"rear bypasses shield");
    Check(!ShieldBlocks(false,1,100,0),"open shield cannot reduce damage");
    Check(!ShieldBlocks(true,1,0,0),"cross-up at zero horizontal distance bypasses shield");
    Check(!ShieldBlocks(true,1,100,160.01),"high aerial cross-up bypasses shield");
    Check(GuardedDamage(20,true)==5&&GuardedDamage(20,false)==20,"shield health mitigation is exactly one quarter");
    Check(InGunnerLine(0,48,90)&&InGunnerLine(760,-48,-90),"gunner exact line boundaries hit");
    Check(!InGunnerLine(-.01,0,0)&&!InGunnerLine(760.01,0,0),"line does not hit behind or past its end");
    Check(!InGunnerLine(400,48.01,0),"depth dodge leaves gunner footprint");
    Check(!InGunnerLine(400,0,90.01),"jump clears gunner ground shot");
    Check(ShouldShoot(250,160,true)&&ShouldShoot(700,0,true),"gunner engages only at intentional medium range");
    Check(!ShouldShoot(249,0,true)&&!ShouldShoot(701,0,true)&&!ShouldShoot(500,0,false),"close far and obstructed shots rejected");
    Check(TellProgress(0,0,.85)==0&&TellProgress(.85,0,.85)==1,"tell progress exact start and release");
    Check(TellProgress(-1,0,.85)==0&&TellProgress(2,0,.85)==1,"tell progress clamps outside lifetime");
    Check(TellProgress(1,1,1)==0,"zero duration tell rejected");
    for(double Invalid:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity()})
    {
        Check(!ShieldBlocks(true,Invalid,100,0),"nonfinite shield direction rejected");
        Check(!ShieldBlocks(true,1,Invalid,0),"nonfinite shield distance rejected");
        Check(!ShieldBlocks(true,1,100,Invalid),"nonfinite shield height rejected");
        Check(!InGunnerLine(Invalid,0,0)&&!InGunnerLine(0,Invalid,0)&&!InGunnerLine(0,0,Invalid),"nonfinite projectile footprint rejected");
        Check(!ShouldShoot(Invalid,0,true)&&!ShouldShoot(500,Invalid,true),"nonfinite firing condition rejected");
        Check(TellProgress(Invalid,0,1)==0&&TellProgress(1,Invalid,2)==0&&TellProgress(1,0,Invalid)==0,"nonfinite telegraph clock rejected");
    }
    std::cout<<"Portable enemy tactics: "<<Checks-Failures<<'/'<<Checks<<" checks passed\n";
    std::cout<<"Unreal timing, shield integration, line traces, replication and visual tells remain separate tests.\n";
    return Failures?1:0;
}
