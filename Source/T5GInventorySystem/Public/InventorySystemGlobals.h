#pragma once

#include "CoreMinimal.h"
#include "lib/InventorySlot.h"

#include "InventorySystemGlobals.generated.h"

UCLASS(Blueprintable)
class T5GINVENTORYSYSTEM_API UInventorySystem : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	static bool IsSameItem(const FStInventorySlot& SlotOne, const FStInventorySlot& SlotTwo);
	static bool IsExactSameItem(const FStInventorySlot& SlotOne, const FStInventorySlot& SlotTwo);
};