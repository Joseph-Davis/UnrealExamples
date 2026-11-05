// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Game/ALKPlayerController.h"
#include "ALKMenuSubsystem.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Save/ALKSaveSubsystem.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPlayerController)

AALKPlayerController::AALKPlayerController()
{
}

AALKMainMenuPlayerController::AALKMainMenuPlayerController()
{
	bShowMainMenuOnBeginPlay = true;
	bEnablePauseMenuInput = false;
}

void AALKPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UALKSettingsSubsystem* Settings = GameInstance->GetSubsystem<UALKSettingsSubsystem>())
		{
			Settings->ApplyToPlayerController(this);
		}
	}

	if (bShowMainMenuOnBeginPlay)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UALKMenuSubsystem* Menu = LocalPlayer->GetSubsystem<UALKMenuSubsystem>())
			{
				Menu->ShowStartupMenu();
			}
		}
	}
}

void AALKPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent && bEnablePauseMenuInput)
	{
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::TogglePauseMenu).bExecuteWhenPaused = true;
		InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &ThisClass::TogglePauseMenu).bExecuteWhenPaused = true;
	}

	// Quick save and quick load (menu config: Quick Save Key, Quick Load Key), in gameplay levels only.
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	if (InputComponent && bEnableQuickSaveInput && bEnablePauseMenuInput && Config->bEnableSaveSlots)
	{
		// Development builds bind F5 (shader complexity) and F9 (screenshot) as engine debug keys: free the keys used here.
		FreeDebugKeys({ Config->QuickSaveKey, Config->QuickLoadKey });
		if (Config->QuickSaveKey.IsValid())
		{
			InputComponent->BindKey(Config->QuickSaveKey, IE_Pressed, this, &ThisClass::QuickSave);
		}
		if (Config->QuickLoadKey.IsValid())
		{
			InputComponent->BindKey(Config->QuickLoadKey, IE_Pressed, this, &ThisClass::QuickLoad);
		}
	}
}

void AALKPlayerController::FreeDebugKeys(const TArray<FKey>& Keys)
{
	if (!PlayerInput)
	{
		return;
	}
	PlayerInput->DebugExecBindings.RemoveAll([&Keys](const FKeyBind& Bind)
	{
		return Keys.ContainsByPredicate([&Bind](const FKey& Key) { return Key.IsValid() && Bind.Key == Key; });
	});
}

void AALKPlayerController::QuickSave()
{
	UALKMenuSubsystem* Menu = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UALKMenuSubsystem>() : nullptr;
	UALKSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UALKSaveSubsystem>() : nullptr;
	if (Saves && !(Menu && Menu->IsAnyMenuOpen()))
	{
		Saves->QuickSave();
	}
}

void AALKPlayerController::QuickLoad()
{
	UALKMenuSubsystem* Menu = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UALKMenuSubsystem>() : nullptr;
	UALKSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UALKSaveSubsystem>() : nullptr;
	if (Saves && !(Menu && Menu->IsAnyMenuOpen()))
	{
		Saves->QuickLoad();
	}
}

void AALKPlayerController::TogglePauseMenu()
{
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UALKMenuSubsystem* Menu = LocalPlayer->GetSubsystem<UALKMenuSubsystem>())
		{
			Menu->TogglePauseMenu();
		}
	}
}
