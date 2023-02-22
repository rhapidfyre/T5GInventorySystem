#pragma once

#include "CoreMinimal.h"
#include "ItemData.h"
#include "InventoryData.h"
#include "EquipmentData.h"
#include "EnhancedInput/Public/InputAction.h"

#include "InventorySlot.generated.h"

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStStartingItem
{
	GENERATED_BODY()
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int quantity = 0;
	
	// The slot the item should spawn into. Negative means first available slot.
	// Ignored when the item is going into an equipment slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int startingSlot = -1;

	// Item Name is required. Everything else is an optional OVERRIDE.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FStItemData itemData;

	// The type of slot this item should be sent to
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInventorySlotType slotType = EInventorySlotType::NONE;

	// The equipment slot the item should start in
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipmentSlotType equipType = EEquipmentSlotType::NONE;
	
};

USTRUCT(BlueprintType)
struct T5GINVENTORYSYSTEM_API FStInventorySlot
{
	GENERATED_BODY()
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int slotQuantity = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UInputAction* inputAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FStItemData itemData;

	// The type of this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInventorySlotType slotType = EInventorySlotType::NONE;

	// The equipment relationship to this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipmentSlotType equipType = EEquipmentSlotType::NONE;
    
};
