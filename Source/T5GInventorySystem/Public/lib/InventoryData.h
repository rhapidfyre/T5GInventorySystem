#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ItemData.h"

#include "InventoryData.generated.h"


USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStInventoryNotify
{
	GENERATED_BODY()

	FStInventoryNotify() {};
	FStInventoryNotify(const FStItemData& NewItemData, const int NewQuantity)
		: ItemData(NewItemData), itemQuantity(NewQuantity), wasAdded(NewQuantity > 0) {};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FStItemData ItemData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int itemQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool wasAdded = true;
};


UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API UInventoryDataAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable)
	TArray<FStItemData> GetStartingItems() const;

	// The total number of inventory slots, not accounting for backpacks or equipment slots
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int NumberOfInventorySlots = 24;

	// Inventory slots that will be used with this inventory
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer EquipmentSlots = {};

	// Optional save folder path WITHOUT the following forward slash or back slash
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SavePath = "";
	
	// If true, the owner of this inventory can pick up items on the ground by walking over it.
	// If false, the owner must interact with the actor to pick them up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPicksUpItems = false;

	// If true, the owner of this inventory will receive notifications on addition/removal of items
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShowNotifications = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FStartingItem> StartingItems;
	
};