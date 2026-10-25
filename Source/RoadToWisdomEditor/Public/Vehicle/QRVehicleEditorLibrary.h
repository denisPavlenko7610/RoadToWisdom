#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRVehicleEditorLibrary.generated.h"
class UStaticMesh;
class USkeletalMesh;
class UAnimBlueprint;
/** Editor asset creation for the supplied undamaged utility van. */
UCLASS()
class ROADTOWISDOMEDITOR_API UQRVehicleEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="QuietRoad|Vehicle")
    static USkeletalMesh* BuildVanRig(UStaticMesh* Source, const FString& AssetFolder, const TArray<FVector>& WheelCenters);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|Vehicle")
    static UAnimBlueprint* BuildVanAnimation(USkeletalMesh* Mesh, const FString& AssetPath);
};
