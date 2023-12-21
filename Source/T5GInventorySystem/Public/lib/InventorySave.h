
#pragma once


#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"

#include "InventorySave.generated.h"

struct FStInventorySlot;

UCLASS(Blueprintable, BlueprintType)
class T5GINVENTORYSYSTEM_API UInventorySave : public USaveGame
{
	GENERATED_BODY()
	
public:
	
	UInventorySave() {};
	UInventorySave(FString NewSaveName) { InventorySaveSlotName_ = NewSaveName; }

	void SetSaveName(const FString& SaveSlotName)
	{
		if (InventorySaveSlotName_.IsEmpty())
		{
			InventorySaveSlotName_ = SaveSlotName;
		}
	}
	FString GetSaveSlotName() const { return InventorySaveSlotName_; }

	UPROPERTY(SaveGame) TArray<FStInventorySlot> InventorySlots_ = {};
	UPROPERTY(SaveGame) TArray<FStInventorySlot> EquipmentSlots_ = {};

private:
	UPROPERTY(SaveGame) FString InventorySaveSlotName_ = "";
	
};