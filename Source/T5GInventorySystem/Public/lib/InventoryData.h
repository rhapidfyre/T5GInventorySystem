#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "InventoryData.generated.h"

struct FStStartingItem;
/**
 * WARNING - If you CHANGE, ADD or REMOVE any of these enums,
 * you will need to go to       Content/AZ_Assets/DataTables
 * and update the               DT_SlotIcons
 * if you want icons to work
 */
UENUM(BlueprintType)
enum class EInventorySlotType : uint8
{
	NONE		UMETA(DisplayName = "Invalid Slot"),
	
	GENERAL		UMETA(DisplayName = "Generic Slot"),
	
	// This slot is a normal inventory slot for the player who owns it, and disabled for those who do not.
	HIDDEN		UMETA(DisplayName = "Hidden Slot"),
	
	EQUIP		UMETA(DisplayName = "Equipment Slot"),

	// This slot is normal equipment for the owning player, and disabled to anyone else
	LOCKED		UMETA(DisplayName = "Hidden Equipment Slot"),
	
	// This slot points to an existing inventory slot, aka "mirrors" it.
	MIRROR		UMETA(DisplayName = "Mirrored Slot"),

	MAX			UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStInventoryNotify
{
	GENERATED_BODY()

	FStInventoryNotify() {};
	FStInventoryNotify(FName iName, int iQuantity)
		{ itemName = iName; itemQuantity = iQuantity; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName itemName = FName();

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
	TArray<class UStartingItemData*> GetStartingItems() const;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<class UStartingItemData*> StartingItems;
	
};