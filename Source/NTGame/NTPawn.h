// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Pawn.h"
#include "NTPawn.generated.h"

USTRUCT()
struct FCubeState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FVector Position;
	UPROPERTY()
	FVector Velocity;
	UPROPERTY()
	FVector AngularVelocity;
	UPROPERTY()
	FQuat Rotation;

	bool operator==(const FCubeState& Other) const
	{
		return Position == Other.Position && Velocity == Other.Velocity && Rotation == Other.Rotation && AngularVelocity == Other.AngularVelocity;
	}

	bool operator!=(const FCubeState& Other) const
	{
		return !(*this == Other);
	}

	// Comparison Operator. Used to figure out if the other state has a 'significant' difference.
	bool Compare(const FCubeState& Other) const
	{
		const float Threshold = FMath::Square(0.1f);
		const FQuat RotAxis = FQuat(Other.Rotation - Rotation);
		const float RotNorm = FMath::Square(RotAxis.W) + FMath::Square(RotAxis.X) + FMath::Square(RotAxis.Y) + FMath::Square(RotAxis.Z);
		const float LocSize = (Other.Position - Position).SizeSquared();

		if (LocSize > (Threshold * Threshold) || RotNorm > Threshold)
		{
			return true;
		}

		return false;
	}

	FCubeState()
		: Position(ForceInitToZero)
		, Rotation(ForceInitToZero)
		, Velocity(ForceInitToZero)
		, AngularVelocity(ForceInitToZero)
	{}
};

USTRUCT()
struct FCubeInput
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	bool Forward;
	UPROPERTY()
	bool Backward;
	UPROPERTY()
	bool Left;
	UPROPERTY()
	bool Right;

	FCubeInput()
		: Forward(false)
		, Backward(false)
		, Left(false)
		, Right(false)
	{}
};

USTRUCT()
struct FCubeMove
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 TimeStamp;
	UPROPERTY()
	int32 RandHash;
	UPROPERTY()
	FCubeState CubeState;
	UPROPERTY()
	FCubeInput CubeInput;

	FCubeMove()
		: TimeStamp(0)
		, RandHash(0)
		, CubeState(FCubeState())
		, CubeInput(FCubeInput())
	{}
};

USTRUCT()
struct FCubeMoveBuffer
{
	GENERATED_USTRUCT_BODY()

	uint32 Head;
	uint32 Tail;

	FCubeMoveBuffer()
		: Head(0)
		, Tail(0)
	{}

	// Re-sizes the Move Buffer
	void Resize(uint32 NewSize)
	{
		Head = 0;
		Tail = 0;
		MoveArray.SetNumUninitialized(NewSize);
	}

	// Gets current size of Move Buffer
	uint32 GetSize()
	{
		uint32 Count = Head - Tail;
		if (Count < 0)
		{
			Count += MoveArray.Num();
		}
		return Count;
	}

	uint32 GetArraySize()
	{
		return (uint32)MoveArray.Num();
	}

	// Adds Move to Move Buffer
	void Add(const FCubeMove& NewMove)
	{
		MoveArray[Head] = NewMove;
		Next(Head);
	}

	// Removes Oldest Move from Buffer
	void Remove()
	{
		ensure(!IsEmpty());
		Next(Tail);
	}

	// Returns Oldest Move
	FCubeMove& Oldest()
	{
		//check(!IsEmpty());
		return MoveArray[Tail];
	}

	// Returns the latest move
	FCubeMove& Newest()
	{
		//check(!IsEmpty());
		int32 Index = Head - 1;
		if (Index == -1)
		{
			Index = MoveArray.Num() - 1;
		}
		return MoveArray[Index];
	}

	// Determines if we have any saved moves or not
	bool IsEmpty() const
	{
		return Head == Tail;
	}

	// Finds next Array Index
	void Next(uint32& Index)
	{
		Index++;
		if (Index >= (uint32)MoveArray.Num())
		{
			Index -= MoveArray.Num();
		}
	}

	// Finds Previous Array Index
	void Previous(uint32& Index)
	{
		Index--;
		if (Index < 0)
		{
			Index += MoveArray.Num();
		}
	}

	// Operator Overload to extract a certain move
	FCubeMove& operator[](uint32 Index)
	{
		ensure(Index >= 0);
		ensure(Index < (uint32)MoveArray.Num());
		return MoveArray[Index];
	}

private:
	TArray<FCubeMove> MoveArray;
};

UCLASS()
class NTGAME_API ANTPawn : public APawn
{
	GENERATED_BODY()

public:
	FCubeMoveBuffer StoredMoves;
	FCubeMoveBuffer ImportantMoves;

	ANTPawn(const FObjectInitializer& ObjectInitializer);
	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* InputComponent) override;

	// Current / Previous Physics States
	FCubeState CurrentPhysState;
	FCubeState PreviousPhysState;

	/* If true, server will send important moves to get around Packet Loss */
	bool bUseImportantMoves;

	UPROPERTY()
	FCubeInput InputStates;

	UPROPERTY()
	FVector Accel; // Linear Acceleration
	UPROPERTY()
	FVector Alpha; // Angular Acceleration

	bool bReplayingMoves;

	float StartSmoothAlpha;
	float DefaultSmoothAlpha;
	float SmoothAlpha;

	UPROPERTY(EditDefaultsOnly, Category = "Network")
	uint32 MaxHistoryStates;

	void StartCorrection(const FCubeMove& TargetMove, const FCubeMove& SavedMove);

	// Called on Client when we recieve new move data from the Server
	UFUNCTION()
	void OnRep_ServerMoveData();

	UPROPERTY(ReplicatedUsing = "OnRep_ServerMoveData")
	FCubeMove ServerMoveData;

	void UpdateHistoryBuffer(int32 ForTime);

	void AddMoveToHistory(const FCubeMove& NewMove);
	void HistoryCorrection(ANTPawn* InActor, const FCubeMove& MoveData);

	void CalculateAccel(float DeltaSeconds, const FCubeInput& FromInput);
	void SmoothToState(const FCubeState& TargetState, float Alpha);
	void Snap(const FCubeState& NewState);
	void BeginSmoothing();

	// Updates Visual Elements
	void Interpolate(const FCubeState& FromState, const FCubeState& ToState, float Alpha = 1.f);

	virtual void OnRep_ReplicatedMovement() override;

	UFUNCTION(Server, Unreliable, WithValidation)
	void Server_SimulateInput(const FCubeInput& FromInput);
	virtual void Server_SimulateInput_Implementation(const FCubeInput& FromInput);
	virtual bool Server_SimulateInput_Validate(const FCubeInput& FromInput);

	void VisualizeMoveHistory();
	int32 GetTimeFromController(bool bNetworkTime);

protected:
	UFUNCTION(Client, Unreliable)
	void Client_SendCorrection(const FCubeMove& CorrectedMove);
	void Client_SendCorrection_Implementation(const FCubeMove& CorrectedMove);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ForceCube")
	float ForceStrength;

	// Components
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UBoxComponent* RootCollision;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RootMesh;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UCameraComponent* ViewCamera;
	
public:
	// Input from Keyboard
	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStartForward();
	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStopForward();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStartBackward();
	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStopBackward();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStartLeft();
	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStopLeft();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStartRight();
	UFUNCTION(BlueprintCallable, Category = "Input")
	void OnStopRight();
};