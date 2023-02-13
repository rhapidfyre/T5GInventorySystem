#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "EquipmentData.h"

#include "EquipmentSlot.generated.h"

USTRUCT(BlueprintType)
struct FStEquipmentSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	/* The ID of the 'm_currentItems' index where the item can be found in this inventory
	 * (-1) indicates the slot is empty or there is no item in that slot.
	 */
	int itemId = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int slotQuantity = 0;

	UPROPERTY()
	int slotNumber = -1;

	UPROPERTY()
	FStItemData itemData;

	// If true, players can only take from this slot, not put into it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool withdrawOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSingleItemOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	// The equipment relationship to this inventory slot
	EEquipmentSlotType slotType = EEquipmentSlotType::NONE;
    
};