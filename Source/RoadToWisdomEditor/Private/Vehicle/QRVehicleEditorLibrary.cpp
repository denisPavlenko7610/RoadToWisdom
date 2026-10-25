#include "Vehicle/QRVehicleEditorLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimBlueprint.h"
#include "ReferenceSkeleton.h"
#include "StaticToSkeletalMeshConverter.h"
#include "SkeletalMeshAttributes.h"
#include "StaticMeshAttributes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Misc/PackageName.h"
#include "Factories/AnimBlueprintFactory.h"
#include "VehicleAnimationInstance.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_MeshRefPose.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_WheelController.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraphNodeUtils.h"

USkeletalMesh* UQRVehicleEditorLibrary::BuildVanRig(UStaticMesh* Source,const FString& AssetFolder,const TArray<FVector>& Centers)
{
    if (!Source || Centers.Num()!=4 || !Source->GetMeshDescription(0)) return nullptr;
    const FString MeshPath=AssetFolder+TEXT("/Meshes/SK_UtilityVan");
    if (FindObject<USkeletalMesh>(nullptr,*(MeshPath+TEXT(".SK_UtilityVan")))) return nullptr;
    const EObjectFlags Flags=RF_Public|RF_Standalone|RF_Transactional;
    USkeleton* Skeleton=NewObject<USkeleton>(CreatePackage(*(AssetFolder+TEXT("/Meshes/SKEL_UtilityVan"))),TEXT("SKEL_UtilityVan"),Flags);
    USkeletalMesh* Mesh=NewObject<USkeletalMesh>(CreatePackage(*MeshPath),TEXT("SK_UtilityVan"),Flags);
    FReferenceSkeleton Ref;
    const FName Bones[]={TEXT("Wheel_FL"),TEXT("Wheel_FR"),TEXT("Wheel_BL"),TEXT("Wheel_BR")};
    {
        FReferenceSkeletonModifier Modifier(Ref,Skeleton);
        Modifier.Add(FMeshBoneInfo(TEXT("Root"),TEXT("Root"),INDEX_NONE),FTransform::Identity);
        for (int32 I=0;I<4;++I) Modifier.Add(FMeshBoneInfo(Bones[I],Bones[I].ToString(),0),FTransform(Centers[I]));
    }
    FMeshDescription Desc=*Source->GetMeshDescription(0);
    FSkeletalMeshAttributes Attributes(Desc);
    Attributes.Register(true);
    auto Weights=Attributes.GetVertexSkinWeights();
    auto Bind=[&Weights](FVertexID V,int32 Bone)
    {
        UE::AnimationCore::FBoneWeight W(Bone,1.f);
        Weights.Set(V,UE::AnimationCore::FBoneWeights::Create(MakeArrayView(&W,1)));
    };
    for (FVertexID V:Desc.Vertices().GetElementIDs()) Bind(V,0);
    const auto Slots=Attributes.GetPolygonGroupMaterialSlotNames();
    int32 WheelVertices[4]={};
    for (FTriangleID Triangle:Desc.Triangles().GetElementIDs())
    {
        const FString Slot=Slots[Desc.GetTrianglePolygonGroup(Triangle)].ToString();
        for (int32 I=0;I<4;++I) if (Slot.Contains(Bones[I].ToString()))
        {
            for (FVertexInstanceID VI:Desc.GetTriangleVertexInstances(Triangle))
            { Bind(Desc.GetVertexInstanceVertex(VI),I+1); ++WheelVertices[I]; }
        }
    }
    for (int32 I=0;I<4;++I) if (!WheelVertices[I])
    { UE_LOG(LogTemp,Error,TEXT("Van source lacks material group %s"),*Bones[I].ToString()); return nullptr; }
    TArray<FSkeletalMaterial> Materials;
    for (const FStaticMaterial& M:Source->GetStaticMaterials())
        Materials.Emplace(M.MaterialInterface,true,false,M.MaterialSlotName,M.ImportedMaterialSlotName);
    FStaticToSkeletalMeshConverter::FInitializationParams Params;
    Params.Materials=Materials;
    const FMeshDescription* Descs[]={&Desc};
    if (!FStaticToSkeletalMeshConverter::InitializeSkeletalMeshFromMeshDescriptions(Mesh,MakeArrayView(Descs),Ref,Params)) return nullptr;
    Mesh->SetSkeleton(Skeleton);
    Skeleton->MergeAllBonesToBoneTree(Mesh);
    Skeleton->SetPreviewMesh(Mesh);
    UPhysicsAsset* Physics=NewObject<UPhysicsAsset>(CreatePackage(*(AssetFolder+TEXT("/Physics/PHYS_UtilityVan"))),TEXT("PHYS_UtilityVan"),Flags);
    USkeletalBodySetup* Body=NewObject<USkeletalBodySetup>(Physics,NAME_None,RF_Transactional);
    Body->BoneName=TEXT("Root");
    FKBoxElem Box; Box.X=490; Box.Y=186; Box.Z=145; Box.Center=FVector(-3,0,110);
    Body->AggGeom.BoxElems.Add(Box);
    Body->PhysicsType=PhysType_Default;
    Body->CreatePhysicsMeshes();
    Physics->SkeletalBodySetups.Add(Body);
    Physics->UpdateBodySetupIndexMap();
    Physics->UpdateBoundsBodiesArray();
    Mesh->SetPhysicsAsset(Physics);
    for (UObject* Asset:TArray<UObject*>{Skeleton,Mesh,Physics})
    { FAssetRegistryModule::AssetCreated(Asset); Asset->MarkPackageDirty(); }
    return Mesh;
}

UAnimBlueprint* UQRVehicleEditorLibrary::BuildVanAnimation(USkeletalMesh* Mesh,const FString& Path)
{
    if (!Mesh || !Mesh->GetSkeleton()) return nullptr;
    UAnimBlueprintFactory* Factory=NewObject<UAnimBlueprintFactory>();
    Factory->ParentClass=UVehicleAnimationInstance::StaticClass();
    Factory->TargetSkeleton=Mesh->GetSkeleton();
    UAnimBlueprint* BP=Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),CreatePackage(*Path),*FPackageName::GetLongPackageAssetName(Path),RF_Public|RF_Standalone|RF_Transactional,nullptr,GWarn));
    if (!BP) return nullptr;
    UEdGraph* Graph=nullptr;
    for (UEdGraph* G:BP->FunctionGraphs) if (G->GetFName()==TEXT("AnimGraph")) Graph=G;
    if (!Graph)
    {
        Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("AnimGraph"),UAnimationGraph::StaticClass(),UAnimationGraphSchema::StaticClass());
        BP->FunctionGraphs.Add(Graph);
        Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
    }
    UAnimGraphNode_Root* Root=nullptr;
    for (UEdGraphNode* N:Graph->Nodes) if (auto* R=Cast<UAnimGraphNode_Root>(N)) Root=R;
    if (!Root) return nullptr;
    FGraphNodeCreator<UAnimGraphNode_MeshRefPose> RefCreator(*Graph);
    auto* Ref=RefCreator.CreateNode(); RefCreator.Finalize(); Ref->NodePosX=-600;
    FGraphNodeCreator<UAnimGraphNode_WheelController> WheelCreator(*Graph);
    auto* Wheel=WheelCreator.CreateNode(); WheelCreator.Finalize(); Wheel->NodePosX=-400;
    FGraphNodeCreator<UAnimGraphNode_ComponentToLocalSpace> LocalCreator(*Graph);
    auto* Local=LocalCreator.CreateNode(); LocalCreator.Finalize(); Local->NodePosX=-200;
    auto Connect=[Graph](UEdGraphNode* A,UEdGraphNode* B)
    {
        for (UEdGraphPin* Out:A->Pins) if (Out->Direction==EGPD_Output)
            for (UEdGraphPin* In:B->Pins) if (In->Direction==EGPD_Input && Graph->GetSchema()->TryCreateConnection(Out,In)) return true;
        return false;
    };
    if (!Connect(Ref,Wheel) || !Connect(Wheel,Local) || !Connect(Local,Root)) return nullptr;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status==BS_Error) return nullptr;
    FAssetRegistryModule::AssetCreated(BP);
    BP->MarkPackageDirty();
    return BP;
}
