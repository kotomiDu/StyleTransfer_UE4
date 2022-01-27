// Copyright Epic Games, Inc. All Rights Reserved.

#include "StyleTransferGameMode.h"
#include "StyleTransferCharacter.h"
#include "UObject/ConstructorHelpers.h"

AStyleTransferGameMode::AStyleTransferGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
