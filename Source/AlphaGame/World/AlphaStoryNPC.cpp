#include "World/AlphaStoryNPC.h"

#include "Characters/AlphaCharacter.h"
#include "Combat/AlphaCombatComponent.h"
#include "Rules/StoryRules.h"
#include "Visuals/AlphaArtLibrary.h"
#include "World/AlphaGameState.h"
#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

AAlphaStoryNPC::AAlphaStoryNPC()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetActorEnableCollision(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StoryGuideRoot"));
    Tags.Add(TEXT("StoryNPC"));
}

void AAlphaStoryNPC::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() != NM_DedicatedServer) BuildAppearance();
}

FText AAlphaStoryNPC::GetSpeakerName(int32 Chapter)
{
    switch (Chapter)
    {
    case 0: return FText::FromString(TEXT("渡伯 · 灰渡摆渡人"));
    case 1: return FText::FromString(TEXT("穗 · 工坊记录员"));
    case 2: return FText::FromString(TEXT("澄 · 水道守灯人"));
    default: return FText::GetEmpty();
    }
}

int32 AAlphaStoryNPC::GetPageCount(int32 Chapter)
{
    return Chapter >= 0 && Chapter < 3 ? 3 : 0;
}

FText AAlphaStoryNPC::GetDialogue(int32 Chapter, int32 Page, bool bDebrief)
{
    if (Page < 0 || Page >= GetPageCount(Chapter)) return FText::GetEmpty();
    if (bDebrief)
    {
        static const TCHAR* Debriefs[3][3] = {
            {
                TEXT("渡口安静下来了。我在守卫留下的货箱里找到了工坊通行牌，还有一张被撕碎的押运记录。"),
                TEXT("编号对得上。那些被送进炉里的记忆，不全是自愿交出的。带着这张残页去见穗，她能替你们找出完整账册。"),
                TEXT("船已经备好。你们准备好后，就前往百炉工坊吧。那里的铸炉官不会让真相轻易离开。"),
            },
            {
                TEXT("铸炉官倒下了，原始账册也终于回到了我们手里。里面记录的不是燃料，是一个个人曾经活过的证据。"),
                TEXT("妹妹的编号、机关的改装命令，还有最后一批记忆的去向，全都指向城中的炉心。可这份账册也写着：水道的供水离不开它。"),
                TEXT("下一站是悬灯水道。守灯人澄正护着避难的人，她会告诉你们，怎样在停止抽取的同时，让这座城继续活下去。"),
            },
            {
                TEXT("信灯重新连上了。你们清出了护送的路，避难的人终于可以沿水道安全转移。谢谢你们没有丢下这些灯。"),
                TEXT("现在你们知道了：炉心不能被简单熄灭。终止强制抽取与维持城民生存，必须成为同一件事。账册会是与守炉者对质的证据。"),
                TEXT("这段灯路已经守住。旧城的下一程还在前方——带着他们的名字走下去，别让新的循环再靠遗忘与牺牲维持。"),
            },
        };
        return FText::FromString(Debriefs[Chapter][Page]);
    }
    static const TCHAR* Lines[3][3] = {
        {
            TEXT("归炉城的灯，烧的是人的记忆。起初人们只想忘掉痛苦，后来城门关了，谁也不能拒绝献出过去。"),
            TEXT("那夜炉心失控，散落的记忆成了烬，钻进人和机器里。码头这些失控的守卫，曾经也只是送货的人。"),
            TEXT("先清出渡口，回来告诉我。我会带你们去百炉工坊——记录员穗藏着那晚的押运账册，答案就在里面。"),
        },
        {
            TEXT("账上写着自愿捐献，可名字后面全是强制押送的印记。沈砚，这份清单里有你妹妹的编号。"),
            TEXT("阿绫，你的机关原本用来保存记忆，是铸炉官改了它。打倒他，取回封存在炉下的原始记录。"),
            TEXT("但别急着熄灭炉心。水道的水泵、伤者的暖灯，都靠它运转。回来找我，再去问守灯人澄怎么救这座城。"),
        },
        {
            TEXT("闻灯，你听见的那些声音，不全在怪物身上。这里每一盏灯，都替一个家留着尚未散去的记忆。"),
            TEXT("熄火能终止抽取，也会让活着的人失去水和热。我们要带着账册找到守炉者，逼他交出新的循环之法。"),
            TEXT("先清理水道，让信灯重新连成一线。回来报平安，我会把通向旧城的灯路交给你们。记住：忘记，不能抵消责任。"),
        },
    };
    return FText::FromString(Lines[Chapter][Page]);
}

bool AAlphaStoryNPC::CanTalk(AAlphaCharacter* Speaker) const
{
    if (!GetWorld() || GetNetMode() == NM_Client || !HasAuthority() || !IsValid(Speaker)
        || Speaker->GetWorld() != GetWorld() || !Speaker->HasAuthority()
        || !Cast<APlayerController>(Speaker->GetController())) return false;
    const UAlphaCombatComponent* Combat = Speaker->GetCombatComponent();
    const AAlphaGameState* State = GetWorld()->GetGameState<AAlphaGameState>();
    if (!Combat || Combat->TeamId != 1 || !Combat->IsAlive() || Speaker->IsDefeated()
        || !State || State->ChapterIndex != ChapterIndex || ChapterIndex < 0 || ChapterIndex >= 3) return false;
    const FVector Offset = Speaker->GetActorLocation() - GetActorLocation();
    if (Offset.ContainsNaN() || !AlphaStoryRules::WithinConversationRange(Offset.Size2D(), Offset.Z)) return false;
    FCollisionQueryParams Sight(SCENE_QUERY_STAT(AlphaStoryTalk), false, Speaker);
    Sight.AddIgnoredActor(this);
    // Aim above the feet so the floor itself is never the obstruction.
    return !GetWorld()->LineTraceTestByChannel(Speaker->GetActorLocation(),
        GetActorLocation() + FVector(0, 0, 110.f), ECC_Visibility, Sight);
}

void AAlphaStoryNPC::AddPart(EAlphaPrimitive Shape, const FVector& Location,
    const FVector& Dimensions, UMaterialInstanceDynamic* Material, const FRotator& Rotation)
{
    UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Part);
    Part->SetupAttachment(RootComponent);
    Part->SetMobility(EComponentMobility::Movable);
    Part->SetStaticMesh(AlphaArt::Mesh(Shape));
    Part->SetMaterial(0, Material);
    Part->SetRelativeLocation(Location);
    Part->SetRelativeRotation(Rotation);
    Part->SetRelativeScale3D(Dimensions / 100.f);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetGenerateOverlapEvents(false);
    Part->SetCanEverAffectNavigation(false);
    Part->RegisterComponent();
    Parts.Add(Part);
}

void AAlphaStoryNPC::BuildAppearance()
{
    const int32 Chapter = FMath::Clamp(ChapterIndex, 0, 2);
    const FLinearColor Coats[] = {FLinearColor(.18f, .4f, .52f), FLinearColor(.65f, .25f, .2f), FLinearColor(.52f, .38f, .68f)};
    UMaterialInstanceDynamic* Coat = AlphaArt::Material(this, Coats[Chapter], .03f, .8f, 0.f, 1.f);
    UMaterialInstanceDynamic* Skin = AlphaArt::Material(this, FLinearColor(.8f, .61f, .43f), 0.f, .8f, 0.f, 1.f);
    UMaterialInstanceDynamic* Dark = AlphaArt::Material(this, FLinearColor(.1f, .13f, .18f), .05f, .8f, 0.f, 1.f);
    UMaterialInstanceDynamic* Paper = AlphaArt::Material(this, FLinearColor(.86f, .78f, .57f), 0.f, .9f, 0.f, 1.f);
    UMaterialInstanceDynamic* Signal = AlphaArt::EffectMaterial(this, FLinearColor(.35f, 1.f, .72f), .2f);
    if (Coat)
    {
        Coat->SetScalarParameterValue(TEXT("SurfaceStyle"), 3.f);
        Coat->SetScalarParameterValue(TEXT("TextureStrength"), .15f);
    }

    AddPart(EAlphaPrimitive::Cube, FVector(0, -10, 43), FVector(15, 15, 70), Dark);
    AddPart(EAlphaPrimitive::Cube, FVector(0, 10, 43), FVector(15, 15, 70), Dark);
    AddPart(EAlphaPrimitive::Cube, FVector(6, -10, 8), FVector(26, 17, 13), Dark);
    AddPart(EAlphaPrimitive::Cube, FVector(6, 10, 8), FVector(26, 17, 13), Dark);
    AddPart(EAlphaPrimitive::Cube, FVector(0, 0, 99), FVector(37, 40, 54), Coat);
    AddPart(EAlphaPrimitive::Cube, FVector(1, 0, 73), FVector(40, 43, 6), Paper);
    AddPart(EAlphaPrimitive::Cube, FVector(0, -27, 98), FVector(16, 15, 51), Coat, FRotator(-10, 0, -7));
    AddPart(EAlphaPrimitive::Cube, FVector(0, 27, 98), FVector(16, 15, 51), Coat, FRotator(-10, 0, 7));
    AddPart(EAlphaPrimitive::Sphere, FVector(0, 0, 143), FVector(33, 31, 36), Skin);
    AddPart(EAlphaPrimitive::Sphere, FVector(-5, 0, 154), FVector(32, 33, 21), Dark);
    for (float Side : {-1.f, 1.f})
    {
        AddPart(EAlphaPrimitive::Cube, FVector(16, Side * 7, 147), FVector(2, 4, 3), Dark);
        AddPart(EAlphaPrimitive::Sphere, FVector(5, Side * 28, 73), FVector(12, 12, 13), Skin);
    }
    // Friendly interaction marker, no combat/collision component or hostile health bar.
    AddPart(EAlphaPrimitive::Cube, FVector(0, 0, 199), FVector(7, 7, 21), Signal);
    AddPart(EAlphaPrimitive::Sphere, FVector(0, 0, 181), FVector(8, 8, 8), Signal);
    if (Chapter == 0)
    {
        AddPart(EAlphaPrimitive::Cone, FVector(0, 0, 168), FVector(65, 65, 18), Paper);
        AddPart(EAlphaPrimitive::Cylinder, FVector(13, -38, 87), FVector(5, 5, 170), Paper, FRotator(0, 0, -9));
    }
    else if (Chapter == 1)
    {
        AddPart(EAlphaPrimitive::Cube, FVector(27, 0, 104), FVector(5, 42, 32), Paper, FRotator(12, 0, 0));
        AddPart(EAlphaPrimitive::Cube, FVector(24, 0, 104), FVector(3, 45, 35), Dark, FRotator(12, 0, 0));
        AddPart(EAlphaPrimitive::Cube, FVector(17, 0, 149), FVector(2, 26, 2), Paper);
    }
    else
    {
        AddPart(EAlphaPrimitive::Cube, FVector(-4, 0, 64), FVector(34, 42, 32), Coat);
        AddPart(EAlphaPrimitive::Sphere, FVector(15, 35, 63), FVector(21, 21, 30), Signal);
        AddPart(EAlphaPrimitive::Cylinder, FVector(15, 35, 82), FVector(29, 29, 5), Dark);
        AddPart(EAlphaPrimitive::Cylinder, FVector(15, 35, 45), FVector(25, 25, 5), Dark);
    }
}
