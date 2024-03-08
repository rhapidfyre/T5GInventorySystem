#pragma once

#include "CoreMinimal.h"
#include "lib/InventorySlot.h"

#include "InventorySystemGlobals.generated.h"


UCLASS(Blueprintable)
class T5GINVENTORYSYSTEM_API UInventorySystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintPure)
	bool GetIsSameItem(const FStItemData& ItemOne, const FStItemData& ItemTwo) const;
	
	UFUNCTION(BlueprintPure)
	bool GetIsSameExactItem(const FStItemData& ItemOne, const FStItemData& ItemTwo) const;

	
	UFUNCTION(BlueprintPure)
	bool GetIsSameSlot(const FStInventorySlot& SlotOne, const FStInventorySlot& SlotTwo) const;
	
	UFUNCTION(BlueprintPure)
	bool GetIsSameExactSlot(const FStInventorySlot& SlotOne, const FStInventorySlot& SlotTwo) const;
	
};