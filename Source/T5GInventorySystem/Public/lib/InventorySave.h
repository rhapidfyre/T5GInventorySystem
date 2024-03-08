
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

	/*
	UFUNCTION(BlueprintPure)
	TArray<FStInventorySlot> LoadInventorySlots();

	UFUNCTION(BlueprintCallable)
	void SaveInventorySlots(TArray<FStInventorySlot> ArrayOfCopiedSlots);


private:
*/
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite)
	TArray<FStInventorySlot> SavedInventorySlots = {};
	
};