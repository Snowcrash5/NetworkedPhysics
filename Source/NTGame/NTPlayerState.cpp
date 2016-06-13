// Fill out your copyright notice in the Description page of Project Settings.

#include "NTGame.h"
#include "NTPlayerController.h"
#include "NTPlayerState.h"

void ANTPlayerState::UpdatePing(float InPing)
{
	// Do Nothing - We don't want to do Engine-Based Pinging!
}

void ANTPlayerState::CalculatePing(float NewPing)
{
	if (NewPing < 0.f)
	{
		// Caused by TimeStamp Wraparound
		return;
	}

	float OldPing = ExactPing;
	Super::UpdatePing(NewPing);

	ANTPlayerController* PC = Cast<ANTPlayerController>(GetOwner());
	if (PC)
	{
		PC->LastPingCalcTime = GetWorld()->GetTimeSeconds();
		if (ExactPing != OldPing)
		{
			PC->ServerUpdatePing(ExactPing); // Tell the Server to Update our Ping
			PC->ClientRequestNewTimeStamp(); // Update the Clients' Local TimeStamp
		}
	}
}