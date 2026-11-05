// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKControllerLayoutScreen.h"
#include "Widgets/ALKTitleText.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Settings/ALKSetting.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "HAL/PlatformInput.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKControllerLayoutScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKControllerLayoutScreen::UALKControllerLayoutScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

void UALKControllerLayoutScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	FALKWidgetBuilder::AddToOverlay(Root, Main);

	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"), MakeSystemLabel(10, LOCTEXT("ControllerLabel", "Controller")));
	FALKWidgetBuilder::AddToVerticalBox(Main, SystemLabelText, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Left);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("ControllerTitle", "Controller Layout"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);

	Diagram = FALKWidgetBuilder::Make<UALKGamepadDiagram>(Tree, TEXT("Diagram"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Diagram, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); })),
		FALKInputHint(FText::GetEmpty(), EKeys::Gamepad_FaceButton_Top, LOCTEXT("HintChangeBindings", "Change Bindings"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::OpenBindings))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Left);
}

void UALKControllerLayoutScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (WidgetTree)
	{
		StyleScreenBackground(Theme);
		if (UWidget* Main = WidgetTree->FindWidget(TEXT("MainColumn")))
		{
			if (UOverlaySlot* MainSlot = Cast<UOverlaySlot>(Main->Slot))
			{
				MainSlot->SetPadding(Theme.ScreenPadding);
			}
		}
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
	}
	FALKWidgetBuilder::StyleText(SystemLabelText, Theme, EALKTextRole::Data, Theme.Accent);
	if (SystemLabelText)
	{
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UALKControllerLayoutScreen::NativeOnActivated()
{
	RefreshLayout();
	Super::NativeOnActivated();
}

void UALKControllerLayoutScreen::RefreshLayout()
{
	if (Diagram)
	{
		Diagram->SetCallouts(GetCallouts());
	}
}

bool UALKControllerLayoutScreen::GetInputForKey(const FKey& Key, EALKGamepadInput& OutInput)
{
	struct FKeyInput
	{
		FKey Key;
		EALKGamepadInput Input;
	};
	static const FKeyInput Map[] = {
		{ EKeys::Gamepad_LeftTrigger, EALKGamepadInput::LeftTrigger },
		{ EKeys::Gamepad_LeftTriggerAxis, EALKGamepadInput::LeftTrigger },
		{ EKeys::Gamepad_LeftShoulder, EALKGamepadInput::LeftBumper },
		{ EKeys::Gamepad_Left2D, EALKGamepadInput::LeftStick },
		{ EKeys::Gamepad_LeftX, EALKGamepadInput::LeftStick },
		{ EKeys::Gamepad_LeftY, EALKGamepadInput::LeftStick },
		{ EKeys::Gamepad_LeftStick_Up, EALKGamepadInput::LeftStick },
		{ EKeys::Gamepad_LeftStick_Down, EALKGamepadInput::LeftStick },
		{ EKeys::Gamepad_LeftStick_Left, EALKGamepadInput::LeftStick },
		{ EKeys::Gamepad_LeftStick_Right, EALKGamepadInput::LeftStick },
		{ EKeys::Gamepad_LeftThumbstick, EALKGamepadInput::LeftStickPress },
		{ EKeys::Gamepad_Special_Left, EALKGamepadInput::View },
		{ EKeys::Gamepad_DPad_Up, EALKGamepadInput::DPad },
		{ EKeys::Gamepad_DPad_Down, EALKGamepadInput::DPad },
		{ EKeys::Gamepad_DPad_Left, EALKGamepadInput::DPad },
		{ EKeys::Gamepad_DPad_Right, EALKGamepadInput::DPad },
		{ EKeys::Gamepad_RightTrigger, EALKGamepadInput::RightTrigger },
		{ EKeys::Gamepad_RightTriggerAxis, EALKGamepadInput::RightTrigger },
		{ EKeys::Gamepad_RightShoulder, EALKGamepadInput::RightBumper },
		{ EKeys::Gamepad_Special_Right, EALKGamepadInput::Menu },
		{ EKeys::Gamepad_FaceButton_Top, EALKGamepadInput::FaceTop },
		{ EKeys::Gamepad_FaceButton_Right, EALKGamepadInput::FaceRight },
		{ EKeys::Gamepad_FaceButton_Left, EALKGamepadInput::FaceLeft },
		{ EKeys::Gamepad_FaceButton_Bottom, EALKGamepadInput::FaceBottom },
		{ EKeys::Gamepad_Right2D, EALKGamepadInput::RightStick },
		{ EKeys::Gamepad_RightX, EALKGamepadInput::RightStick },
		{ EKeys::Gamepad_RightY, EALKGamepadInput::RightStick },
		{ EKeys::Gamepad_RightStick_Up, EALKGamepadInput::RightStick },
		{ EKeys::Gamepad_RightStick_Down, EALKGamepadInput::RightStick },
		{ EKeys::Gamepad_RightStick_Left, EALKGamepadInput::RightStick },
		{ EKeys::Gamepad_RightStick_Right, EALKGamepadInput::RightStick },
		{ EKeys::Gamepad_RightThumbstick, EALKGamepadInput::RightStickPress },
	};
	for (const FKeyInput& Entry : Map)
	{
		if (Entry.Key == Key)
		{
			OutInput = Entry.Input;
			return true;
		}
	}
	return false;
}

TArray<FALKGamepadCallout> UALKControllerLayoutScreen::GetCallouts_Implementation() const
{
	// Every input can do several things (one per action bound to it); keep them in the order found.
	TArray<TPair<EALKGamepadInput, TArray<FText>>> Actions;
	auto AddAction = [&Actions](EALKGamepadInput Input, const FText& Name)
	{
		TPair<EALKGamepadInput, TArray<FText>>* Existing = Actions.FindByPredicate([Input](const TPair<EALKGamepadInput, TArray<FText>>& Pair) { return Pair.Key == Input; });
		if (!Existing)
		{
			Existing = &Actions.Emplace_GetRef(Input, TArray<FText>());
		}
		if (!Existing->Value.ContainsByPredicate([&Name](const FText& Other) { return Other.EqualTo(Name); }))
		{
			Existing->Value.Add(Name);
		}
	};
	// The d-pad shares one callout: each action says which direction it is on.
	auto DPadDirection = [](const FKey& Key) -> FText
	{
		if (Key == EKeys::Gamepad_DPad_Up) return LOCTEXT("DPadUp", "Up");
		if (Key == EKeys::Gamepad_DPad_Down) return LOCTEXT("DPadDown", "Down");
		if (Key == EKeys::Gamepad_DPad_Left) return LOCTEXT("DPadLeft", "Left");
		if (Key == EKeys::Gamepad_DPad_Right) return LOCTEXT("DPadRight", "Right");
		return FText::GetEmpty();
	};

	// The player's current gamepad bindings.
	if (const UEnhancedInputUserSettings* InputSettings = UALKSetting_KeyBinding::GetInputUserSettings(GetOwningLocalPlayer()))
	{
		if (const UEnhancedPlayerMappableKeyProfile* Profile = UALKSetting_KeyBinding::GetKeyProfile(InputSettings))
		{
			for (const TPair<FName, FKeyMappingRow>& Row : Profile->GetPlayerMappingRows())
			{
				for (const FPlayerKeyMapping& Mapping : Row.Value.Mappings)
				{
					const FKey Key = Mapping.GetCurrentKey();
					EALKGamepadInput Input;
					if (!Key.IsValid() || !Key.IsGamepadKey() || !GetInputForKey(Key, Input))
					{
						continue;
					}
					const FText Name = Mapping.GetDisplayName().IsEmpty() ? FText::FromName(Row.Key) : Mapping.GetDisplayName();
					const FText Direction = DPadDirection(Key);
					AddAction(Input, Direction.IsEmpty() ? Name : FText::Format(LOCTEXT("DPadAction", "{0}: {1}"), Direction, Name));
				}
			}
		}
	}

	// Inputs the rebindable actions do not cover.
	for (const TPair<FKey, FText>& Label : GetMenuConfig()->ControllerLayoutLabels)
	{
		EALKGamepadInput Input;
		if (!Label.Value.IsEmpty() && GetInputForKey(Label.Key, Input) && !Actions.ContainsByPredicate([Input](const TPair<EALKGamepadInput, TArray<FText>>& Pair) { return Pair.Key == Input; }))
		{
			AddAction(Input, Label.Value);
		}
	}

	TArray<FALKGamepadCallout> Callouts;
	for (const TPair<EALKGamepadInput, TArray<FText>>& Pair : Actions)
	{
		Callouts.Emplace(Pair.Key, FText::Join(INVTEXT(" / "), Pair.Value));
	}
	return Callouts;
}

void UALKControllerLayoutScreen::OpenBindings()
{
	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem())
	{
		MenuSubsystem->OpenSettingsPage(LOCTEXT("ControlsPageTitle", "Controls"), { EALKSettingsCategory::Controls });
	}
}

bool UALKControllerLayoutScreen::NativeHandleMenuKey(const FKey& Key)
{
	if (Key == EKeys::Gamepad_FaceButton_Top)
	{
		OpenBindings();
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
