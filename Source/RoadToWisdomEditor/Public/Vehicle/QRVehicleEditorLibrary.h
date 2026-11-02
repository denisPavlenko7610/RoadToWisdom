#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRVehicleEditorLibrary.generated.h"
class UStaticMesh;
class USkeletalMesh;
class UAnimBlueprint;
class USkinnedMeshComponent;
/** Editor asset creation for the supplied undamaged utility van. */
UCLASS()
class ROADTOWISDOMEDITOR_API UQRVehicleEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Repair the generated mesh LOD bone lists from its actual skin sections. */
    UFUNCTION(BlueprintCallable, Category="QuietRoad|Vehicle")
    static bool RepairVanActiveBones(USkeletalMesh* Mesh);
    /** Test-only CPU skin readback, in component space. */
    UFUNCTION(BlueprintCallable, Category="QuietRoad|Vehicle")
    static TArray<FVector> GetSkinnedVertexPositions(USkinnedMeshComponent* Component);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|Vehicle")
    static USkeletalMesh* BuildVanRig(UStaticMesh* Source, const FString& AssetFolder, const TArray<FVector>& WheelCenters);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|Vehicle")
    static UAnimBlueprint* BuildVanAnimation(USkeletalMesh* Mesh, const FString& AssetPath);
};
