// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.h"
#include "lib/CraftData.h"
#include "lib/ItemData.h"

#include "CraftingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemCreated, int, slotNumber);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class T5GINVENTORYSYSTEM_API UCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UCraftingComponent();

	// Called after BeginPlay, when the component has been fully set up.
	// Ensure the inventory members are set before calling this function.
	void InitializeCraftingStation();

	/**
	 * Enables/Disables (toggles) the crafting system.
	 * Useful for things like crafting devices that require fuel
	 */
	UFUNCTION(BlueprintCallable)
	void SetCraftingEnabled(bool isEnabled = true);

	void SetCraftingRate(float newRate = 1.f);

	// Called when all crafting for this component should halt
	// Will resume automatically when 'ResumeCrafting' or 'RequestToCraft' succeed.
	void StopCrafting();

	// Called when crafting should be continued
	void ResumeCrafting();
	
	// Sets the input inventory. Internally calls InitializeCraftingStation.
	// Call SetOutputInventory BEFORE running this function.
	UFUNCTION(BlueprintCallable)
	void SetInputInventory(UInventoryComponent* inputInventory);

	// Run before using SetInputInventory. Sets the output inventory.
	UFUNCTION(BlueprintCallable)
	void SetOutputInventory(UInventoryComponent* outputInventory);
	
	/**
	 * Sends a request to the component to create the given item. Checks if mInventoryInput contains
	 * all required recipe items, and that this component's actor class is a valid target for the recipe.
	 * @param itemName The FName of the item from DT_ItemData that we want to craft
	 * @return True if the recipe requirements were met
	 */
	bool RequestToCraft(FName itemName);

	// Determines what category of crafting items this component can create.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ECraftingType> EligibleCraftingTypes;
	
protected:

	virtual void BeginPlay() override;

	virtual void OnComponentCreated() override;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Conducts the 'tick', updating the crafting queue
	virtual void DoCraftingTick();

	/**
	 * Server Only. Given the index of the crafting queue, consumes the ingredients required
	 * to craft the item, and finishes the process, outputting the resulting
	 * product into the Output Inventory.
	 * @param queueSlot Optional, defaults to the first item in the queue.
	 */
	virtual void CompleteCraftingItem(int queueSlot = 0);

	/**
	 * Server Only. Consumes the ingredients for the given crafting queue index.
	 * Does not conduct validation checks. Assumes truthful.
	 * @param idx The index of the mCraftingQueue to calculate.
	 * @return True if the ingredients were consumed.
	 */
	virtual bool ConsumeIngredients(int idx);

private:

	void TickCraftingItem(int idx = 0);

	UPROPERTY(Replicated) UInventoryComponent* mInventoryInput;
	UPROPERTY(Replicated) UInventoryComponent* mInventoryOutput;

	UPROPERTY() FTimerHandle mCraftingTimer;
	
	UPROPERTY() TArray<FStCraftingRecipe> mCraftingQueue;

	int mCraftingQueueSize;
	
	// The rate at which items craft. 2 means twice as fast.
	// 0.5 is half speed, zero or negative is instant-craft.
	float mCraftingRate = 1.f;

	// The rate at which recipe ingredients will be consumed.
	// 2 means it consumes twice as many resources as listed
	// Anything below 1 uses the exact ingredients required.
	float mConsumeRate = 1.f;

	bool bInstantCraft = false;
	
	bool bCraftingReady = false;

	bool bIsCraftingAllowed = true;
	
	bool bShowDebug = true;

	// Simple Mutex
	bool bCanTick = true;
	
};
