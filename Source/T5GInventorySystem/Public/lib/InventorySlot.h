#pragma once

#include "CoreMinimal.h"
#include "InventoryData.h"
#include "EquipmentData.h"

#include "InventorySlot.generated.h"

// fwd declare
struct FStItemData;


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

	FStInventorySlot() {};
	
	FStInventorySlot(FName InitItemName, int InitQuantity = 1);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemName = FName();
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int SlotQuantity = 0;
	
	// The quantity of the item in this slot. Zero means empty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SlotDurability = 0.f;

	// The type of this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EInventorySlotType SlotType = EInventorySlotType::NONE;

	// The equipment relationship to this inventory slot
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEquipmentSlotType EquipType = EEquipmentSlotType::NONE;

	FStItemData GetItemData() const;

	bool IsSlotEmpty() const;

	void EmptyAndResetSlot();
    
};
