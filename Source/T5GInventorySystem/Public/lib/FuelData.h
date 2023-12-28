
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NativeGameplayTags.h"
#include "GameplayTags/Public/GameplayTags.h"
#include "ItemData.h"

#include "FuelData.generated.h"


UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Fuel_Wood);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Fuel_Oil);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Item_Fuel_Flammable);


UENUM(BlueprintType)
enum class EFuelType : uint8
{
	NONE	UMETA(DisplayName = "Invalid Fuel Item"),
	WOOD	UMETA(DisplayName = "Basic Burn Fuel"),
	SOLID	UMETA(DisplayName = "Solid Based Fuel"),
	OIL		UMETA(DisplayName = "Oil-based Fuel")
};

USTRUCT()
struct T5GINVENTORYSYSTEM_API FFuelByProduct : public FStItemData
{
	GENERATED_BODY()
	
	FFuelByProduct() {};
	FFuelByProduct(FGameplayTag NewFuelTag) : FuelTag(NewFuelTag) {};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int MinimumQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int MaximumQuantity = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag FuelTag = TAG_Item_Fuel_Wood.GetTag();
};


/**
 * Items that are used as fuel
 */
UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UFuelItemAsset : public UItemDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<UItemDataAsset*, int> ByProducts = {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int BurnTimeInSeconds = 60;
	
};


USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStFuelData : public FStItemData
{
	GENERATED_BODY()

	FStFuelData() : ItemAsset(nullptr) {};

	FStFuelData(const UFuelItemAsset* NewData);

	// The source item name of the fuel item
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	const UFuelItemAsset* ItemAsset;

	// The amount of time in seconds each of this fuel provides
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int burnTime = 0;

	// The items that are created when x1 quantity of this item is burned off
	// Can be nothing/empty.
	
	TArray<FFuelByProduct> byProducts;
	
};

UCLASS()
class T5GINVENTORYSYSTEM_API UFuelSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
};