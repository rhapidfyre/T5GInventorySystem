#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "lib/InventorySlot.h"

#include "PickupActorBase.generated.h"

UCLASS(BlueprintType)
class T5GINVENTORYSYSTEM_API APickupActorBase : public AStaticMeshActor
{
	GENERATED_BODY()
public:

	APickupActorBase();

	virtual void Tick(float DeltaTime) override;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PostLoad() override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actor Settings");
	float SphereRadius = 64.0f;

	UFUNCTION(BlueprintCallable)
	void SetupItem(const FItemStatics& ItemData, int OrderQuantity = 1);

	UFUNCTION(BlueprintPure) int		 GetItemQuantity() const { return ItemQuantity; }
	UFUNCTION(BlueprintPure) FStItemData GetItemData() const { return ItemData; }

	UFUNCTION(BlueprintCallable)
	void OnPickedUp(AActor* targetActor);
	
protected:
	
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	virtual void CheckOverlapCall(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:

	void SetupItemData();

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USphereComponent* PickUpDetection;

	// used in blueprints to create pick up actors in the world
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UItemDataAsset* ItemDataAsset = nullptr;

	// used in blueprints to create pick up actors in the world
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int StartingQuantity = 1;;

private:

	// Used to set how many items will spawn prior to BeginPlay()
	UPROPERTY(Replicated);
	int ItemQuantity = 1;
	
	// Used to set the item that will spawn prior to BeginPlay()
	UPROPERTY(Replicated) FStItemData ItemData;
	
	// Used to simulate physics without a huge amount of bandwith use
	UPROPERTY(Replicated) FTransform WorldTransform_;
	
	FTimerHandle WaitTimer_;
	
	float SphereRadius_ = 64.0f;

	// Flipped to true while the pickup actor is processing a pickup request
	bool bIsOperating = false;

	// Flips to true once the pickup actor has initialized and can be acted upon
	bool bReady = false;
	
};
