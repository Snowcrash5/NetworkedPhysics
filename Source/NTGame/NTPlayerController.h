// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/PlayerController.h"
#include "NTPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class NTGAME_API ANTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ANTPlayerController(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	float AccumulativeDeltaTime;

	// --- TIMESTAMP / PING FUNCTIONALITY ----------------------------------------------
	UPROPERTY()
	float LastPingCalcTime;

	/* Client sends ping request to server */
	UFUNCTION(Unreliable, Server, WithValidation)
	virtual void ServerBouncePing(float TimeStamp);
	virtual void ServerBouncePing_Implementation(float TimeStamp);
	virtual bool ServerBouncePing_Validate(float TimeStamp) { return true; }

	/* Server bounces ping request back to client, with it's own local timestamp */
	UFUNCTION(Unreliable, Client)
	virtual void ClientReturnPing(float TimeStamp);
	virtual void ClientReturnPing_Implementation(float TimeStamp);

	/* Client informs server of new ping update */
	UFUNCTION(Unreliable, Server, WithValidation)
	virtual void ServerUpdatePing(float ExactPing);
	virtual void ServerUpdatePing_Implementation(float ExactPing);
	virtual bool ServerUpdatePing_Validate(float ExactPing) { return true; }

	// --- NETWORK PREDICTION / TIMESTAMP SYNCHRONIZATION

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Network")
	float PredictionFudgeFactor;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Network")
	float MaxPredictionPing;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Network")
	float ServerMaxPredictionPing;

	UPROPERTY(EditDefaultsOnly, Replicated, Category = "Network")
	float DesiredPredictionPing;

	/* Propose a desired Ping to the Server */
	UFUNCTION(Reliable, Server, WithValidation)
	virtual void ServerNegotiatePredictionPing(float NewPredictionPing);
	virtual void ServerNegotiatePredictionPing_Implementation(float NewPredictionPing);
	virtual bool ServerNegotiatePredictionPing_Validate(float NewPredictionPing) { return true; }

	/* Return amount of time to tick to make up for network latency */
	virtual float GetPredictionTime();

	// --- END TIMESTAMP FUNCTIONALITY ---------------------------------------------------


	// --- TIMESTAMP FUNCTIONALITY -------------------------------------------------------
	/* TimeStamp Vars. int32 Gives a Maximum of 596 Hours of Play-Time */
	int32 T_ClientSentRequest; // Client System time when it asked for Server System Time.
	int32 T_ServerOffsetTime; // How far behind / ahead of the server we are.

	/* True when we have received a valid timestamp. Invalidated when we request a new one */
	bool bHasValidTimestamp;

	/* Get System Time in MS as int32 */
	int32 GetLocalTime();
	/* Get Current Time on the Server */
	int32 GetNetworkTime();
	/* Invalidate Client TimeStamp and Resend the Request */
	void ClientRequestNewTimeStamp();

	/* Request that the server tell us to calculate time offset */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ClientRequestedTimeStamp();
	virtual void Server_ClientRequestedTimeStamp_Implementation();
	virtual bool Server_ClientRequestedTimeStamp_Validate();

	/* Called by Server on Client. We can use the time the function was called to work out what the latency is. */
	UFUNCTION(Client, Reliable)
	void Client_ServerSentTimeStamp(int32 ServerTimeStamp);
	virtual void Client_ServerSentTimeStamp_Implementation(int32 ServerTimeStamp);
};