// Fill out your copyright notice in the Description page of Project Settings.

#include "NTGame.h"
#include "NTPlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "NTPawn.h"

ANTPawn::ANTPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	RootCollision = ObjectInitializer.CreateDefaultSubobject<UBoxComponent>(this, TEXT("RootCollision"));
	RootCollision->SetSimulatePhysics(true);
	RootCollision->SetCollisionResponseToAllChannels(ECR_Block);
	RootComponent = RootCollision;

	RootMesh = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("RootMesh"));
	RootMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//RootMesh->SetAbsolute(true, true, false);
	RootMesh->SetupAttachment(RootCollision);

	ViewCamera = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(RootMesh);
	ViewCamera->bAbsoluteLocation = true;
	ViewCamera->bAbsoluteRotation = true;
	ViewCamera->bAbsoluteScale = true;
	ViewCamera->SetWorldLocation(FVector(0.f, 430.0f, 100.0f));
	ViewCamera->SetWorldRotation(FRotator(0.f, 90.f, 0.f));

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	//PrimaryActorTick.TickGroup = ETickingGroup::TG_PrePhysics;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	bReplicateMovement = false;

	// Defaults
	ForceStrength = 1500.0f;
	Accel = FVector::ZeroVector;
	InputStates = FCubeInput();

	PreviousPhysState = FCubeState();
	CurrentPhysState = FCubeState();

	SmoothAlpha = 0.f;
	StartSmoothAlpha = 0.1f;
	DefaultSmoothAlpha = 0.25f;

	MaxHistoryStates = 100;
}

void ANTPawn::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	StoredMoves.Resize(MaxHistoryStates);
	ImportantMoves.Resize(MaxHistoryStates);
}

void ANTPawn::Tick(float DeltaSeconds)
{
	PreviousPhysState = CurrentPhysState;

	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled())
 	{
 		// Store the Move in History.
 		// The moves are stored at the time we *think* they'll be when they reach the server.
 		UpdateHistoryBuffer(GetTimeFromController(true));
 
 		if (Role < ROLE_Authority)
 		{
 			Server_SimulateInput(InputStates);
 		}
 	}
 
 	CalculateAccel(DeltaSeconds, InputStates);
 
   	if (!bReplayingMoves)
   	{
   		SmoothToState(CurrentPhysState, SmoothAlpha);
   	}
 
 	SmoothAlpha += (DefaultSmoothAlpha - SmoothAlpha) * DeltaSeconds;
 	GEngine->AddOnScreenDebugMessage(-1, GetWorld()->GetDeltaSeconds(), FColor::Green, FString::Printf(TEXT("Client Smooth: %f"), SmoothAlpha));
 
 	VisualizeMoveHistory();

	// Update Visual Mesh Before Rendering
	Interpolate(PreviousPhysState, PreviousPhysState, 1.f);
}

void ANTPawn::Interpolate(const FCubeState& FromState, const FCubeState& ToState, float Alpha /*= 1.f*/)
{
	const FVector NewPos = UKismetMathLibrary::VLerp(FromState.Position, ToState.Position, Alpha);
	const FVector NewVel = UKismetMathLibrary::VLerp(FromState.Velocity, ToState.Velocity, Alpha);
	const FVector NewAngVel = UKismetMathLibrary::VLerp(FromState.AngularVelocity, ToState.AngularVelocity, Alpha);
	const FQuat NewOrienation = FQuat::Slerp(FromState.Rotation, ToState.Rotation, Alpha);

	RootCollision->SetPhysicsLinearVelocity(NewVel);
	RootCollision->SetPhysicsAngularVelocity(NewAngVel);
	RootCollision->SetWorldLocationAndRotationNoPhysics(NewPos, NewOrienation.Rotator());
}

void ANTPawn::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);

 	InputComponent->BindAction("Forward", EInputEvent::IE_Pressed, this, &ANTPawn::OnStartForward);
 	InputComponent->BindAction("Forward", EInputEvent::IE_Released, this, &ANTPawn::OnStopForward);
 
 	InputComponent->BindAction("Backward", EInputEvent::IE_Pressed, this, &ANTPawn::OnStartBackward);
 	InputComponent->BindAction("Backward", EInputEvent::IE_Released, this, &ANTPawn::OnStopBackward);
 
 	InputComponent->BindAction("Left", EInputEvent::IE_Pressed, this, &ANTPawn::OnStartLeft);
 	InputComponent->BindAction("Left", EInputEvent::IE_Released, this, &ANTPawn::OnStopLeft);
 
 	InputComponent->BindAction("Right", EInputEvent::IE_Pressed, this, &ANTPawn::OnStartRight);
 	InputComponent->BindAction("Right", EInputEvent::IE_Released, this, &ANTPawn::OnStopRight);
}

void ANTPawn::StartCorrection(const FCubeMove& TargetMove, const FCubeMove& SavedMove)
{
	// Time different between current time and server time. AKA, how long we have to blend in MS
	const uint32 TimeToMakeMove = GetTimeFromController(false) - TargetMove.TimeStamp;

	// TODO - Use Acceleration Instead!
	// Difference between the Clients' Velocity and the Target Velocity at that timestamp
	const FVector CorrectiveVelocity = TargetMove.CubeState.Velocity - SavedMove.CubeState.Velocity;
	const FVector CorrectiveAngVelocity = TargetMove.CubeState.AngularVelocity - SavedMove.CubeState.AngularVelocity;
}

void ANTPawn::OnRep_ServerMoveData()
{
	const FCubeState OriginalState = CurrentPhysState;
	HistoryCorrection(this, ServerMoveData);

	if (OriginalState.Compare(CurrentPhysState))
	{
		BeginSmoothing();
	}
}

void ANTPawn::UpdateHistoryBuffer(int32 ForTime)
{
	FCubeMove NewMove;
	NewMove.TimeStamp = ForTime;
	NewMove.CubeInput = InputStates;
	NewMove.CubeState = CurrentPhysState;
	AddMoveToHistory(NewMove);
}

void ANTPawn::CalculateAccel(float DeltaSeconds, const FCubeInput& FromInput)
{
	Accel = FVector::ZeroVector;
	Alpha = FVector::ZeroVector;

	/* Calculate Local Movement */
	if (FromInput.Left) { Accel.X -= ForceStrength; }
	if (FromInput.Right) { Accel.X += ForceStrength; }
	if (FromInput.Forward) { Accel.Y -= ForceStrength; }
	if (FromInput.Backward) { Accel.Y += ForceStrength; }

	FVector Veloc = RootCollision->GetPhysicsLinearVelocity();
	FVector Omega = RootCollision->GetPhysicsAngularVelocity();

	Veloc += Accel * DeltaSeconds;
	Omega += Alpha * DeltaSeconds;

	RootCollision->SetPhysicsLinearVelocity(Accel * DeltaSeconds, true);
	RootCollision->SetPhysicsAngularVelocity(Alpha * DeltaSeconds, true);

	// Update Current Physics State
	CurrentPhysState.Velocity = RootCollision->GetPhysicsLinearVelocity();
	CurrentPhysState.AngularVelocity = RootCollision->GetPhysicsAngularVelocity();
	CurrentPhysState.Position = RootCollision->GetComponentLocation();
	CurrentPhysState.Rotation = RootCollision->GetComponentQuat();

	// If Server, Send State Back
 	if (Role == ROLE_Authority && !IsLocallyControlled())
 	{
 		FCubeMove NewMove = FCubeMove();
 		NewMove.CubeInput = FromInput;
 		NewMove.CubeState = CurrentPhysState;
 		NewMove.TimeStamp = GetTimeFromController(false);
 
 		ServerMoveData = NewMove;
 	}
}

void ANTPawn::OnRep_ReplicatedMovement()
{
	if (GetNetMode() == NM_Client && IsLocallyControlled())
	{
		return;
	}
	else
	{
		Super::OnRep_ReplicatedMovement();
	}
}

void ANTPawn::Server_SimulateInput_Implementation(const FCubeInput& FromInput)
{
	InputStates = FromInput;
}

bool ANTPawn::Server_SimulateInput_Validate(const FCubeInput& FromInput)
{
	return true;
}

void ANTPawn::VisualizeMoveHistory()
{
	if (IsLocallyControlled())
	{
		const FColor NewColour = FLinearColor(1.f, 1.f, 1.f, 0.5f).ToFColor(false);
		for (uint32 i = 0; i < StoredMoves.GetArraySize(); i++)
		{
			FCubeMove BufferedMove = StoredMoves[i];
			DrawDebugBox(GetWorld(), BufferedMove.CubeState.Position, FVector(25.f, 25.f, 25.f), BufferedMove.CubeState.Rotation, NewColour, false, GetWorld()->GetDeltaSeconds() + 0.01f, 1);
		}
	}
}

/////////////////////////////////////////////
///// CLIENT RECIEVES SERVER CORRECTION /////
/////////////////////////////////////////////

void ANTPawn::Client_SendCorrection_Implementation(const FCubeMove& CorrectedMove)
{
	GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, FString::Printf(TEXT("Client Recieved Correction: T %i Pos %f %f %f"), CorrectedMove.TimeStamp, CorrectedMove.CubeState.Position.X, CorrectedMove.CubeState.Position.Y, CorrectedMove.CubeState.Position.Z));

	const FCubeState OriginalState = CurrentPhysState;
	HistoryCorrection(this, CorrectedMove);

	// Smooth to our actual physics state from what we currently are.
	//if (OriginalState.Compare(CurrentPhysState))
	//{
//		BeginSmoothing();
//	}
}

void ANTPawn::BeginSmoothing()
{
	SmoothAlpha = StartSmoothAlpha;
}

void ANTPawn::SmoothToState(const FCubeState& TargetState, float Alpha)
{
	PreviousPhysState = CurrentPhysState;
	CurrentPhysState = TargetState;

	CurrentPhysState.Position = PreviousPhysState.Position + (TargetState.Position - PreviousPhysState.Position) * Alpha;
	CurrentPhysState.Rotation = FQuat::Slerp(PreviousPhysState.Rotation, TargetState.Rotation, Alpha);
	CurrentPhysState.Velocity = PreviousPhysState.Velocity + (TargetState.Velocity - PreviousPhysState.Velocity) * Alpha;
	CurrentPhysState.AngularVelocity = PreviousPhysState.AngularVelocity + (TargetState.AngularVelocity - PreviousPhysState.AngularVelocity) * Alpha;

	// TODO - Collision Check First!
	//GetWorld()->EncroachingBlockingGeometry()

	// TODO - Location Blending would be better as opposed to a hard set!
	RootCollision->SetWorldLocation(CurrentPhysState.Position);
	RootCollision->SetWorldRotation(CurrentPhysState.Rotation.Rotator());
	RootCollision->SetPhysicsLinearVelocity(CurrentPhysState.Velocity);
	RootCollision->SetPhysicsAngularVelocity(CurrentPhysState.AngularVelocity);
}

void ANTPawn::Snap(const FCubeState& NewState)
{
	CurrentPhysState = NewState;

	/* TODO - Commenting this out effectively stops the snapping, but shows the history in the right place. */
	RootCollision->SetWorldLocation(NewState.Position);
	RootCollision->SetWorldRotation(NewState.Rotation.Rotator());
	RootCollision->SetPhysicsLinearVelocity(NewState.Velocity);
	RootCollision->SetPhysicsAngularVelocity(NewState.AngularVelocity);
}

//////////////////
///// FORCES /////
//////////////////

void ANTPawn::OnStartForward()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Forward != true)
		{
			InputStates.Forward = true;
		}
	}
}

void ANTPawn::OnStopForward()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Forward != false)
		{
			InputStates.Forward = false;
		}
	}
}

void ANTPawn::OnStartBackward()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Backward != true)
		{
			InputStates.Backward = true;
		}
	}
}

void ANTPawn::OnStopBackward()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Backward != false)
		{
			InputStates.Backward = false;
		}
	}
}

void ANTPawn::OnStartLeft()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Left != true)
		{
			InputStates.Left = true;
		}
	}
}

void ANTPawn::OnStopLeft()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Left != false)
		{
			InputStates.Left = false;
		}
	}
}

void ANTPawn::OnStartRight()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Right != true)
		{
			InputStates.Right = true;
		}
	}
}

void ANTPawn::OnStopRight()
{
	if (IsLocallyControlled())
	{
		if (InputStates.Right != false)
		{
			InputStates.Right = false;
		}
	}
}

int32 ANTPawn::GetTimeFromController(bool bNetworkTime)
{
	ANTPlayerController* LocalPC = Cast<ANTPlayerController>(GetController());
	if (LocalPC)
	{
		if (bNetworkTime)
		{
			return LocalPC->GetNetworkTime();
		}
		else
		{
			return LocalPC->GetLocalTime();
		}
	}

	return 0;
}

///////////////////////////////////
///// MOVE HISTORY MANAGEMENT /////
///////////////////////////////////

void ANTPawn::AddMoveToHistory(const FCubeMove& NewMove)
{
	bool bImportant = true;
	if (!StoredMoves.IsEmpty())
	{
		const FCubeMove& PreviousMove = StoredMoves.Newest();

		// TODO: Could just Override the Operator in the struct for this!
		bImportant = (NewMove.CubeInput.Forward != PreviousMove.CubeInput.Forward) || (NewMove.CubeInput.Backward != PreviousMove.CubeInput.Backward) || (NewMove.CubeInput.Left != PreviousMove.CubeInput.Left) || (NewMove.CubeInput.Right != PreviousMove.CubeInput.Right);
	}

	if (bImportant)
	{
		ImportantMoves.Add(NewMove);
	}

	StoredMoves.Add(NewMove);
}

void ANTPawn::HistoryCorrection(ANTPawn* InActor, const FCubeMove& MoveData)
{
	const int32 Time = MoveData.TimeStamp;

	// Discard Out of Date Moves
	while (ImportantMoves.Oldest().TimeStamp < Time && !ImportantMoves.IsEmpty())
	{
		ImportantMoves.Remove();
	}
	while (StoredMoves.Oldest().TimeStamp < Time && !StoredMoves.IsEmpty())
	{
		StoredMoves.Remove();
	}

	// If we have no stored moves, then we want to exit out of here
	if (StoredMoves.IsEmpty())
	{
		return;
	}

	FColor TextColor;

	// Check if Timestamps are Equal - Which they may not be! We only really want to correct the right moves!
	if (MoveData.TimeStamp != StoredMoves.Oldest().TimeStamp)
	{
		TextColor = FColor::Red;
	}
	else
	{
		TextColor = FColor::Blue;
	}
	
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, TextColor, FString::Printf(TEXT("Recieved = %i - Stored = %i"), MoveData.TimeStamp, StoredMoves.Oldest().TimeStamp));

	// Compare the Moves at current time, to see if they are equal
// 	if (MoveData.CubeState.Compare(StoredMoves.Oldest().CubeState))
// 	{
// 		// Discard Corrected Move
// 		StoredMoves.Remove();
// 
// 		// Save Current Scene Data
// 		FCubeInput SavedInput = InActor->InputStates;
// 
// 		// Rewind to Correction, and Replay Moves After the Corrected Move
// 		InActor->bReplayingMoves = true;
// 		InActor->InputStates = MoveData.CubeInput;
// 
// 		FVector AvgVelocity
// 
// 
// 
// 	}
	
 	// Compare correction State with move history state
 	if (MoveData.CubeState != StoredMoves.Oldest().CubeState)
 	{
 		// Discard Corrected Move
 		StoredMoves.Remove();
 
 		// Saved Current Scene Data
 		int32 SavedTimeStamp = InActor->GetTimeFromController(true);
 		FCubeInput SavedInput = InActor->InputStates;
 
 		// Rewind to Correction, and replay moves
 		InActor->InputStates = MoveData.CubeInput;
 
 		// Dummy Current State
 		//FCubePhysState
 		InActor->Snap(MoveData.CubeState);
 		InActor->bReplayingMoves = true;
 
 		uint32 i = StoredMoves.Tail;
 
 		while (i != StoredMoves.Head)
 		{
 			const int32 SceneTime = InActor->GetTimeFromController(true);
 
 			while (SceneTime < StoredMoves[i].TimeStamp)
 			{
 				InActor->UpdateHistoryBuffer(SceneTime);
 			}
 
 			InActor->InputStates = StoredMoves[i].CubeInput;
 			StoredMoves[i].CubeState = InActor->CurrentPhysState;
 			StoredMoves.Next(i);
 		}
 
 		InActor->UpdateHistoryBuffer(InActor->GetTimeFromController(true));
 
 		InActor->bReplayingMoves = false;
 		InActor->InputStates = SavedInput;
 	}
}

///////////////////////
///// REPLICATION /////
///////////////////////

void ANTPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANTPawn, ServerMoveData, COND_OwnerOnly);
}