// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "HUD/ALKHUDVolumes.h"
#include "HUD/ALKHUDSubsystem.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Save/ALKSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/BoxComponent.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKHUDVolumes)

namespace ALKHUDVolumesPrivate
{
	static UALKHUDSubsystem* HUDOf(const APlayerController* PlayerController)
	{
		const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
		return LocalPlayer ? LocalPlayer->GetSubsystem<UALKHUDSubsystem>() : nullptr;
	}
}

// ---------------------------------------------------------------- Base

AALKHUDVolumeBase::AALKHUDVolumeBase()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetBoxExtent(FVector(150.0, 150.0, 120.0));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
	RootComponent = Box;
}

APlayerController* AALKHUDVolumeBase::GetLocalController(AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	return PlayerController && PlayerController->IsLocalController() ? PlayerController : nullptr;
}

void AALKHUDVolumeBase::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (APlayerController* PlayerController = GetLocalController(OtherActor))
	{
		OnPlayerEntered(PlayerController);
	}
}

void AALKHUDVolumeBase::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (APlayerController* PlayerController = GetLocalController(OtherActor))
	{
		OnPlayerLeft(PlayerController);
	}
}

// ---------------------------------------------------------------- Interaction volume

void AALKInteractionVolume::OnPlayerEntered(APlayerController* PlayerController)
{
	Player = PlayerController;
	if (!bBound)
	{
		// The volume takes the input itself while the player is inside: the Input Action if it has one, else the keys.
		EnableInput(PlayerController);
		UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
		if (Action && EnhancedInput)
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::HandleAction);
		}
		else if (InputComponent)
		{
			InputComponent->BindKey(KeyboardKey, IE_Pressed, this, &ThisClass::Interact);
			InputComponent->BindKey(GamepadKey, IE_Pressed, this, &ThisClass::Interact);
		}
		bBound = true;
	}
	else
	{
		EnableInput(PlayerController);
	}
	ShowPrompt();
}

void AALKInteractionVolume::OnPlayerLeft(APlayerController* PlayerController)
{
	if (UALKHUDSubsystem* HUD = ALKHUDVolumesPrivate::HUDOf(PlayerController))
	{
		HUD->HideInteractionPrompt();
	}
	DisableInput(PlayerController);
	Player.Reset();
}

void AALKInteractionVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UALKHUDSubsystem* HUD = ALKHUDVolumesPrivate::HUDOf(Player.Get()))
	{
		HUD->HideInteractionPrompt();
	}
	Super::EndPlay(EndPlayReason);
}

void AALKInteractionVolume::ShowPrompt()
{
	UALKHUDSubsystem* HUD = ALKHUDVolumesPrivate::HUDOf(Player.Get());
	if (!HUD || !bEnabled)
	{
		return;
	}
	FALKInteractionPrompt Prompt;
	Prompt.Action = Action;
	Prompt.KeyboardKey = KeyboardKey;
	Prompt.GamepadKey = GamepadKey;
	Prompt.ActionText = ActionText;
	Prompt.SubjectText = SubjectText;
	Prompt.Target = this;
	Prompt.WorldOffset = GetActorRotation().RotateVector(PromptOffset);
	HUD->ShowInteractionPrompt(Prompt);
}

void AALKInteractionVolume::SetInteractionEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;
	if (UALKHUDSubsystem* HUD = ALKHUDVolumesPrivate::HUDOf(Player.Get()))
	{
		if (bEnabled)
		{
			ShowPrompt();
		}
		else
		{
			HUD->HideInteractionPrompt();
		}
	}
}

void AALKInteractionVolume::HandleAction(const FInputActionInstance& Instance)
{
	Interact();
}

void AALKInteractionVolume::Interact()
{
	APlayerController* PlayerController = Player.Get();
	if (!PlayerController || !bEnabled)
	{
		return;
	}
	if (bOnlyOnce)
	{
		SetInteractionEnabled(false);
	}
	OnInteracted.Broadcast(PlayerController);
}

// ---------------------------------------------------------------- Subtitle trigger

void AALKSubtitleTrigger::OnPlayerEntered(APlayerController* PlayerController)
{
	if (!bPlayed || bRepeat)
	{
		PlayLines();
	}
}

void AALKSubtitleTrigger::PlayLines()
{
	bPlayed = true;
	NextLine = 0;
	GetWorldTimerManager().ClearTimer(NextLineTimer);
	PlayNext();
}

void AALKSubtitleTrigger::PlayNext()
{
	if (!Lines.IsValidIndex(NextLine))
	{
		return;
	}
	const FALKSubtitleLine& Line = Lines[NextLine++];
	const float Duration = Line.Duration > 0.f ? Line.Duration : FMath::Clamp(1.5f + Line.Line.ToString().Len() * 0.055f, 2.f, 9.f);
	if (UALKHUDSubsystem* HUD = ALKHUDVolumesPrivate::HUDOf(GetWorld()->GetFirstPlayerController()))
	{
		HUD->ShowSubtitle(Line.Speaker, Line.Line, Duration);
	}
	if (Lines.IsValidIndex(NextLine))
	{
		GetWorldTimerManager().SetTimer(NextLineTimer, this, &ThisClass::PlayNext, Duration + 0.25f, false);
	}
}

// ---------------------------------------------------------------- Checkpoint volume

void AALKCheckpointVolume::OnPlayerEntered(APlayerController* PlayerController)
{
	if (bReached)
	{
		return;
	}
	bReached = true;
	if (UALKHUDSubsystem* HUD = ALKHUDVolumesPrivate::HUDOf(PlayerController))
	{
		if (Title.IsEmpty() && Detail.IsEmpty())
		{
			HUD->ShowCheckpointNotification();
		}
		else
		{
			HUD->ShowNotification(Title, Detail, Icon);
			HUD->ShowSavingIndicator(1.5f);
		}
	}
	// Autosave (menu config: Autosave At Checkpoints). Set bAutosave off on a volume that should only show the notice.
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	UALKSaveSubsystem* Saves = PlayerController && PlayerController->GetGameInstance() ? PlayerController->GetGameInstance()->GetSubsystem<UALKSaveSubsystem>() : nullptr;
	if (bAutosave && Saves && Config->bEnableSaveSlots && Config->bAutosaveAtCheckpoints)
	{
		Saves->Autosave();
	}
	OnReached.Broadcast(PlayerController);
}
