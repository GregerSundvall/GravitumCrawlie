

#include "PhyCrawlie.h"
#include <algorithm>
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/UnitConversion.h"
#include "Kismet/KismetSystemLibrary.h"

APhyCrawlie::APhyCrawlie()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USphereComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetSphereRadius(ColliderRadius);
	Root->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(RootComponent);
}

void APhyCrawlie::BeginPlay()
{
	Super::BeginPlay();

	ForwardSpeed = 50;
	TargetTransform = GetActorTransform();
	
	AddActorLocalRotation(FRotator(0, FMath::RandRange(0, 359), 0));
	SetNextTimeOfChangeInTurnRate();
}



void APhyCrawlie::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	DTime = DeltaTime;
	
	if (GetGameTimeSinceCreation() > TimeOfNextTurnRateChange)
	{
		UpdateTurnRate();
		SetNextTimeOfChangeInTurnRate();	
	}
	
	if (bIsGoingDown || bIsGoingUp)
	{
		GoToNewSurface();
		return;
	}


	Move();

	TraceForBarrier();
	if (bIsSwitchingSurface) return;

	TraceAhead();
	if (bIsSwitchingSurface) return;
	
	TraceFloor();
}

void APhyCrawlie::GoToNewSurface()
{
	UE_LOG(LogTemp, Warning, TEXT("Going to new surface"));
	SetActorLocation(FMath::Lerp(OldTransform.GetLocation(), TargetTransform.GetLocation(), LerpValue));
	SetActorRotation(FQuat::Slerp(OldTransform.GetRotation(), TargetTransform.Rotator().Quaternion(), LerpValue));

	if (LerpValue != 1)
	{
		float LerpSpeed = ForwardSpeed * 0.3f;
		float Distance = TargetTransform.GetLocation().Size() - OldTransform.GetLocation().Size();
		Distance = FMath::Abs(Distance);
		UE_LOG(LogTemp, Warning, TEXT("Distance: %f"), Distance);
		LerpValue += DTime / Distance * LerpSpeed;
		if (LerpValue > 1) LerpValue = 1;
		if (bIsGoingDown)
		{
			UE_LOG(LogTemp, Warning, TEXT("Abort going down"));
			TraceAhead();
		}
		return;
	}

	LerpValue = 0;
	bIsGoingUp = false;
	bIsGoingDown = false;
}

void APhyCrawlie::TraceAhead()
{
	ECollisionChannel Channel = ECC_WorldStatic;
	float Radius = ColliderRadius;
	float TraceWidth = 1.0f * ColliderRadius;
	float TraceHeight = 1.6f * ColliderRadius;
	FVector LocationVector = GetActorLocation();	
	FVector TraceDistance = GetActorForwardVector() * ColliderRadius * 1.5f;
	FVector ForwardOffset = GetActorForwardVector() * ColliderRadius * 0.5f;
	FVector LowerOffset = GetActorUpVector() * -TraceHeight / 2;
	FVector UpperOffset = GetActorUpVector() * TraceHeight / 2;
	FVector RightOffset = GetActorRightVector() * TraceWidth / 2;
	FVector LeftOffset = GetActorRightVector() * -TraceWidth / 2;

	
	// Trace low. So I don't kick a toe.
	FVector StartLowRight = LocationVector + ForwardOffset + LowerOffset + RightOffset;
	FVector EndLowRight = StartLowRight + TraceDistance;
	FVector StartLowLeft = LocationVector + ForwardOffset + LowerOffset + LeftOffset;
	FVector EndLowLeft = StartLowLeft + TraceDistance;
	FHitResult HitResultLowRight;
	FHitResult HitResultLowLeft;
	GetWorld()->LineTraceSingleByChannel(HitResultLowRight, StartLowRight, EndLowRight, Channel);
	GetWorld()->LineTraceSingleByChannel(HitResultLowLeft, StartLowLeft, EndLowLeft, Channel);

	if (HitResultLowLeft.bBlockingHit && HitResultLowRight.bBlockingHit)
	{
		SetTransforms(&HitResultLowRight, &HitResultLowLeft, TraceWidth);
		bIsGoingUp = true;
		bIsGoingDown = false;
		UE_LOG(LogTemp, Warning, TEXT("Obstacle LOW, going up"));
		return;
	}

	
	// Nothing down low. Check at actor center elevation
	FVector StartMidRight = LocationVector + ForwardOffset + RightOffset;
	FVector EndMidRight = StartMidRight + TraceDistance;
	FVector StartMidLeft = LocationVector + ForwardOffset + LeftOffset;
	FVector EndMidLeft = StartMidLeft + TraceDistance;
	FHitResult HitResultMidRight;
	FHitResult HitResultMidLeft;
	GetWorld()->LineTraceSingleByChannel(HitResultMidRight, StartMidRight, EndMidRight, Channel);
	GetWorld()->LineTraceSingleByChannel(HitResultMidLeft, StartMidLeft, EndMidLeft, Channel);

	if (HitResultMidLeft.bBlockingHit && HitResultMidRight.bBlockingHit)
	{
		SetTransforms(&HitResultMidRight, &HitResultMidLeft, TraceWidth);
		bIsGoingUp = true;
		bIsGoingDown = false;
		UE_LOG(LogTemp, Warning, TEXT("Obstacle MID, going up"));

		return;
	}

	
	// Nothing at center elevation either. Check higher so as not to bump my head.
	FVector StartHighRight = LocationVector + ForwardOffset + UpperOffset + RightOffset;
	FVector EndHighRight = StartHighRight + TraceDistance;
	FVector StartHighLeft = LocationVector + ForwardOffset + UpperOffset + LeftOffset;
	FVector EndHighLeft = StartHighLeft + TraceDistance;
	FHitResult HitResultHighRight;
	FHitResult HitResultHighLeft;
	GetWorld()->LineTraceSingleByChannel(HitResultHighRight, StartHighRight, EndHighRight, Channel);
	GetWorld()->LineTraceSingleByChannel(HitResultHighLeft, StartHighLeft, EndHighLeft, Channel);

	if (HitResultHighLeft.bBlockingHit && HitResultHighRight.bBlockingHit)
	{
		SetTransforms(&HitResultHighRight, &HitResultHighLeft, TraceWidth);
		bIsGoingUp = true;
		bIsGoingDown = false;
		UE_LOG(LogTemp, Warning, TEXT("Obstacle HIGH, going up"));

		return;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////

	//
	//
	// if (HitResultMidRight.bBlockingHit && HitResultMidLeft.bBlockingHit)
	// {
	// 	float LenghtRight = HitResultMidRight.Distance;
	// 	float LenghtLeft = HitResultMidLeft.Distance;
	// 	float Length = LenghtRight - LenghtLeft;
	// 	float Width = ColliderRadius * 0.4f;
	// 	float Angle = UKismetMathLibrary::Atan(Length / Width);
	// 	FVector CenterPoint = (HitResultMidRight.Location + HitResultMidLeft.Location) / 2;
	//
	// 	FVector NewUp = HitResultMidRight.Normal;
	// 	FVector NewRight = FVector::CrossProduct(HitResultMidRight.Normal, GetActorUpVector());
	// 	NewRight = NewRight.RotateAngleAxis(Angle * (180/3.14f) * 0.5f, NewUp);
	// 	FVector NewForward = FVector::CrossProduct(NewRight, NewUp);
	// 	FRotator NewRotator = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
	// 	FVector NewLocation = CenterPoint +
	// 		HitResultMidRight.Normal * ColliderRadius +
	// 		NewForward * ColliderRadius;
	// 	
	// 	TargetTransform.SetRotation(NewRotator.Quaternion());
	// 	TargetTransform.SetLocation(NewLocation);
	// 	OldTransform.SetRotation(GetActorRotation().Quaternion());
	// 	OldTransform.SetLocation(GetActorLocation());
	// 	bIsSwitchingSurface = true;
	// 	return;
	// }
	//
	// if (HitResultHighRight.bBlockingHit && HitResultHighLeft.bBlockingHit)
	// {
	// 	float LenghtRight = HitResultHighRight.Distance;
	// 	float LenghtLeft = HitResultHighLeft.Distance;
	// 	float Length = LenghtRight - LenghtLeft;
	// 	float Width = ColliderRadius * 0.4f;
	// 	float Angle = UKismetMathLibrary::Atan(Length / Width);
	// 	FVector CenterPoint = (HitResultHighRight.Location + HitResultHighLeft.Location) / 2;
	//
	// 	FVector NewUp = HitResultHighRight.Normal;
	// 	FVector NewRight = FVector::CrossProduct(HitResultHighRight.Normal, GetActorUpVector());
	// 	NewRight = NewRight.RotateAngleAxis(Angle * (180/3.14f) * 0.5f, NewUp);
	// 	FVector NewForward = FVector::CrossProduct(NewRight, NewUp);
	// 	FRotator NewRotator = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
	// 	FVector NewLocation = CenterPoint +
	// 		HitResultHighRight.Normal * ColliderRadius +
	// 		NewForward * ColliderRadius;
	// 	
	// 	TargetTransform.SetRotation(NewRotator.Quaternion());
	// 	TargetTransform.SetLocation(NewLocation);
	// 	OldTransform.SetRotation(GetActorRotation().Quaternion());
	// 	OldTransform.SetLocation(GetActorLocation());
	// 	bIsSwitchingSurface = true;
	// }
}

void APhyCrawlie::SetTransforms(FHitResult* HitResultR, FHitResult* HitResultL, float TraceWidth)
{
	float LengthDiff = HitResultR->Distance - HitResultL->Distance;
	float Angle = UKismetMathLibrary::Atan(LengthDiff / TraceWidth);
	FVector CenterPoint = (HitResultR->Location + HitResultL->Location) / 2;
	
	FVector NewUp = HitResultR->Normal;
	FVector NewRight = FVector::CrossProduct(HitResultR->Normal, GetActorUpVector());
	NewRight = NewRight.RotateAngleAxis(Angle * (180/PI), NewUp);
	FVector NewForward = FVector::CrossProduct(NewRight, NewUp);
	FRotator NewRotator = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
	FVector NewLocation = CenterPoint + NewForward * ColliderRadius + HitResultR->Normal * ColliderRadius;
		
	TargetTransform.SetRotation(NewRotator.Quaternion());
	TargetTransform.SetLocation(NewLocation);
	OldTransform.SetRotation(GetActorRotation().Quaternion());
	OldTransform.SetLocation(GetActorLocation());
}


void APhyCrawlie::TraceForBarrier()
{
	FVector Start = GetActorLocation();
	FVector End = GetActorLocation() + GetActorForwardVector() * ColliderRadius * 3;
	FHitResult HitResult;
	ECollisionChannel CrawlieBarrierChannel = ECollisionChannel::ECC_GameTraceChannel2;

	GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, CrawlieBarrierChannel);
	if (HitResult.bBlockingHit)
	{
		// UE_LOG(LogTemp, Warning, TEXT("Barrier, turn around"))
		OldTransform.SetLocation(GetActorLocation());
		OldTransform.SetRotation(GetActorRotation().Quaternion());
		FRotator NewRotation = GetActorRotation() + FRotator(180, 0, 0);
		TargetTransform.SetRotation(NewRotation.Quaternion());
		TargetTransform.SetLocation(GetActorLocation() + GetActorForwardVector() * -ColliderRadius);

		bIsGoingUp = true;
	}
}

void APhyCrawlie::TraceFloor()
{
	ECollisionChannel Channel = ECC_WorldStatic;

	// Trace below center of actor
	FVector Start = GetActorLocation() + GetActorUpVector() * ColliderRadius * -0.9f;
	FVector End = Start + GetActorUpVector() * ColliderRadius * -0.2f;
	FHitResult HitResult;
	GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, Channel);

	if (HitResult.bBlockingHit)
	{
		// Fine tune distance to floor
		float DistanceToFloor = HitResult.Distance + ColliderRadius * 0.9f;
		// AddActorLocalOffset(FVector(0, 0, DistanceToFloor - ColliderRadius));
		// UE_LOG(LogTemp, Warning, TEXT("Elevation adjusted with %f"), DistanceToFloor - ColliderRadius);
		// GEngine->AddOnScreenDebugMessage(1, 3.f, FColor::Red,
		// 	FString::Printf(TEXT("Distance to floor adjusted by %f"), DistanceToFloor - ColliderRadius));
		return;
	}


	// Substep tracing below front half of collider.
	// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
	// 	FString::Printf(TEXT("Not grounded. Checking a little further ahead.")));
	int Steps = 6;

	FHitResult HitResultSubstep;
	for (int i = 0; i < Steps; ++i)
	{
		FVector StartSubstep = GetActorLocation() +
			GetActorUpVector() * (ColliderRadius * -0.9f) +
			GetActorForwardVector() * (ColliderRadius * i / (Steps - 1));
		FVector EndSubstep = StartSubstep + GetActorUpVector() * (ColliderRadius * -0.2f);
		GetWorld()->LineTraceSingleByChannel(HitResultSubstep, StartSubstep, EndSubstep, Channel);
		// DrawDebugLine(GetWorld(), StartSubstep, EndSubstep, HitResultSubstep.bBlockingHit? FColor::Green : FColor::Red);
		if (HitResultSubstep.bBlockingHit)
		{
			// Found something close enough. Just keep going.
			UE_LOG(LogTemp, Warning, TEXT("Floor seems uneven, but ok"));
			// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
			// 	FString::Printf(TEXT("Found floor a little ahead")));
			return;
		}
	}
	

	// Trace for lower ground
	// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
	// 	FString::Printf(TEXT("Not grounded. Checking deeper.")));
	
	FVector Start2 = GetActorLocation() +
					GetActorUpVector() * -ColliderRadius;
	FVector End2 = Start2 +
					GetActorUpVector() * ColliderRadius * -1 +
					GetActorForwardVector() * ColliderRadius * -2;
	FHitResult HitResult2;
	GetWorld()->LineTraceSingleByChannel(HitResult2, Start2, End2, Channel);

	if (HitResult2.bBlockingHit)
	{
		// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
		// 	FString::Printf(TEXT("Found new lower floor")));

		// Get angle of edge data
		FVector StartRight = Start2 + GetActorRightVector() * 0.2f;
		FVector EndRight = End2 + GetActorRightVector() * 0.2f;
		FVector StartLeft = Start2 + GetActorRightVector() * -0.2f;
		FVector EndLeft = End2 + GetActorRightVector() * -0.2f;
		FHitResult HitResultRight;
		FHitResult HitResultLeft;
		GetWorld()->LineTraceSingleByChannel(HitResultRight, StartRight, EndRight, Channel);
		GetWorld()->LineTraceSingleByChannel(HitResultLeft, StartLeft, EndLeft, Channel);
		float Diff = HitResultRight.Distance - HitResultLeft.Distance;
		float Width = 0.4f;
		float Angle = UKismetMathLibrary::Atan(Diff / Width);
		
		FVector NewUp = HitResult2.ImpactNormal;
		FVector NewForward = FVector::CrossProduct(GetActorRightVector(), NewUp);
		NewForward = NewForward.RotateAngleAxis(-Angle * (180 / PI) * 1, NewUp);
		FVector NewRight = FVector::CrossProduct(NewUp, NewForward);
		FRotator NewRotation = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
		FVector NewLocation = HitResult2.ImpactPoint +
			HitResult2.ImpactNormal * ColliderRadius +
			NewForward * ColliderRadius;
		
		
		TargetTransform.SetRotation(NewRotation.Quaternion());
		TargetTransform.SetLocation(NewLocation);
		OldTransform.SetRotation(GetActorRotation().Quaternion());
		OldTransform.SetLocation(GetActorLocation());
		bIsGoingDown = true;
		UE_LOG(LogTemp, Warning, TEXT("Going down"));

		return;
	}

	// Trace from below and back/up. Am I on a plane?
	// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
	// FString::Printf(TEXT("Still not grounded. Am I on a plane? Checking.")));
	
	FVector Start3 = GetActorLocation() +
					GetActorUpVector() * ColliderRadius * -1.2f;
	FVector End3 = Start3 +
					GetActorUpVector() * ColliderRadius +
					GetActorForwardVector() * ColliderRadius * -1;
	FHitResult HitResult3;
	GetWorld()->LineTraceSingleByChannel(HitResult3, Start3, End3, Channel);

	if (HitResult2.bBlockingHit)
	{
		// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
		// 	FString::Printf(TEXT("Found new floor on the flipside")));

		// Get angle of edge data
		FVector StartRight = Start2 + GetActorRightVector() * 0.2f;
		FVector EndRight = End2 + GetActorRightVector() * 0.2f;
		FVector StartLeft = Start2 + GetActorRightVector() * -0.2f;
		FVector EndLeft = End2 + GetActorRightVector() * -0.2f;
		FHitResult HitResultRight;
		FHitResult HitResultLeft;
		GetWorld()->LineTraceSingleByChannel(HitResultRight, StartRight, EndRight, Channel);
		GetWorld()->LineTraceSingleByChannel(HitResultLeft, StartLeft, EndLeft, Channel);
		float Diff = HitResultRight.Distance - HitResultLeft.Distance;
		float Width = 0.4f;
		float Angle = UKismetMathLibrary::Atan(Diff / Width);

		
		FVector NewUp = HitResult3.ImpactNormal;
		FVector NewForward = FVector::CrossProduct(GetActorRightVector(), NewUp);
		NewForward = NewForward.RotateAngleAxis(-Angle * (180 / PI) * 1, NewUp);
		FVector NewRight = FVector::CrossProduct(NewUp, NewForward);
		FRotator Rotator = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
		FVector NewLocation = HitResult3.Location +
			HitResult3.Normal * ColliderRadius +
			NewForward * ColliderRadius;
		
		TargetTransform.SetRotation(Rotator.Quaternion());
		TargetTransform.SetLocation(NewLocation);
		OldTransform.SetRotation(GetActorRotation().Quaternion());
		OldTransform.SetLocation(GetActorLocation());
		bIsGoingDown = true;
		UE_LOG(LogTemp, Warning, TEXT("Going to flipside"));

		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("No floor found"));

	// GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red,
	// FString::Printf(TEXT("Still no ground! I guess I'll just keep going..?")));
}



void APhyCrawlie::Move()
{
	AddActorLocalRotation(FRotator(0,CurrentTurnRateInDegrees, 0) * GetWorld()->GetDeltaSeconds());
	AddActorLocalOffset(FVector(ForwardSpeed  * GetWorld()->GetDeltaSeconds(), 0, 0));
}

void APhyCrawlie::SetNextTimeOfChangeInTurnRate()
{
	TimeOfNextTurnRateChange = GetGameTimeSinceCreation() + FMath::RandRange(0.1f, 1.5f);
}

void APhyCrawlie::UpdateTurnRate()
{
	CurrentTurnRateInDegrees = (CurrentTurnRateInDegrees += FMath::RandRange(-15, 15)) % 50;
}

void APhyCrawlie::SetSpeed(int NewSpeed)
{
	ForwardSpeed = std::clamp(NewSpeed, 0, 100);
}



	
// FVector Start = GetActorLocation() + GetActorUpVector() *  ColliderRadius * -0.9f;
// FVector End = Start +
// 	GetActorForwardVector() * ColliderRadius * 1.0f +
// 	GetActorUpVector() *  ColliderRadius * -0.2f;
// FHitResult HitResult;
// GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, Channel);
//
// if (HitResult.bBlockingHit)
// {
// 	return;
// }



	// bool bIsGrounded = false;
	// int Steps = 6;
	// for (int i = 0; i < Steps; ++i)
	// {
	// 	FVector Start = GetActorLocation() + ColliderRadius * i / (Steps - 1);
	// 	FVector End = Start + -GetActorUpVector() * ColliderRadius * 2;
	// 	FHitResult HitResult;
	// 	GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, Channel);
	// 	if (FMath::IsNearlyEqual(HitResult.Distance, ColliderRadius, 0.5f))
	// 	{
	// 		bIsGrounded = true;
	// 		break;
	// 	}
	// }
	// if (bIsGrounded)
	// {
	// 	return;
	// }

	
	// FVector StartCenter = GetActorLocation();
	// FVector EndCenter = StartCenter + GetActorUpVector() * ColliderRadius * 2;
	// FHitResult HitResultCenter;
	// GetWorld()->LineTraceSingleByChannel(HitResultCenter, StartCenter, EndCenter, Channel);
	// if (FMath::IsNearlyEqual(HitResultCenter.Distance, ColliderRadius, 0.1f))
	// {
	// 	return;
	// }
	//
	// FVector StartFront = GetActorLocation() + GetActorForwardVector() * ColliderRadius;
	// FVector EndFront = StartFront + GetActorUpVector() * ColliderRadius * 2;
	// FHitResult  HitResultFront;
	// GetWorld()->LineTraceSingleByChannel(HitResultFront, StartFront, EndFront, Channel);
	// if (FMath::IsNearlyEqual(HitResultFront.Distance, ColliderRadius, 0.1f))
	// {
	// 	return;
	// }
	//
	// FVector StartAhead = GetActorLocation() + GetActorForwardVector() * ColliderRadius * 2;
	// FVector EndAhead = StartAhead + GetActorUpVector() * ColliderRadius * 2;
	// FHitResult  HitResultAhead;
	// GetWorld()->LineTraceSingleByChannel(HitResultAhead, StartAhead, EndAhead, Channel);
	// if (FMath::IsNearlyEqual(HitResultAhead.Distance, ColliderRadius, 0.1f))
	// {
	// 	return;
	// }
	
		// SetActorTransform(FTransform(Rotator,
		// 	HitResult1.Location + HitResult1.Normal * ColliderRadius,
		// 	GetActorScale()));

		// DrawDebugLine(GetWorld(), HitResult1.Location, HitResult1.Location + NewUp * 20,
		//               FColor::Blue, false, -1, 0, 1);
		// DrawDebugLine(GetWorld(), HitResult1.Location, HitResult1.Location + NewForward * 20,
		//               FColor::Red, false, -1, 0, 1);
		// DrawDebugLine(GetWorld(), HitResult1.Location, HitResult1.Location + NewRight * 20,
		//               FColor::Green, false, -1, 0, 1);
		
		// FVector NewUp = HitResult2.Normal;
		// FVector NewForward = (HitResult2.Location - HitResult1.Location).GetSafeNormal();
		// FVector NewRight = FVector::CrossProduct(NewUp, NewForward);
		//
		//
		//
		

		// Plane projection
		// FVector NewUp = HitResult.Normal;
		// FVector NewForward = FVector::VectorPlaneProject(GetActorUpVector(), NewUp).GetSafeNormal();
		// FVector NewRight = FVector::CrossProduct(NewUp, NewForward);
		
		// FVector TargetSurfaceRight = FVector::CrossProduct(HitResult.Normal, GetActorUpVector());
		//
		// FVector NewUp = HitResult.Normal;
		// FVector NewRight = FVector::CrossProduct(HitResult.Normal, GetActorUpVector());
		// FVector NewForward = FVector::CrossProduct(NewRight, NewUp);
		// FRotator NewRotation = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
		// auto ProjectedNewRight = FVector::VectorPlaneProject(NewRight, GetActorUpVector());
		// auto DeltaZ = FVector::DotProduct(GetActorRightVector(), ProjectedNewRight.GetSafeNormal());
		//
		// auto Delta = GetActorRotation().Yaw - NewRotation.Roll;
		//
		// GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Red,
		// 	FString::Printf(TEXT("Angle: %f"), DeltaZ));
		//
		// // NewRotation.Add(0, -Delta.Yaw, 0);
		// auto Transform = FTransform(NewRotation, GetActorLocation(), GetActorScale());
		// SetActorTransform(Transform);
		
		// GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
		// 	FString::Printf(TEXT("Hit")));
		
		// DrawDebugLine(GetWorld(), HitResult.Location, HitResult.Location + NewPlaneUp * 30, FColor::Red,
		// 	0, -1, 0, 2);
		
		// FVector NewForward = FVector::CrossProduct(NewUp, NewRight);
		// SetActorRotation(UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp));
		// FRotator Target = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
		// FQuat Rotator = FQuat::Slerp(GetActorRotation().Quaternion(), Target.Quaternion(), 1);
		// SetActorRotation(Rotator);
	
	// if (bIsSwitchingSurface == false)
	// {
	// 	
	// 	bIsSwitchingSurface = true;
	// }
	
	// if (HitResult.bBlockingHit)
	// {
	// 	// UE_LOG(LogTemp, Warning, TEXT("Going UP"));
	//
	// 	// Align X rotation to wall
	// 	float Delta = HitResult.Normal.Dot(-GetActorForwardVector());
	// 	if (Delta != 0)
	// 	{
	// 		Delta = FMath::Acos(Delta);
	// 		Delta = UKismetMathLibrary::RadiansToDegrees(Delta);
	// 		UE_LOG(LogTemp,Warning, TEXT("%f"), Delta);
	// 		AddActorLocalRotation(FRotator(0, 0, Delta / 10));
	// 	}
	// 	AddActorLocalRotation(FRotator(ForwardSpeed * PitchMultiplier * DeltaTime, 0, 0));
	// 	AddActorLocalOffset(FVector(0, 0, ForwardSpeed* 0.5f * DeltaTime * 1));
	// 	return;
	// }



	
	// //Check below
	// bool bIsGrounded = false;
	// int Steps = 10;
	// FHitResult HitResultBelow;
	// FVector EndBelow;
	// FVector StartBelow;
	// float ColliderMultiplier;
	// for (float i = 0; i < Steps; i += 1)
	// {
	// 	ColliderMultiplier = i / (Steps - 1);
	// 	StartBelow = GetActorLocation() +
	// 		-GetActorUpVector() * ColliderRadius * 0.9f +
	// 		GetActorForwardVector() * ColliderRadius * ColliderMultiplier;
	// 	EndBelow = StartBelow +
	// 		-GetActorUpVector() * ColliderRadius * 0.2f;
	// 	GetWorld()->LineTraceSingleByChannel(HitResultBelow, StartBelow, EndBelow, Channel);
	// 	DrawDebugLine(GetWorld(), StartBelow, EndBelow, FColor::Green);
	// 	if (HitResultBelow.bBlockingHit == true)
	// 	{
	// 		bIsGrounded = true;
	// 		break;
	// 	}
	// }
	//
	// if (!bIsGrounded)
	// {
	// 	// UE_LOG(LogTemp, Warning, TEXT("Going DOWN"));
	// 	AddActorLocalRotation(FRotator(ForwardSpeed * PitchMultiplier * DeltaTime * -1, 0, 0));
	// 	AddActorLocalOffset(FVector(0, 0, ForwardSpeed * 0.5f * DeltaTime * -1));
	// 	return;
	// }
	
	
	// Move();
	// TraceForBarrier();
	// TraceAhead();
	// TraceBelow();
	// TraceForObstacle();
	// TraceForVoid();
	// ScanForObstacleAhead();
	// ScanForVoidAhead();		






// void APhyCrawlie::TraceAhead()
// {
// 	ECollisionChannel Channel = ECollisionChannel::ECC_WorldStatic;
//
// 	// Trace right in front
// 	FVector StartCenter = GetActorLocation();
// 	FVector EndCenter = StartCenter + GetActorForwardVector() * ColliderRadius;
// 	FHitResult HitResultCenter;
// 	GetWorld()->LineTraceSingleByChannel(HitResultCenter, StartCenter, EndCenter, Channel);
//
// 	// Trace just above floor
// 	FVector StartLow = StartCenter + -GetActorUpVector() * ColliderRadius * 0.9f;
// 	FVector EndLow = StartLow + GetActorForwardVector() * ColliderRadius;
// 	FHitResult HitResultLow;
// 	GetWorld()->LineTraceSingleByChannel(HitResultLow, StartLow, EndLow, Channel);
//
// 	if (!HitResultCenter.bBlockingHit && !HitResultLow.bBlockingHit)
// 	{
// 		return;
// 	}
//
// 	if (HitResultCenter.bBlockingHit)
// 	{
// 		// Center trace hit something. Consider it a wall and go up it.
// 		UE_LOG(LogTemp, Warning, TEXT("Wall ahead, go up"))
// 		TargetTransform.SetLocation(HitResultCenter.ImpactPoint + HitResultCenter.ImpactNormal * ColliderRadius);
// 		FVector NewUp = HitResultCenter.ImpactNormal;
// 		FVector NewForward = FVector::CrossProduct(GetActorRightVector(), NewUp);
// 		FVector NewRight = FVector::CrossProduct(NewUp, NewForward);
// 		auto Rotator = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
// 		TargetTransform.SetRotation(Rotator.Quaternion());
// 		
// 		SetActorTransform(TargetTransform);
// 		return;
// 	}
// 	
// 	if (HitResultLow.bBlockingHit)
// 	{
// 		// Hit on lower but not in center. Trace the height of it and step up on it.
// 		UE_LOG(LogTemp, Warning, TEXT("Obstacle, step up"))
// 		FVector StartHeightCheck = GetActorLocation() + GetActorForwardVector() * ColliderRadius;
// 		FVector EndHeightCheck = StartHeightCheck + -GetActorUpVector() * ColliderRadius;
// 		FHitResult HitResultObstacleHeight;
// 		GetWorld()->LineTraceSingleByChannel(HitResultObstacleHeight, StartHeightCheck, EndHeightCheck, Channel);
// 		float ObstacleHeight = ColliderRadius - HitResultObstacleHeight.Distance;
// 		AddActorLocalOffset(FVector(0, 0, ObstacleHeight));
// 		AddActorLocalOffset(FVector(ColliderRadius * 0.5f, 0, 0));
// 	}
// }
//
// void APhyCrawlie::TraceForObstacle()
// {
// 	
// }
//
//
// void APhyCrawlie::ScanForObstacleAhead()
// {
// 	FVector Start = GetActorLocation();
// 	FVector End = GetActorLocation() + GetActorForwardVector() * ColliderRadius * 0.5f;
// 	FCollisionShape Sphere = FCollisionShape::MakeSphere(ColliderRadius * 0.8f);
// 	FHitResult BarrierHitResult;
// 	ECollisionChannel BarrierChannel = ECollisionChannel::ECC_GameTraceChannel2;
// 	FHitResult HitResult;
// 	ECollisionChannel Channel = ECollisionChannel::ECC_WorldStatic;
//
// 	// Scan for invisible barrier box. Turn around and early out of function if so. 
// 	GetWorld()->SweepSingleByChannel(BarrierHitResult, Start, End, FQuat::Identity, BarrierChannel, Sphere);
// 	if (BarrierHitResult.bBlockingHit)
// 	{
// 		AddActorLocalRotation(FRotator(0, 180, 0));
// 		return;
// 	}
// 	
// 	// Scan for in world obstacle
// 	GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, Channel, Sphere);
// 	// DrawDebugSphere(GetWorld(), End, ColliderRadius * 0.8f, 10, HitResult.bBlockingHit? FColor::Green : FColor::Red);
// 	
// 	if (HitResult.bBlockingHit)
// 	{
// 		// Use hit info to make a new position/rotation, and apply it. 
// 		TargetTransform.SetLocation(HitResult.ImpactPoint + HitResult.ImpactNormal * ColliderRadius);
// 		FVector NewUp = HitResult.ImpactNormal;
// 		FVector NewForward = FVector::CrossProduct(GetActorRightVector(), NewUp);
// 		FVector NewRight = FVector::CrossProduct(NewUp, NewForward);
// 		auto Rotator = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
// 		TargetTransform.SetRotation(Rotator.Quaternion());
// 		
// 		SetActorTransform(TargetTransform);
// 	}
// }
//
// void APhyCrawlie::ScanForVoidAhead()
// {
// 	// Trace from bottom of collider, down and a little backwards. Backwards is to catch up to around a 90 degree edge.
// 	FVector Start = GetActorLocation() +
// 					GetActorUpVector() * -ColliderRadius;
// 	
// 	FVector End =	GetActorLocation() +
// 					GetActorForwardVector() * -1 +
// 					GetActorUpVector() * (-ColliderRadius * 3.0f);
// 	FHitResult HitResult;
// 	ECollisionChannel Channel = ECollisionChannel::ECC_WorldStatic;
// 	GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, Channel);
//
// 	// DrawDebugLine(GetWorld(), Start, End, FColor::Green);
//
// 	// If there is a void below actor, check further ahead. In case it's just a small gap.
// 	if (HitResult.Distance > 0.5f)
// 	{
// 		FVector StartLeft = GetActorLocation() +
// 							GetActorForwardVector() * ColliderRadius * 2.f +
// 							GetActorRightVector() * -ColliderRadius * 0.8f +
// 							GetActorUpVector() * ColliderRadius * -0.9f;
// 		FVector EndLeft = StartLeft + GetActorUpVector() * ColliderRadius * -1.1f;
// 		
// 		FVector StartRight = GetActorLocation() +
// 							GetActorForwardVector() * ColliderRadius * 2.f +
// 							GetActorRightVector() * ColliderRadius * 0.8f +
// 							GetActorUpVector() * ColliderRadius * -0.9f;
// 		FVector EndRight = StartRight + GetActorUpVector() * ColliderRadius * -1.1f;
//
// 		FHitResult HitResultLeft;
// 		FHitResult HitResultRight;
// 		GetWorld()->LineTraceSingleByChannel(HitResultLeft, StartLeft, EndLeft, Channel);
// 		GetWorld()->LineTraceSingleByChannel(HitResultRight, StartRight, EndRight, Channel);
// 		
// 		// DrawDebugLine(GetWorld(), StartLeft, EndLeft, FColor::Green);
// 		// DrawDebugLine(GetWorld(), StartRight, EndRight, FColor::Green);
//
// 		if (!HitResultLeft.bBlockingHit && !HitResultRight.bBlockingHit)
// 		{
// 			TargetTransform.SetLocation(HitResult.ImpactPoint + HitResult.ImpactNormal * ColliderRadius);
// 			FVector NewUp = HitResult.ImpactNormal;
// 			FVector NewForward = FVector::CrossProduct(GetActorRightVector(), NewUp);
// 			FVector NewRight = FVector::CrossProduct(NewUp, NewForward);
// 			auto Rotator = UKismetMathLibrary::MakeRotationFromAxes(NewForward, NewRight, NewUp);
// 			TargetTransform.SetRotation(Rotator.Quaternion());
// 			SetActorTransform(TargetTransform);
// 		}
// 	}
// }









