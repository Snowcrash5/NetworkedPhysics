// Fill out your copyright notice in the Description page of Project Settings.

#include "NTGame.h"
#include "NTPlayerState.h"
#include "NTPlayerController.h"

ANTPlayerController::ANTPlayerController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// Latency
	T_ClientSentRequest = 0;
	T_ServerOffsetTime = 0;

	bHasValidTimestamp = false;

	PredictionFudgeFactor = 15.0f;
	MaxPredictionPing = 0.f;
	DesiredPredictionPing = 0.f;
	ServerMaxPredictionPing = 160.0f;
}

void ANTPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (Role < ROLE_Authority)
	{
		ServerNegotiatePredictionPing(DesiredPredictionPing);
	}
}

void ANTPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AccumulativeDeltaTime += DeltaSeconds;

	// If we're the Client and we haven't updated Ping in a while, try to get a ping update
	const float LocTimeSec = GetWorld()->GetTimeSeconds();
	if (((LocTimeSec - LastPingCalcTime) > 0.5f) && (GetNetMode() == NM_Client))
	{
		LastPingCalcTime = LocTimeSec;
		ServerBouncePing(LocTimeSec);
	}

	if (PlayerState)
	{
		if (Role == ROLE_Authority && !IsLocalController())
		{
			GEngine->AddOnScreenDebugMessage(-1, DeltaSeconds, FColor::Blue, FString::Printf(TEXT("Server Ping %f - TimeStamp %i"), PlayerState->ExactPing, GetLocalTime()));
		}
		else if (Role < ROLE_Authority)
		{
			GEngine->AddOnScreenDebugMessage(-1, DeltaSeconds, FColor::Green, FString::Printf(TEXT("Client Ping %f - TimeStamp %i"), PlayerState->ExactPing, GetNetworkTime()));
		}
	}
}

//////////////////////////////////
///// EXACT PING CALCULATION /////
//////////////////////////////////

void ANTPlayerController::ServerBouncePing_Implementation(float TimeStamp)
{
	ClientReturnPing(TimeStamp);
}

void ANTPlayerController::ClientReturnPing_Implementation(float TimeStamp)
{
	ANTPlayerState* NTPS = Cast<ANTPlayerState>(PlayerState);
	if (NTPS)
	{
		NTPS->CalculatePing(GetWorld()->GetTimeSeconds() - TimeStamp);
	}
}

void ANTPlayerController::ServerUpdatePing_Implementation(float ExactPing)
{
	if (PlayerState)
	{
		PlayerState->ExactPing = ExactPing;
		PlayerState->Ping = FMath::Min(255, (int32)(ExactPing * 0.25f));
	}
}

///////////////////////////////////////
///// PREDICTION PING CALCULATION /////
///////////////////////////////////////

void ANTPlayerController::ServerNegotiatePredictionPing_Implementation(float NewPredictionPing)
{
	MaxPredictionPing = FMath::Clamp(NewPredictionPing, 0.f, ServerMaxPredictionPing);
}

float ANTPlayerController::GetPredictionTime()
{
	return (PlayerState && (GetNetMode() != NM_Standalone)) ? (0.0005f * FMath::Clamp(PlayerState->ExactPing - PredictionFudgeFactor, 0.f, MaxPredictionPing)) : 0.f;
}

void ANTPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANTPlayerController, MaxPredictionPing, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ANTPlayerController, PredictionFudgeFactor, COND_OwnerOnly);
}

////////////////////////////////////
///// OLD TIME STAMP FUNCTIONS /////
////////////////////////////////////

int32 ANTPlayerController::GetLocalTime()
{
	return FMath::FloorToInt(AccumulativeDeltaTime * 1000.0f);
}

int32 ANTPlayerController::GetNetworkTime()
{
	return GetLocalTime() + T_ServerOffsetTime;
}

void ANTPlayerController::ClientRequestNewTimeStamp()
{
	if (Role < ROLE_Authority)
	{
		bHasValidTimestamp = false;
		T_ClientSentRequest = GetLocalTime();
		Server_ClientRequestedTimeStamp();
	}
}

void ANTPlayerController::Server_ClientRequestedTimeStamp_Implementation()
{
	// Send back with Stamp
	Client_ServerSentTimeStamp(GetLocalTime());
}

bool ANTPlayerController::Server_ClientRequestedTimeStamp_Validate()
{
	return true;
}

void ANTPlayerController::Client_ServerSentTimeStamp_Implementation(int32 ServerTimeStamp)
{
	// Calculate Servers System Time at the moment we actually sent the request for it.
	int32 RTT = GetLocalTime() - T_ClientSentRequest;
	
	// Difference between both values
	T_ServerOffsetTime = (ServerTimeStamp - (RTT / 2)) - T_ClientSentRequest;

	bHasValidTimestamp = true;
}