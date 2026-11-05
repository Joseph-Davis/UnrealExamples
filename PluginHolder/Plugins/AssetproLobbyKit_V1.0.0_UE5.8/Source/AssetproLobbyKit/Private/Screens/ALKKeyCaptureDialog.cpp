// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKKeyCaptureDialog.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "ALKTheme.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKKeyCaptureDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKKeyCaptureDialog::UALKKeyCaptureDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
	bAnimateIn = true;
}

void UALKKeyCaptureDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	Panel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Panel->SetPadding(FMargin(44.f, 54.f, 44.f, 38.f));
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Panel, 560.f, 0.f), HAlign_Center, VAlign_Center);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Panel->SetContent(Column);

	TitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("TitleText"));
	TitleText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(), HAlign_Center);

	UALKTitleText* Prompt = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("PromptText"));
	Prompt->SetText(LOCTEXT("PressAKey", "Press a key"));
	PromptText = Prompt;
	PromptText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, PromptText, FMargin(0.f, 26.f, 0.f, 0.f), HAlign_Center);

	MessageText = FALKWidgetBuilder::MakeText(Tree, TEXT("MessageText"));
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, MessageText, FMargin(0.f, 26.f, 0.f, 0.f), HAlign_Center);
}

void UALKKeyCaptureDialog::SetupCapture(const FText& InActionName, bool bInGamepad, TFunction<void(FKey)> InOnKeyCaptured)
{
	bGamepad = bInGamepad;
	OnKeyCaptured = MoveTemp(InOnKeyCaptured);

	if (TitleText)
	{
		TitleText->SetText(InActionName);
	}
	if (PromptText)
	{
		PromptText->SetText(bGamepad ? LOCTEXT("PressAButton", "Press a button") : LOCTEXT("PressAKey", "Press a key"));
	}
	if (MessageText)
	{
		MessageText->SetText(bGamepad
			? LOCTEXT("CaptureHintPad", "Press B / Circle to cancel.")
			: LOCTEXT("CaptureHintKeyboard", "Press any key or mouse button. Press Escape to cancel."));
	}
}

void UALKKeyCaptureDialog::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// Like the confirm dialog: centred text over the dimmed screen, no panel.
	if (DimImage)
	{
		DimImage->SetBrush(UALKTheme::MakeSolidBrush(Theme.ScreenDim));
	}
	if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
	{
		FALKFrameStyle Style = Theme.MakeDialogFrame();
		Style.Ring = Theme.Accent;
		Style.RingWidth = Theme.FocusRingWidth;
		Frame->SetFrameStyle(Style);
	}
	else if (Panel)
	{
		Panel->SetBrush(UALKTheme::MakeSolidBrush(FLinearColor::Transparent));
	}
	// The accent as it reads as text on the panel (darker on a light skin, where the accent itself is too pale).
	FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	FALKWidgetBuilder::StyleText(PromptText, Theme, EALKTextRole::Heading, Theme.GetAccentTextColor());
	FALKWidgetBuilder::StyleText(MessageText, Theme, EALKTextRole::Caption);
}

void UALKKeyCaptureDialog::NativeOnActivated()
{
	Super::NativeOnActivated();
	// Ignore input for a moment so the key that opened the dialog is not captured.
	ArmTime = 0.2f;
	PulseTime = 0.f;
}

void UALKKeyCaptureDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	ArmTime = FMath::Max(0.f, ArmTime - InDeltaTime);
	PulseTime += InDeltaTime;
	if (PromptText)
	{
		PromptText->SetRenderOpacity(0.55f + 0.45f * (0.5f + 0.5f * FMath::Cos(PulseTime * 4.f)));
	}
}

bool UALKKeyCaptureDialog::TryCaptureKey(const FKey& Key)
{
	if (ArmTime > 0.f)
	{
		return true;
	}

	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right || Key == FPlatformInput::GetGamepadBackKey())
	{
		Finish(EKeys::Invalid);
		return true;
	}

	// Only accept keys of the device this binding is for. Analog axes are never bound as buttons here.
	if (Key.IsGamepadKey() != bGamepad || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D() || Key.IsTouch())
	{
		return true;
	}

	Finish(Key);
	return true;
}

bool UALKKeyCaptureDialog::NativeHandleMenuKey(const FKey& Key)
{
	return TryCaptureKey(Key);
}

FReply UALKKeyCaptureDialog::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// CommonUI turns gamepad accept into a fake left click; never bind that as the mouse button.
	const UCommonInputSubsystem* InputSubsystem = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
	const bool bSimulatedClick = InputSubsystem && InputSubsystem->GetIsGamepadSimulatedClick();
	if (bSimulatedClick)
	{
		// The simulated click *is* the gamepad accept button, which CommonUI consumed before we saw the key.
		if (bGamepad)
		{
			TryCaptureKey(FPlatformInput::GetGamepadAcceptKey());
		}
	}
	else if (!bGamepad)
	{
		TryCaptureKey(InMouseEvent.GetEffectingButton());
	}
	return FReply::Handled();
}

FReply UALKKeyCaptureDialog::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bGamepad && ArmTime <= 0.f)
	{
		Finish(InMouseEvent.GetWheelDelta() > 0.f ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
	}
	return FReply::Handled();
}

bool UALKKeyCaptureDialog::OnBackRequested_Implementation()
{
	Finish(EKeys::Invalid);
	return true;
}

void UALKKeyCaptureDialog::Finish(const FKey& Key)
{
	TFunction<void(FKey)> Callback = MoveTemp(OnKeyCaptured);
	OnKeyCaptured = nullptr;

	PlayThemeSound(Key.IsValid() ? EALKUISound::Click : EALKUISound::Back);
	CloseScreen();
	if (Callback)
	{
		Callback(Key);
	}
}

#undef LOCTEXT_NAMESPACE
