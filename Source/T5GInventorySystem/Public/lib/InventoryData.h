#pragma once

#include "CoreMinimal.h"

#include "InventoryData.generated.h"

/**
 * WARNING - If you CHANGE, ADD or REMOVE any of these enums,
 * you will need to go to       Content/AZ_Assets/DataTables
 * and update the               DT_SlotIcons
 * if you want icons to work
 */
UENUM(BlueprintType)
enum class EInventorySlotType : uint8
{
	NONE     UMETA(DisplayName = "Invalid Slot"),
	
	GENERAL  UMETA(DisplayName = "Generic Slot"),
	
	// This slot is a normal inventory slot for the player who owns it, and disabled for those who do not.
	HIDDEN   UMETA(DisplayName = "Hidden Slot"),
	
	EQUIP    UMETA(DisplayName = "Equipment Slot"),

	// This slot is normal equipment for the owning player, and disabled to anyone else
	LOCKED   UMETA(DisplayName = "Hidden Equipment Slot"),
	
	// This slot points to an existing inventory slot, aka "mirrors" it.
	MIRROR  UMETA(DisplayName = "Mirrored Slot")
};

USTRUCT(BlueprintType)
struct FStInventoryNotify
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName itemName = "None";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float itemQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool wasAdded = true;
};