
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhyCrawlie.generated.h"

class USphereComponent;
class USkeletalMeshComponent;

UCLASS()
class PHY_API APhyCrawlie : public AActor
{
	GENERATED_BODY()	
	
public:	
	APhyCrawlie();

	UPROPERTY()
	float DTime;
	UPROPERTY()
	USphereComponent* Root;
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	USkeletalMeshComponent* SkeletalMesh;
	UPROPERTY()
	float ColliderRadius = 10;
	UPROPERTY()
	float ForwardSpeed = 30;
	UPROPERTY()
	float TraceAheadDistance = 25;
	UPROPERTY()
	float MaxStepHeight;

private:
	UPROPERTY()
	int CurrentTurnRateInDegrees = 0;
	UPROPERTY()
	float TimeOfNextTurnRateChange = 0;
	UPROPERTY()
	bool bIsSwitchingSurface = false;
	UPROPERTY()
	bool bIsGoingUp = false;
	UPROPERTY()
	bool bIsGoingDown = false;
	UPROPERTY()
	int SurfaceSwitchesDone = 0;
	UPROPERTY()
	FTransform OldTransform;
	UPROPERTY()
	FTransform TargetTransform;
	UPROPERTY()
	bool bIsWallAhead = false;
	UPROPERTY()
	bool bGapAhead = false;
	UPROPERTY()
	FHitResult LastVoidHit;
	UPROPERTY()
	float LerpValue = 0;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	void GoToNewSurface();
	void TraceForBarrier();
	void TraceFloor();
	void TraceAhead();
	void SetTransforms(FHitResult* HitResultR, FHitResult* HitResultL, float TraceWidth);
	void SetNextTimeOfChangeInTurnRate();
	void UpdateTurnRate();
	void SetSpeed(int NewSpeed);
	void Move();
};
