#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "InventoryData.h"
#include "EquipmentData.h"

#include "InventorySlot.generated.h"

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStInventorySlot
{
	GENERATED_BODY()
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int slotQuantity = 0;

	// The slot number of this slot. Negative 1 means invalid slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int slotNumber = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FStItemData itemData;

	// The type of this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInventorySlotType slotType = EInventorySlotType::NONE;

	// The equipment relationship to this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipmentSlotType equipType = EEquipmentSlotType::NONE;
    
};
