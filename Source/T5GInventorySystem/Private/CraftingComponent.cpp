
#include "CraftingComponent.h"
#include "Net/UnrealNetwork.h"

UCraftingComponent::UCraftingComponent()
{
#ifdef UE_BUILD_DEBUG
	bShowDebug = true;
#endif
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	bAutoActivate = true;
}

void UCraftingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Turn setting into private variable
	mCraftingQueueSize = MaxItemsInQueue;
	
	// Attempts to automatically detect the input inventory
	if (!IsValid(GetInputInventory()))
	{
		const AActor* ownerActor = GetOwner();
		if (IsValid(ownerActor))
		{
			TArray<UActorComponent*> invComps = ownerActor->GetComponentsByTag(
				UInventoryComponent::StaticClass(), FName("craft"));
			if (invComps.IsValidIndex(0))
			{
				SetInputInventory(Cast<UInventoryComponent>(invComps[0]));
			}
		}
	}
	else
		bCraftingReady = true;

	// Attempts to automatically detect the output inventory
	if (!IsValid(GetOutputInventory()))
	{
		const AActor* ownerActor = GetOwner();
		if (IsValid(ownerActor))
		{
			TArray<UActorComponent*> invComps = ownerActor->GetComponentsByTag(
				UInventoryComponent::StaticClass(), FName("output"));
			if (invComps.IsValidIndex(0))
			{
				SetOutputInventory(Cast<UInventoryComponent>(invComps[0]));
			}
		}
	}
	
}

void UCraftingComponent::InitializeCraftingStation()
{
	// If there is no input inventory, the crafting component is invalid.
	if (!IsValid(mInventoryInput))
	{
		UE_LOG(LogTemp, Error, TEXT("%s(%s): No input inventory set. Crafting Component will not work properly!"),
			*GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"));
		return;
	}
	
	// If there is no output inventory, use the input inventory as dual purpose
	if (!IsValid(mInventoryOutput))
		mInventoryOutput = mInventoryInput;

	bCraftingReady = true;
	
}

void UCraftingComponent::SetCraftingEnabled(bool isEnabled)
{
	if (GetOwner()->HasAuthority())
	{
		if (bCraftingReady)
		{
			bIsCraftingAllowed = isEnabled;
			if (bIsCraftingAllowed)
			{
				// Restart the queue
				if (!mCraftingQueue.IsEmpty() && mCraftingTimer.IsValid())
					GetWorld()->GetTimerManager().UnPauseTimer(mCraftingTimer);
			}
			else
			{
				// Stop the queue
				if (mCraftingTimer.IsValid())
					GetWorld()->GetTimerManager().PauseTimer(mCraftingTimer);
			}
		}
	}
}

void UCraftingComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
	mCraftingQueueSize = MaxItemsInQueue;
	RegisterComponent();
}

void UCraftingComponent::SetCraftingRate(float newRate)
{
	mCraftingRate = newRate;
	bInstantCraft = false;
	if (newRate <= 0.f)
	{
		mCraftingRate = 0.f;
		bInstantCraft = true;
	}
}

void UCraftingComponent::StopCrafting()
{
	if (!bCraftingReady) return;
	if (mCraftingTimer.IsValid())
	{
		if (!GetWorld()->GetTimerManager().IsTimerPaused(mCraftingTimer))
			GetWorld()->GetTimerManager().PauseTimer(mCraftingTimer);
	}
}

void UCraftingComponent::ResumeCrafting()
{
	if (!bCraftingReady) return;
	if (!mCraftingTimer.IsValid())
	{
		GetWorld()->GetTimerManager().SetTimer(mCraftingTimer, this,
			&UCraftingComponent::DoCraftingTick, mCraftingRate, true, mCraftingRate);
	}
	else
	{
		if (GetWorld()->GetTimerManager().IsTimerPaused(mCraftingTimer))
			GetWorld()->GetTimerManager().UnPauseTimer(mCraftingTimer);
	}
}

void UCraftingComponent::SetInputInventory(UInventoryComponent* inputInventory)
{
	if (IsValid(inputInventory))
	{
		mInventoryInput = inputInventory;
		InitializeCraftingStation();
	}
	else
		mInventoryInput = nullptr;
}

void UCraftingComponent::SetOutputInventory(UInventoryComponent* outputInventory)
{
	if (IsValid(outputInventory))
	{
		mInventoryOutput = outputInventory;
	}
	else
		mInventoryInput = nullptr;
}

bool UCraftingComponent::RequestToCraft(FName itemName)
{
	if (!bCraftingReady) return false;
	
	// Only works on authority
	if (!GetOwner()->HasAuthority()) return false;

	// Must have a valid input inventory
	if (!IsValid(mInventoryInput)) return false;

	// Queue is Full
	if (mCraftingQueue.Num() >= mCraftingQueueSize) return false;
	
	const FStCraftingRecipe craftRecipe = UCraftSystem::getRecipeFromItemName(itemName);
	const FStItemData itemData = UItemSystem::getItemDataFromItemName(itemName);
	
	FStCraftQueueData craftQueueData(craftRecipe.itemName, itemData.itemThumbnail);
	
	if (UCraftSystem::getRecipeIsValid(craftRecipe))
		mCraftingQueue.Add(craftQueueData);

	if (!mCraftingQueue.IsEmpty())
	{
		if (bInstantCraft)
			DoCraftingTick();
		else
		{
			ResumeCrafting();
		}
		return true;
	}
	return false;
}

FStCraftQueueData UCraftingComponent::GetItemInCraftingQueue(int slotNumber)
{
	if (mCraftingQueue.IsValidIndex(slotNumber))
	{
		return mCraftingQueue[slotNumber];
	}
	return FStCraftQueueData();
}

bool UCraftingComponent::CancelCrafting(int queueIndex)
{
	if (!bCraftingReady) return false;
	
	// Only works on authority
	if (!GetOwner()->HasAuthority()) return false;

	// Must have a valid input inventory
	if (!IsValid(mInventoryInput)) return false;
	
	if (mCraftingQueue.IsValidIndex(queueIndex))
	{
		const FStCraftQueueData craftQueue = mCraftingQueue[queueIndex];
		const FStCraftingRecipe craftRecipe = UCraftSystem::getRecipeFromItemName(craftQueue.itemName);
		mCraftingQueue.RemoveAt(queueIndex);
		const int ingredientMultiplier = FMath::Floor(craftQueue.ticksCompleted / craftRecipe.tickConsume);
		if (ingredientMultiplier > 0)
		{
			for (const TPair<FName, int> thisRecipe : craftRecipe.craftingRecipe)
			mInventoryInput->AddItemFromDataTable(thisRecipe.Key, thisRecipe.Value * ingredientMultiplier,
				-1, true, false, false);
		}
		return true;
	}
	return false;
}

bool UCraftingComponent::ConsumeIngredients(int idx)
{
	if (!GetOwner()->HasAuthority()) return false;
	if (!mCraftingQueue.IsValidIndex(idx)) return false;
	
	// Does input inventory have ALL of the ingredients required?
	const FStCraftQueueData qData = mCraftingQueue[idx];
	const FStCraftingRecipe recipeData = UCraftSystem::getRecipeFromItemName( qData.itemName );

	for (const TPair<FName, int> craftRecipe : recipeData.craftingRecipe)
	{
		// If we hit an ingredient that isn't present, return and continue onto the next
		if (mInventoryInput->GetQuantityOfItem(craftRecipe.Key) < craftRecipe.Value)
		{
			return false;
		}
	}

	// If all ingredients are present, deduct them, then tick
	for (const TPair<FName, int> craftRecipe : recipeData.craftingRecipe)
	{
		UE_LOG(LogTemp, Display, TEXT("%s(%s): Consuming x%d of '%s' for crafting."),
			*GetName(), GetOwner()->HasAuthority()?TEXT("SERVER"):TEXT("CLIENT"),
			craftRecipe.Value, *craftRecipe.Key.ToString());
		mInventoryInput->RemoveItemByQuantity(craftRecipe.Key, craftRecipe.Value);
	}
	return true;
}

void UCraftingComponent::TickCraftingItem(int idx)
{
	if (!bCraftingReady) return;
	if (mCraftingQueue.IsValidIndex(idx))
	{
		// Check for ingredient consumption
		const FStCraftingRecipe craftingRecipe = UCraftSystem::getRecipeFromItemName( mCraftingQueue[idx].itemName );

		if (mCraftingQueue[idx].ticksCompleted % craftingRecipe.tickConsume == 0)
		{
			if (IsValid(mInventoryInput))
			{
				if (ConsumeIngredients(idx))
					mCraftingQueue[idx].ticksCompleted += 1;
			}
		}
		else
			mCraftingQueue[idx].ticksCompleted += 1;
	}
}

void UCraftingComponent::OnRep_CraftingQueue_Implementation()
{
	OnQueueUpdated.Broadcast(-1);
}

void UCraftingComponent::DoCraftingTick()
{
	if (!bCraftingReady) return;
	// Authority Only
	if (!GetOwner()->HasAuthority()) return;
	
	// If instant crafting is on, complete all items that are in the queue.
	if (bInstantCraft)
	{
		for (int i = 0; i < mCraftingQueue.Num(); i++)
		{
			CompleteCraftingItem();
		}
		mCraftingQueue.Empty();
		return;
	}
	
	if(!bCanTick) return;
	bCanTick = false; // Mutex Unlock

	// Tick the crafting queued items, and complete them if they're finished.
	// Does not remove them from the queue. That comes next.
	for (int i = 0; i < mCraftingQueue.Num(); i++)
	{
		if (mCraftingQueue.IsValidIndex(i))
		{
			const FStCraftingRecipe craftingRecipe = UCraftSystem::getRecipeFromItemName( mCraftingQueue[i].itemName );

			if (mCraftingQueue[i].ticksCompleted < craftingRecipe.ticksToComplete)
			{
				TickCraftingItem(i);
			}
		}
	}

	// Remove
	for (int i = mCraftingQueue.Num() - 1; i >= 0; i--)
	{
		if (mCraftingQueue.IsValidIndex(i))
		{
			const FStCraftingRecipe craftingRecipe = UCraftSystem::getRecipeFromItemName( mCraftingQueue[i].itemName );

			if (mCraftingQueue[i].ticksCompleted >= craftingRecipe.ticksToComplete)
			{
				CompleteCraftingItem(i);
				mCraftingQueue.RemoveAt(i);
			}
		}
	}

	// If it's empty, pause the timer. Save memory.
	if (mCraftingQueue.IsEmpty())
	{
		if (mCraftingTimer.IsValid())
		{
			StopCrafting();
		}
	}
	
	bCanTick = true; // Mutex Unlock
	
}

void UCraftingComponent::CompleteCraftingItem(int queueSlot)
{
	if (!bCraftingReady) return;
	
	// Authority Only
	if (!GetOwner()->HasAuthority()) return;

	// Validate the queue index
	if (!mCraftingQueue.IsValidIndex(queueSlot)) return;

	// Consume the ingredients
	if (!ConsumeIngredients(queueSlot)) return;
		
	const FStCraftingRecipe craftingRecipe = UCraftSystem::getRecipeFromItemName( mCraftingQueue[queueSlot].itemName );

	// DO NOT remove from the queue once complete.
	// Mark as complete, and wait for DoTick to handle.
	const int itemsAdded = mInventoryOutput->AddItemFromDataTable(
		mCraftingQueue[queueSlot].itemName, craftingRecipe.createsQuantity, -1,
		true, true, true);
	
}


/**
 *		REPLICATION
 */

void UCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCraftingComponent, mInventoryInput, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCraftingComponent, mInventoryOutput, COND_OwnerOnly);
	DOREPLIFETIME(UCraftingComponent, mCraftingQueue);
}
