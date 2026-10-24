#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QRWorldEditorLibrary.generated.h"
class ALandscape;
class UStaticMesh;
class UMaterialInterface;

/** Thin editor-only access to native Landscape and serialized spline/HISM components. */
UCLASS()
class ROADTOWISDOMEDITOR_API UQRWorldEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="QuietRoad|World")
    static bool NewPartitionedWorld();
    UFUNCTION(BlueprintCallable, Category="QuietRoad|World")
    static ALandscape* ImportLandscape(const FString& RawHeightFile, int32 Resolution,
        float SpacingCm, UMaterialInterface* Material);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|World")
    static AActor* CreateRoad(const FString& Label, const TArray<FVector>& Points,
        UStaticMesh* Mesh, UMaterialInterface* Material, float HalfWidthCm);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|World")
    static AActor* CreateInstances(const FString& Label, const FVector& CellOrigin,
        UStaticMesh* Mesh, UMaterialInterface* Material, const TArray<FTransform>& Transforms,
        bool bCollision, float CullDistance);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|World")
    static bool SetSpatialLoading(AActor* Actor, bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|World")
    static bool LoadEditorRegion(FVector Center, FVector Extent, bool bLoad);
    UFUNCTION(BlueprintCallable, Category="QuietRoad|World")
    static UObject* GetPartitionSettings();
};
