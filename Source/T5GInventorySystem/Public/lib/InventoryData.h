#pragma once

#include "CoreMinimal.h"

#include "InventoryData.generated.h"

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStStartingItem : public FTableRowBase
{
	GENERATED_BODY()

	FStStartingItem() {};
	FStStartingItem(FName NewName, int NewQuantity, bool startEquipped = false);
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int quantity = 0;

	// Item Name is required. Everything else is an optional OVERRIDE.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName startingItem = FName();
	
	// If true, the item will be equipped
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStartEquipped = false;
	
};

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