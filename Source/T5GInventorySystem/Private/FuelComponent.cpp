// Fill out your copyright notice in the Description page of Project Settings.


#include "FuelComponent.h"

#include "NavigationSystemTypes.h"
#include "PickupActorBase.h"
#include "lib/FuelData.h"
#include "Net/UnrealNetwork.h"


void UFuelComponent::SetupDefaults()
{
#ifdef UE_BUILD_DEBUG
	bShowDebug = true;
#endif
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	SetAutoActivate(true);
}

void UFuelComponent::OnRep_CurrentFuelItem_Implementation()
{
	OnFuelUpdated.Broadcast();
}

UFuelComponent::UFuelComponent()
{
	SetupDefaults();
}

UFuelComponent::UFuelComponent(const UFuelItemAsset* FuelAsset)
{
	if (IsValid(FuelAsset))
	{
		const FStFuelData fuelData = FStFuelData(FuelAsset);
		const FTimespan fuelTime = FTimespan(fuelData.burnTime);
		mCurrentFuelItem	= FStFuelData(FuelAsset);
		mTimeRemaining		= fuelTime.GetTotalSeconds();
		OnFuelUpdated.Broadcast();
	}
	SetupDefaults();
}

void UFuelComponent::InitializeFuelSystem()
{
	if (GetOwner()->HasAuthority())
	{
		if (!bIsFuelSystemReady)
		{
			// Determine what fuel items are allowed for this entity
			for (UFuelItemAsset* fuelItem : FuelItemsAllowed)
			{
				if (!mAuthorizedFuel.Contains(fuelItem))
				{
					mAuthorizedFuel.Add(fuelItem);
				}
			}
			//FuelItemsAllowed.Empty(0);
	
			// Get current/starting fuel item
			if ( IsValid(mCurrentFuelItem.ItemAsset) )
			{
				const FStFuelData fuelData = mCurrentFuelItem;
				const FTimespan fuelTime = FTimespan(fuelData.burnTime);
				mTimeRemaining = fuelTime.GetTotalSeconds();
			}
		
		}
		
		bIsFuelSystemReady = true;

		// Autostart the system if it has fuel
		StartFuelSystem();
		
	}//server
	else
	{
		bIsFuelSystemReady = true;
	}
}

void UFuelComponent::SetFuelConsumeRate(float inRate)
{
	if (GetOwner()->HasAuthority())
	{
		mTickRate = inRate > 0.f ? inRate : 0.f;
		if (inRate <= 0.f)
		{
			mTimerTick = 1.f;
			bIgnoreFuel = true;
		}
		else
		{
			mTimerTick = 1/inRate;
			bIgnoreFuel = false;
		}
		
		if (GetWorld()->GetTimerManager().TimerExists(mFuelTimer))
		{
			const bool wasPaused = GetWorld()->GetTimerManager().IsTimerPaused(mFuelTimer);
			GetWorld()->GetTimerManager().PauseTimer(mFuelTimer);
			GetWorld()->GetTimerManager().SetTimer(mFuelTimer,this,
					&UFuelComponent::CheckForConsumption,mTimerTick,true);
			if (!wasPaused)
				GetWorld()->GetTimerManager().UnPauseTimer(mFuelTimer);
		}
	}
}

void UFuelComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
	RegisterComponent();
}

bool UFuelComponent::StartFuelSystem()
{
	if (!bIsFuelSystemReady) return false;
	if (GetOwner()->HasAuthority())
	{
		if (IsFuelAvailable())
		{
			if (!GetWorld()->GetTimerManager().TimerExists(mFuelTimer))
			{
				GetWorld()->GetTimerManager().SetTimer(mFuelTimer,this,
						&UFuelComponent::CheckForConsumption,mTimerTick,true);
			}
			GetWorld()->GetTimerManager().UnPauseTimer(mFuelTimer);
			return true;
		}
	}
	return false;
}

bool UFuelComponent::StopFuelSystem()
{
	if (GetOwner()->HasAuthority())
	{
		if (GetWorld()->GetTimerManager().TimerExists(mFuelTimer))
			GetWorld()->GetTimerManager().PauseTimer(mFuelTimer);
		OnFuelSystemToggled.Broadcast(false);
		bIsRunning = false;
		return true;
	}
	return false;
}

bool UFuelComponent::IsReserveFuelAvailable()
{
	if (IsValid(mInventoryFuel))
	{
		for (const UFuelItemAsset* fuelItem : mAuthorizedFuel)
		{
			if (mInventoryFuel->GetTotalQuantityByItem(fuelItem) > 0)
			{
				return true;
			}
		}
	}
	return false;
}

bool UFuelComponent::RemoveFuel()
{
	for (const UFuelItemAsset* fuelItem : mAuthorizedFuel)
	{
		const int itemsRemoved = mInventoryFuel->RemoveItemByQuantity(fuelItem, 1);
		if (itemsRemoved > 0)
		{
			mCurrentFuelItem = fuelItem;
			if (IsValid(mCurrentFuelItem.ItemAsset))
			{
				const FStFuelData fuelData = mCurrentFuelItem;
				const FTimespan fuelTime = FTimespan(fuelData.burnTime);
				mTimeRemaining = fuelTime.GetTotalSeconds();
				return true;
			}
			OnFuelUpdated.Broadcast();
		}
	}
	return false;
}

int UFuelComponent::GetTotalFuelItemsAvailable()
{
	int itemsAvailable = 0;
	if (IsValid(mInventoryFuel))
	{
		for (const FStFuelData fuelItem : mAuthorizedFuel)
		{
			itemsAvailable += mInventoryFuel->GetTotalQuantityByItem(fuelItem);
		}
	}
	return itemsAvailable;
}

FTimespan UFuelComponent::GetTotalFuelTimeAvailable()
{
	FTimespan timespan(0,0,0);
	if (IsValid(mInventoryFuel))
	{
		for (const FStFuelData fuelItem : mAuthorizedFuel)
		{
			const int ttlInSlot = mInventoryFuel->GetTotalQuantityByItem(fuelItem);
			FTimespan burnTime = FTimespan(fuelItem.burnTime);
			timespan += FTimespan::FromSeconds(burnTime.GetTotalSeconds() * ttlInSlot);
		}
	}
	timespan.FromSeconds(mTimeRemaining);
	return timespan;
}

bool UFuelComponent::IsFuelAvailable()
{
	return IsReserveFuelAvailable() || mTimeRemaining > 0.f;
}

void UFuelComponent::SetFuelInventory(UInventoryComponent* fuelInv)
{
	if (IsValid(fuelInv)) mInventoryFuel = fuelInv;
	else mInventoryFuel = nullptr;
}

void UFuelComponent::SetOutputInventory(UInventoryComponent* staticInv)
{
	if (IsValid(staticInv)) mInventoryStatic = staticInv;
	else mInventoryStatic = nullptr;
}


// Called when the game starts
void UFuelComponent::BeginPlay()
{
	Super::BeginPlay();

	// Attempts to automatically detect the fuel inventory
	if (!IsValid(getFuelInventory()))
	{
		const AActor* ownerActor = GetOwner();
		if (IsValid(ownerActor))
		{
			TArray<UActorComponent*> invComps = ownerActor->GetComponentsByTag(
				UInventoryComponent::StaticClass(), FName("fuel"));
			if (invComps.IsValidIndex(0))
			{
				SetFuelInventory(Cast<UInventoryComponent>(invComps[0]));
			}
		}
	}

	// Attempts to automatically detect the output inventory
	if (!IsValid(getOutputInventory()))
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

void UFuelComponent::CheckForConsumption()
{
	if (GetOwner()->HasAuthority())
	{
		bool runSystem = true;
		if (!bIgnoreFuel)
		{
		
			// If the system was running already, and the time expired, create byproduct
			bool isOverflowing = false;
			if (bIsRunning)
			{
				mTimeRemaining -= 1.f;
				if (mTimeRemaining <= 0.f)
					CreateByProduct(isOverflowing);
			}
			
			if (mTimeRemaining <= 0.f)
			{
				mTimeRemaining = 0.f;
				mCurrentFuelItem = FStFuelData();
				runSystem = !isOverflowing && IsReserveFuelAvailable();
				if (runSystem)
					runSystem = ConsumeQueuedItem();
			}
		}
		
		if (bIsRunning && !runSystem)
		{
			StopFuelSystem();
		}
		else if (!bIsRunning && runSystem)
		{
			bIsRunning = true;
			OnFuelSystemToggled.Broadcast(true);
		}
		
	}
}

bool UFuelComponent::ConsumeQueuedItem()
{
	if (GetOwner()->HasAuthority())
	{
		if (IsValid(mInventoryFuel))
		{
			return RemoveFuel();
		}
	}
	return false;
}

void UFuelComponent::CreateByProduct(bool &isOverflowing)
{
	const FStFuelData fuelData = mCurrentFuelItem;
	if (IsValid(fuelData.ItemAsset))
	{
		for (const FFuelByProduct& byProduct : fuelData.byProducts)
		{
			if (IsValid(mInventoryStatic))
			{
				const int byMin = byProduct.MinimumQuantity > 0 ? byProduct.MinimumQuantity : 1;
				const int byMax = byProduct.MaximumQuantity < byProduct.MinimumQuantity ? byProduct.MinimumQuantity : byProduct.MaximumQuantity;
				const int randQuantity = FMath::RandRange(byMin, byMax);
				mInventoryStatic->AddItemFromDataAsset(byProduct.Data, randQuantity,-1,true,true,true);
			}
			else
			{
				FTransform spawnTransform(
					FRotator(FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f)),
					GetOwner()->GetActorLocation());
				spawnTransform.SetLocation(spawnTransform.GetLocation() + FVector(0,0,128));
				
				FActorSpawnParameters spawnParams;
				spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        	
				APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
														APickupActorBase::StaticClass(), spawnTransform);
				if (IsValid(pickupItem))
				{
					const int byMin = byProduct.MinimumQuantity > 0 ? byProduct.MinimumQuantity : 1;
					const int byMax = byProduct.MaximumQuantity < byProduct.MinimumQuantity ? byProduct.MinimumQuantity : byProduct.MaximumQuantity;
					const int randQuantity = FMath::RandRange(byMin, byMax);
					FStItemData TempItem(byProduct, randQuantity);
					pickupItem->SetupItem(TempItem);
					pickupItem->FinishSpawning(spawnTransform);
				}
			}
		}
	}
	isOverflowing = false;
}


void UFuelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UFuelComponent, mCurrentFuelItem);
	DOREPLIFETIME(UFuelComponent, mTimeRemaining);
	DOREPLIFETIME(UFuelComponent, mTickRate);
	DOREPLIFETIME(UFuelComponent, mInventoryStatic);
	DOREPLIFETIME(UFuelComponent, mInventoryFuel);
	DOREPLIFETIME(UFuelComponent, bIsRunning);
}
