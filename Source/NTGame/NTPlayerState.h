// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/PlayerState.h"
#include "NTPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class NTGAME_API ANTPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	// --- Ping Calculation -----
	/* Override Engine-Style Ping Calculation */
	virtual void UpdatePing(float InPing) override;
	/* Called on client using the Round-Trip ping Time */
	virtual void CalculatePing(float NewPing);
};
