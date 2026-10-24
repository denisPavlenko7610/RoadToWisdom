#include "WorldGeneration/QRWorldEditorLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/GameModeBase.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSubsystem.h"
#include "Misc/FileHelper.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionEditorPerProjectUserSettings.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"

bool UQRWorldEditorLibrary::NewPartitionedWorld()
{
    UWorld* World=GEditor ? GEditor->NewMap(true) : nullptr;
    if (!World || !World->GetWorldPartition()) return false;
    World->GetWorldPartition()->SetEnableStreaming(true);
    World->GetWorldSettings()->DefaultGameMode=AGameModeBase::StaticClass();
    return true;
}

ALandscape* UQRWorldEditorLibrary::ImportLandscape(const FString& RawHeightFile,int32 Resolution,
    float SpacingCm,UMaterialInterface* Material)
{
    UWorld* World=GEditor->GetEditorWorldContext().World();
    if (!World || !World->GetWorldPartition() || Resolution<127 || (Resolution-1)%126!=0) return nullptr;
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes,*RawHeightFile) || Bytes.Num()!=Resolution*Resolution*2) return nullptr;
    TArray<uint16> Heights;
    Heights.SetNumUninitialized(Resolution*Resolution);
    FMemory::Memcpy(Heights.GetData(),Bytes.GetData(),Bytes.Num());
    const float HalfSize=(Resolution-1)*SpacingCm*.5f;
    ALandscape* Landscape=World->SpawnActor<ALandscape>();
    Landscape->SetActorLabel(TEXT("QRW_Terrain"));
    Landscape->Tags={TEXT("QRWorld.Generated"),TEXT("QRWorld.Terrain")};
    Landscape->SetActorLocation(FVector(-HalfSize,-HalfSize,0));
    Landscape->SetActorScale3D(FVector(SpacingCm,SpacingCm,256.f));
    Landscape->LandscapeMaterial=Material;
    TMap<FGuid,TArray<uint16>> HeightLayers;
    HeightLayers.Add(FGuid(),MoveTemp(Heights));
    TMap<FGuid,TArray<FLandscapeImportLayerInfo>> WeightLayers;
    WeightLayers.Add(FGuid(),TArray<FLandscapeImportLayerInfo>());
    Landscape->Import(FGuid::NewGuid(),0,0,Resolution-1,Resolution-1,2,63,
        HeightLayers,*RawHeightFile,WeightLayers,ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>());
    ULandscapeInfo* Info=Landscape->GetLandscapeInfo();
    if (!Info) return nullptr;
    World->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(Info,2);
    Landscape->UpdateAllComponentMaterialInstances();
    Landscape->MarkPackageDirty();
    return Landscape;
}

AActor* UQRWorldEditorLibrary::CreateRoad(const FString& Label,const TArray<FVector>& Points,
    UStaticMesh* Mesh,UMaterialInterface* Material,float HalfWidthCm)
{
    if (Points.Num()<2 || !Mesh || !Material) return nullptr;
    UWorld* World=GEditor->GetEditorWorldContext().World();
    AActor* Actor=World->SpawnActor<AActor>();
    Actor->SetActorLabel(Label);
    Actor->Tags={TEXT("QRWorld.Generated"),TEXT("QRWorld.Roads")};
    Actor->SetFolderPath(TEXT("Generated/Roads"));
    auto* Spline=NewObject<USplineComponent>(Actor,TEXT("Route"));
    Actor->AddInstanceComponent(Spline);
    Actor->SetRootComponent(Spline);
    Actor->SetActorLocation(Points[0]);
    Spline->SetMobility(EComponentMobility::Static);
    Spline->RegisterComponent();
    Spline->ClearSplinePoints(false);
    for (const FVector& Point:Points) Spline->AddSplinePoint(Point,ESplineCoordinateSpace::World,false);
    Spline->UpdateSpline();
    // Each segment follows the sampled landscape bed. Tangents are local, unscaled
    // spline derivatives; width is relative to the unit plane's 100 cm footprint.
    for (int32 I=0;I<Points.Num()-1;++I)
    {
        auto* Segment=NewObject<USplineMeshComponent>(Actor);
        Actor->AddInstanceComponent(Segment);
        Segment->SetupAttachment(Spline);
        Segment->SetMobility(EComponentMobility::Static);
        Segment->SetStaticMesh(Mesh);
        Segment->SetMaterial(0,Material);
        Segment->SetForwardAxis(ESplineMeshAxis::X,false);
        Segment->SetSplineUpDir(FVector::UpVector,false);
        Segment->SetStartScale(FVector2D(HalfWidthCm/50.f,1),false);
        Segment->SetEndScale(FVector2D(HalfWidthCm/50.f,1),false);
        Segment->SetStartAndEnd(Spline->GetLocationAtSplinePoint(I,ESplineCoordinateSpace::Local),
            Spline->GetTangentAtSplinePoint(I,ESplineCoordinateSpace::Local),
            Spline->GetLocationAtSplinePoint(I+1,ESplineCoordinateSpace::Local),
            Spline->GetTangentAtSplinePoint(I+1,ESplineCoordinateSpace::Local),false);
        // Landscape provides smooth physical contact; the ribbon must not make seams.
        Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Segment->RegisterComponent();
        Segment->UpdateMesh();
    }
    Actor->MarkPackageDirty();
    return Actor;
}

AActor* UQRWorldEditorLibrary::CreateInstances(const FString& Label,const FVector& CellOrigin,
    UStaticMesh* Mesh,UMaterialInterface* Material,const TArray<FTransform>& Transforms,
    bool bCollision,float CullDistance)
{
    if (!Mesh || Transforms.IsEmpty()) return nullptr;
    auto* Actor=GEditor->GetEditorWorldContext().World()->SpawnActor<AActor>();
    Actor->SetActorLabel(Label);
    Actor->Tags={TEXT("QRWorld.Generated"),Label.Contains(TEXT("Rock"))?FName(TEXT("QRWorld.Rocks")):FName(TEXT("QRWorld.Vegetation"))};
    Actor->SetFolderPath(TEXT("Generated/Nature"));
    auto* Instances=NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor);
    Actor->AddInstanceComponent(Instances);
    Actor->SetRootComponent(Instances);
    Actor->SetActorLocation(CellOrigin);
    Instances->SetMobility(EComponentMobility::Static);
    Instances->SetStaticMesh(Mesh);
    if (Material) Instances->SetMaterial(0,Material);
    Instances->SetCollisionEnabled(bCollision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    Instances->SetCollisionResponseToAllChannels(ECR_Block);
    Instances->SetCullDistances(CullDistance*.8f,CullDistance);
    Instances->RegisterComponent();
    Instances->AddInstances(Transforms,false,true,false);
    Actor->MarkPackageDirty();
    return Actor;
}

bool UQRWorldEditorLibrary::SetSpatialLoading(AActor* Actor,bool bEnabled)
{
    if (!Actor) return false;
    Actor->SetIsSpatiallyLoaded(bEnabled);
    Actor->MarkPackageDirty();
    return true;
}

bool UQRWorldEditorLibrary::LoadEditorRegion(FVector Center,FVector Extent,bool bLoad)
{
    static TWeakObjectPtr<UWorldPartitionEditorLoaderAdapter> Loader;
    static TWeakObjectPtr<UWorldPartition> Owner;
    UWorld* World=GEditor->GetEditorWorldContext().World();
    UWorldPartition* Partition=World?World->GetWorldPartition():nullptr;
    if (!Partition) return false;
    if (Loader.IsValid() && Owner.IsValid())
    {
        Owner->ReleaseEditorLoaderAdapter(Loader.Get());
        Loader.Reset();
    }
    if (bLoad)
    {
        Loader=Partition->CreateEditorLoaderAdapter<FLoaderAdapterShape>(
            World,FBox(Center-Extent,Center+Extent),TEXT("Quiet Road editor working region"));
        Owner=Partition;
        Loader->GetLoaderAdapter()->Load();
    }
    if (!IsRunningCommandlet())
    {
        auto* Settings=GetMutableDefault<UWorldPartitionEditorPerProjectUserSettings>();
        Settings->SetEditorLoadedRegions(World,bLoad?TArray<FBox>{FBox(Center-Extent,Center+Extent)}:TArray<FBox>{});
        Settings->SaveConfig();
    }
    return true;
}

UObject* UQRWorldEditorLibrary::GetPartitionSettings()
{
    UWorld* World=GEditor->GetEditorWorldContext().World();
    return World && World->GetWorldPartition()?World->GetWorldPartition()->RuntimeHash.Get():nullptr;
}
