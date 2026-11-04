#include "Rules/LevelLayoutRules.h"
#include <iostream>
#include <limits>
#include <string>

namespace { int Checks=0,Failures=0;
void Check(bool Value,const std::string& Name) { ++Checks;if(!Value){++Failures;std::cerr<<"FAIL: "<<Name<<'\n';} }
}
int main()
{
    using namespace AlphaLayoutRules;
    const double NaN=std::numeric_limits<double>::quiet_NaN(),Infinity=std::numeric_limits<double>::infinity();
    for(int Chapter=0;Chapter<3;++Chapter)
    {
        Check(FloorCount(Chapter)==(Chapter==2?7:5),"planned chapter floors");
        Check(IsWalkable(Chapter,{-1000,0},45.),"NPC approach is safe");
        Check(IsWalkable(Chapter,{-650,-100},45.),"human spawn safe");
        Check(!IsWalkable(Chapter,{900,450}),"connector has real negative space");
        Check(IsWalkable(Chapter,{900,RouteCenterY(Chapter,900)},45.),"connector center has capsule room");
        Check(!IsWalkable(Chapter,{NaN,0}),"NaN point rejected");
        Check(!IsWalkable(Chapter,{0,Infinity}),"infinite point rejected");
        Check(!IsWalkable(Chapter,{0,0},-1),"negative margin rejected");
        Check(!IsWalkable(Chapter,{0,0},NaN),"NaN margin rejected");
        Check(IsWalkable(Chapter,Project(Chapter,{Infinity,0}),45.),"invalid projection has safe fallback");
        Check(IsWalkable(Chapter,Project(Chapter,{1e308,-1e308}),45.),"extreme point safe projection");
        Check(IsWalkable(Chapter,Project(Chapter,{2499,0},120),120.),"thin exit cannot violate clamp range");
        const auto Tiles=FloorTiles(Chapter);
        Check(!Tiles.empty(),"non-overlapping floor tiles exist");
        for(std::size_t I=0;I<Tiles.size();++I)
        {
            const Rect A=Tiles[I];
            Check(IsWalkable(Chapter,{(A.MinX+A.MaxX)*.5,(A.MinY+A.MaxY)*.5}),"tile center is in route");
            for(std::size_t J=I+1;J<Tiles.size();++J)
            {
                const Rect B=Tiles[J];
                Check(std::min(A.MaxX,B.MaxX)<=std::max(A.MinX,B.MinX)
                    || std::min(A.MaxY,B.MaxY)<=std::max(A.MinY,B.MinY),"tiles do not z fight");
            }
        }
        for(double X=-1500;X<=2600;X+=100)
            for(double Y=-750;Y<=750;Y+=100)
            {
                const Point P{X,Y},Q=Project(Chapter,P);
                Check(IsWalkable(Chapter,Q,45.),"grid projection stays safe");
                if(IsWalkable(Chapter,P,45.)) Check(DistanceSquared(P,Q)<1e-12,"valid union point is unchanged");
                bool TileContains=false;
                for(Rect R:Tiles) if(X>=R.MinX&&X<=R.MaxX&&Y>=R.MinY&&Y<=R.MaxY) TileContains=true;
                Check(TileContains==Contains(Chapter,P),"render/collision slabs equal planned union");
            }
        const auto& Profile=ElevationProfile(Chapter);
        Check(Profile.size()>=6,"chapter has several distinct elevation sections");
        Check(SurfaceHeight(Chapter,{-650,-100})==0.&&SurfaceHeight(Chapter,{-290,100})==0.,"all three human starts remain on level apron");
        double Peak=0.;
        for(std::size_t I=1;I<Profile.size();++I)
        {
            Check(Profile[I].X>Profile[I-1].X,"height profile is ordered");
            const double Slope=(Profile[I].Z-Profile[I-1].Z)/(Profile[I].X-Profile[I-1].X);
            Check(std::abs(Slope)<=.400001,"ramps stay below ordinary character walkable slope");
            Check(std::abs(SurfaceHeight(Chapter,{Profile[I].X-.001,0})-SurfaceHeight(Chapter,{Profile[I].X+.001,0}))<.001,"ramp joins are continuous");
            Peak=std::max(Peak,Profile[I].Z);
        }
        Check(Peak>=120.,"height changes are visible at gameplay scale");
        const auto Terrain=TerrainTiles(Chapter);
        Check(Terrain.size()>Tiles.size(),"floor collision is split at slope changes");
        for(const TerrainTile& T:Terrain)
        {
            const Rect R=T.Bounds;
            const double MidX=(R.MinX+R.MaxX)*.5;
            Check(std::abs(SurfaceHeight(Chapter,{MidX,(R.MinY+R.MaxY)*.5})-(T.StartZ+T.EndZ)*.5)<1e-9,"collision top interpolates same height as shared rules");
        }
        for(double X=-1375;X<2500;X+=50)
            for(double Y=-613;Y<=637;Y+=125)
            {
                int Coverage=0;
                for(const TerrainTile& T:Terrain)
                    if(X>T.Bounds.MinX&&X<T.Bounds.MaxX&&Y>T.Bounds.MinY&&Y<T.Bounds.MaxY) ++Coverage;
                // Grid deliberately avoids all authored tile edges.
                Check(Coverage==(Contains(Chapter,{X,Y})?1:0),"raised floor covers the route exactly once and leaves void empty");
            }
        const auto Edges=BoundarySegments(Chapter);
        Check(!Edges.empty(),"outer collision boundary exists");
        for(const Segment& E:Edges)
        {
            const Point Mid{(E.A.X+E.B.X)*.5,(E.A.Y+E.B.Y)*.5};
            const bool Vertical=E.A.X==E.B.X;
            const Point Left=Vertical?Point{Mid.X-.2,Mid.Y}:Point{Mid.X,Mid.Y-.2};
            const Point Right=Vertical?Point{Mid.X+.2,Mid.Y}:Point{Mid.X,Mid.Y+.2};
            Check(Contains(Chapter,Left)!=Contains(Chapter,Right),"boundary separates floor from void");
        }
        for(int Wave=1;Wave<=(Chapter==1?3:2);++Wave)
        {
            Check(IsWalkable(Chapter,EncounterAnchor(Chapter,Wave),65.),"encounter anchor safe");
            for(int I=0;I<24;++I) Check(IsWalkable(Chapter,EnemySpawn(Chapter,Wave,I),65.),"enemy point safe");
        }
        const Point Goals[]={{-1000,200},{2300,0},{0,300},{1800,-450},{1900,450}};
        for(Point Goal:Goals)
        {
            Point Current=Project(Chapter,{-1100,-150});Goal=Project(Chapter,Goal);
            for(int Hop=0;Hop<20&&DistanceSquared(Current,Goal)>1.;++Hop)
            {
                Point Next=NextWaypoint(Chapter,Current,Goal);
                Check(SegmentClear(Chapter,Current,Next),"waypoint avoids outer collision edges");
                Check(DistanceSquared(Current,Next)>1.,"routing advances rather than loops");
                Current=Next;
            }
            Check(DistanceSquared(Current,Goal)<1.,"route reaches projected destination");
            Point Back=NextWaypoint(Chapter,Goal,{-1000,0});
            Check(SegmentClear(Chapter,Goal,Back),"return route to NPC starts safely");
        }
    }
    Check(IsWalkable(2,{1500,-500},45.),"canal front garden is playable");
    Check(IsWalkable(2,{1900,500},45.),"canal rear garden is playable");
    Check(!IsWalkable(0,{1500,-500}),"dock footprint differs from canal");
    Check(IsWalkable(1,{700,-200},45.)&&!IsWalkable(1,{700,200}),"forge side corridor is asymmetric");
    Check(SurfaceHeight(0,{200,385})==80.,"cargo approach climbs toward the raised deck");
    Check(std::abs(SurfaceHeight(1,{200,385})-200./3.)<1e-9,"forge has its own height profile");
    Check(SurfaceHeight(-1,{0,0})==0.&&SurfaceHeight(2,{NaN,0})==0.,"invalid height inputs fail safely");
    Check(TerrainTiles(-1).empty(),"invalid chapter has no raised terrain");
    Check(!SegmentClear(2,{200,300},{1900,500}),"direct diagonal cannot cut bridge void");
    Check(FloorCount(-1)==0&&FloorCount(3)==0,"invalid floor chapters fail closed");
    Check(!IsWalkable(-1,{0,0})&&!IsWalkable(3,{0,0}),"invalid walk chapters fail closed");
    Check(BoundarySegments(-1).empty()&&FloorTiles(3).empty(),"invalid geometry is empty");
    std::cout<<"Portable level layout rules: "<<Checks-Failures<<'/'<<Checks<<" checks passed\n";
    std::cout<<"Unreal collision, navigation, rendering and LAN remain separate tests.\n";
    return Failures?1:0;
}
