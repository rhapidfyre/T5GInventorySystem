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

	// Item Name is required. Everything else is an optional OVERRIDE.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName startingItem = FName();
	
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
