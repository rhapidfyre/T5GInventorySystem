
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "FuelData.generated.h"

UENUM(BlueprintType)
enum class EFuelType : uint8
{
	NONE	UMETA(DisplayName = "Invalid Fuel Item"),
	WOOD	UMETA(DisplayName = "Basic Burn Fuel"),
	SOLID	UMETA(DisplayName = "Solid Based Fuel"),
	OIL		UMETA(DisplayName = "Oil-based Fuel")
};

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStFuelData : public FTableRowBase
{
	GENERATED_BODY()

	// The source item name of the fuel item
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName properName = FName();

	// The amount of time in seconds each of this fuel provides
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int burnTime = 0;

	// The items that are created when x1 quantity of this item is burned off
	// Can be nothing/empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<FName, int> byProducts;
	
};

UCLASS()
class T5GINVENTORYSYSTEM_API UFuelSystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
	// Returns a pointer to the item data table for manual operations
	UFUNCTION(BlueprintPure, Category = "Fuel Data System Globals")
	static UDataTable* getFuelItemDataTable();

	// Gets the item data structure from the given item name ('food_carrot')
	// Performs an O(1) lookup, but retrieves data, which is slower than direct access.
	UFUNCTION(BlueprintPure, Category = "Fuel Data System Globals")
	static FStFuelData getFuelItemFromName(FName itemName);
	
	UFUNCTION(BlueprintPure, Category = "Fuel Data System Globals")
	static bool getFuelNameIsValid(FName itemName, bool performLookup = false);
	
	UFUNCTION(BlueprintPure, Category = "Fuel Data System Globals")
	static bool getFuelItemIsValid(FStFuelData itemData);
	
	UFUNCTION(BlueprintPure, Category = "Fuel Data System Globals")
	static FTimespan getFuelBurnTimeByName(FName itemName);
	
	UFUNCTION(BlueprintPure, Category = "Fuel Data System Globals")
	static FTimespan getFuelBurnTime(FStFuelData itemData);
	
};