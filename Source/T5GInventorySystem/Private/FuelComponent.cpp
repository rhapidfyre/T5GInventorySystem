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

UFuelComponent::UFuelComponent()
{
	SetupDefaults();
}

UFuelComponent::UFuelComponent(FName startingItem)
{
	if (UItemSystem::getItemNameIsValid(startingItem, true))
	{
		const FStFuelData fuelData = UFuelSystem::getFuelItemFromName(startingItem);
		if (UFuelSystem::getFuelItemIsValid(fuelData))
		{
			const FTimespan fuelTime = UFuelSystem::getFuelBurnTime(fuelData);
			mCurrentFuelItem	= startingItem;
			mTimeRemaining		= fuelTime.GetTotalSeconds();
		}
	}
	SetupDefaults();
}

void UFuelComponent::InitializeFuelSystem()
{
	if (UItemSystem::getItemNameIsValid(mCurrentFuelItem))
	{
		const FStFuelData fuelData = UFuelSystem::getFuelItemFromName(mCurrentFuelItem);
		const FTimespan fuelTime = UFuelSystem::getFuelBurnTime(fuelData);
		mTimeRemaining = fuelTime.GetTotalSeconds();
		GetWorld()->GetTimerManager().ClearTimer(mFuelTimer);
		GetWorld()->GetTimerManager().SetTimer(mFuelTimer,this,
				&UFuelComponent::CheckForConsumption,mTickRate,true);
		GetWorld()->GetTimerManager().PauseTimer(mFuelTimer);
	}
}

void UFuelComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
	RegisterComponent();
	InitializeFuelSystem();
	if (bVerboseOutput) bShowDebug = true;
	bIsFuelSystemReady = true;
}

bool UFuelComponent::StartFuelSystem()
{
	if (GetOwner()->HasAuthority())
	{
		if (IsFuelAvailable())
		{
			if (GetWorld()->GetTimerManager().TimerExists(mFuelTimer))
			{
				GetWorld()->GetTimerManager().UnPauseTimer(mFuelTimer);
				OnFuelSystemToggled.Broadcast(true);
				return true;
			}
		}
	}
	return false;
}

bool UFuelComponent::StopFuelSystem()
{
	if (GetOwner()->HasAuthority())
	{
		if (GetWorld()->GetTimerManager().TimerExists(mFuelTimer))
		{
			GetWorld()->GetTimerManager().PauseTimer(mFuelTimer);
			OnFuelSystemToggled.Broadcast(false);
			return true;
		}
	}
	return false;
}

bool UFuelComponent::IsReserveFuelAvailable()
{
	if (IsValid(mInventoryFuel))
	{
		for (const FName fuelItem : FuelItemsAllowed)
		{
			if (mInventoryFuel->getSlotsContainingItem(fuelItem).Num() > 0)
			{
				return true;
			}
		}
	}
	return false;
}

bool UFuelComponent::RemoveFuel()
{
	for (const FName fuelItem : FuelItemsAllowed)
	{
		const int itemsRemoved = mInventoryFuel->removeItemByQuantity(fuelItem, 1);
		if (itemsRemoved > 0)
		{
			mCurrentFuelItem = fuelItem;
			if (UItemSystem::getItemNameIsValid(mCurrentFuelItem))
			{
				const FStFuelData fuelData = UFuelSystem::getFuelItemFromName(mCurrentFuelItem);
				const FTimespan fuelTime = UFuelSystem::getFuelBurnTime(fuelData);
				mTimeRemaining = fuelTime.GetTotalSeconds();
				return true;
			}
		}
	}
	return false;
}

int UFuelComponent::GetTotalFuelItemsAvailable()
{
	int itemsAvailable = 0;
	if (IsValid(mInventoryFuel))
	{
		for (const FName fuelItem : FuelItemsAllowed)
		{
			itemsAvailable += mInventoryFuel->getTotalQuantityInAllSlots(fuelItem);
		}
	}
	return itemsAvailable;
}

FTimespan UFuelComponent::GetCurrentFuelTimeRemaining()
{
	FTimespan timespan;
	timespan.FromSeconds(mTimeRemaining);
	return timespan;
}

FTimespan UFuelComponent::GetTotalFuelTimeAvailable()
{
	FTimespan timespan(0,0,0);
	if (IsValid(mInventoryFuel))
	{
		for (const FName fuelItem : FuelItemsAllowed)
		{
			const int ttlInSlot = mInventoryFuel->getTotalQuantityInAllSlots(fuelItem);
			const FStFuelData fuelData = UFuelSystem::getFuelItemFromName(fuelItem);
			FTimespan burnTime = UFuelSystem::getFuelBurnTime(fuelData);
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


// Called when the game starts
void UFuelComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bStartActivated)
		StartFuelSystem();
}

void UFuelComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UFuelComponent, mInventoryStatic, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFuelComponent, mInventoryFuel, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFuelComponent, mCurrentFuelItem, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFuelComponent, mTimeRemaining, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UFuelComponent, mTickRate, COND_OwnerOnly);
	DOREPLIFETIME(UFuelComponent, bIsRunning);
}

void UFuelComponent::CheckForConsumption()
{
	if (GetOwner()->HasAuthority())
	{
		bool runSystem = false;
		
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
				runSystem = !isOverflowing && IsReserveFuelAvailable();
				if (runSystem)
					runSystem = ConsumeQueuedItem();
			}
		}
		
		if (!bIsRunning)
			bIsRunning = runSystem;
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
	const FStFuelData fuelData = UFuelSystem::getFuelItemFromName(mCurrentFuelItem);
	if (UItemSystem::getItemNameIsValid(fuelData.properName))
	{
		for (const TPair<FName,int> byProduct : fuelData.byProducts)
		{
			if (IsValid(mInventoryStatic))
			{
				mInventoryStatic->addItemByName(byProduct.Key, byProduct.Value, true, true, false);
			}
			else
			{
				FTransform spawnTransform(
					FRotator(FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f),FMath::RandRange(0.f,359.f)),
					GetOwner()->GetActorLocation());
				spawnTransform.SetLocation(spawnTransform.GetLocation() + FVector(0,0,128));
				
				FActorSpawnParameters spawnParams;
				spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        	
				FStItemData itemCopy    = UItemSystem::getItemDataFromItemName(byProduct.Key);

				APickupActorBase* pickupItem = GetWorld()->SpawnActorDeferred<APickupActorBase>(
														APickupActorBase::StaticClass(), spawnTransform);
				if (IsValid(pickupItem))
				{
					pickupItem->SetupItemFromData(itemCopy, byProduct.Value);
					pickupItem->FinishSpawning(spawnTransform);
				}
			}
		}
	}
	isOverflowing = false;
}

void UFuelComponent::OnRep_FuelQuantity_Implementation()
{
	OnFuelUpdated.Broadcast(-1);
}

void UFuelComponent::OnRep_FuelItemData_Implementation()
{
	OnFuelUpdated.Broadcast(-1);
}

void UFuelComponent::OnRep_IsRunning_Implementation()
{
	OnFuelSystemToggled.Broadcast(bIsRunning);
}

