
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
	if (!GetOwner()->HasAuthority()) return;
	
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
	if (GetOwner()->HasAuthority())
	{
		if (IsValid(inputInventory))
		{
			mInventoryInput = inputInventory;
			InitializeCraftingStation();
		}
		else
			mInventoryInput = nullptr;
	}
}

void UCraftingComponent::SetOutputInventory(UInventoryComponent* outputInventory)
{
	if (GetOwner()->HasAuthority())
	{
		if (IsValid(outputInventory))
		{
			mInventoryOutput = outputInventory;
		}
		else
			mInventoryInput = nullptr;
	}
}

bool UCraftingComponent::RequestToCraft(FName itemName)
{
	if (!bCraftingReady) return false;
	
	// Only works on authority
	if (!GetOwner()->HasAuthority()) return false;

	// Must have a valid input inventory
	if (!IsValid(mInventoryInput)) return false;

	FStCraftingRecipe craftRecipe = UCraftSystem::getRecipeFromItemName(itemName);
	if (UCraftSystem::getRecipeIsValid(craftRecipe))
		mCraftingQueue.Add(craftRecipe);

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

bool UCraftingComponent::ConsumeIngredients(int idx)
{
	if (!GetOwner()->HasAuthority()) return false;
	if (!mCraftingQueue.IsValidIndex(idx)) return false;
	
	// Does input inventory have ALL of the ingredients required?
	for (const TPair<FName, int> craftRecipe : mCraftingQueue[idx].craftingRecipe)
	{
		// If we hit an ingredient that isn't present, return and continue onto the next
		if (mInventoryInput->getTotalQuantityInAllSlots(craftRecipe.Key) < craftRecipe.Value)
		{
			return false;
		}
	}

	// If all ingredients are present, deduct them, then tick
	for (const TPair<FName, int> craftRecipe : mCraftingQueue[idx].craftingRecipe)
	{
		mInventoryInput->removeItemByQuantity(craftRecipe.Key, craftRecipe.Value);
	}
	return true;
}

void UCraftingComponent::TickCraftingItem(int idx)
{
	if (!bCraftingReady) return;
	if (mCraftingQueue.IsValidIndex(idx))
	{
		// Check for ingredient consumption
		if (mCraftingQueue[idx].ticksCompleted % mCraftingQueue[idx].tickConsume == 0)
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
			if (mCraftingQueue[i].ticksCompleted >= mCraftingQueue[i].ticksToComplete)
			{
				CompleteCraftingItem(i);
			}
			else
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
			if (mCraftingQueue[i].ticksCompleted >= mCraftingQueue[i].ticksToComplete)
			{
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
		
	// DO NOT remove from the queue once complete.
	// Mark as complete, and wait for DoTick to handle.
	const int itemsAdded = mInventoryOutput->addItemByName(
		mCraftingQueue[queueSlot].itemName,
		mCraftingQueue[queueSlot].createsQuantity,
		true, false, true);

	// If we couldn't add the items, then the inventory is full
	if (itemsAdded > 0) mCraftingQueue.RemoveAt(queueSlot);
	
}


/**
 *		REPLICATION
 */

void UCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UCraftingComponent, mInventoryInput, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UCraftingComponent, mInventoryOutput, COND_OwnerOnly);
}
