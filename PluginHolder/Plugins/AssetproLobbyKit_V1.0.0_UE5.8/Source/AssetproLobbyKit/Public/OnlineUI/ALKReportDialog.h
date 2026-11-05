// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineConfig.h"
#include "Screens/ALKScreen.h"
#include "ALKReportDialog.generated.h"

class UALKButton;
class UALKIconWidget;
class UALKTextField;
class UBorder;
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * Reports a player: pick one reason (the Online Config's Report Reasons), add a line of details if you like, choose
 * whether to mute the player as well, and submit. The kit only raises the online subsystem's On Player Reported event
 * (and tells the provider): send the report to your own moderation tools from there.
 *
 * Open it with Show Report on the Online Flow subsystem; it reads the player from the flow when it opens.
 *
 * Designer widget names: DimImage, Panel, TitleText, NoteText, FormBox (Vertical Box), ReasonLabel, ReasonGrid
 * (Vertical Box), DetailsField, MuteButton, SubmitButton, CancelButton, SentBox (Vertical Box), SentIcon, SentText,
 * SentDetailText, DoneButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKReportDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKReportDialog(const FObjectInitializer& ObjectInitializer);

	/** Picks a reason by its place in the list. Only one reason is picked at a time. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Report")
	void SelectReason(int32 ReasonIndex);

	/** Turns "also mute this player" on or off. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Report")
	void ToggleAlsoMute();

	/** Sends the report (a reason must be picked) and shows the "report sent" state. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Report")
	void SubmitReport();

	/** The user id of the player being reported. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Report")
	const FString& GetReportedUserId() const { return UserId; }

	/** The longest details text, in characters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Report", meta = (ClampMin = "0"))
	int32 MaxDetailsLength = 200;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	/** "Report <name>". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** Where reports go. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NoteText;

	/** Everything of the form; hidden once the report is sent. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> FormBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReasonLabel;

	/** The reason buttons are added here, two to a line. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ReasonGrid;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UALKTextField> DetailsField;

	/** "Also mute this player". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> MuteButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> SubmitButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CancelButton;

	/** Shown once the report is sent. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> SentBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> SentIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SentText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SentDetailText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Report", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> DoneButton;

private:
	void RebuildReasons();
	void RefreshState();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> ReasonButtons;

	TArray<FALKReportReason> Reasons;
	FString UserId;
	FString ShownName;
	int32 SelectedReason = INDEX_NONE;
	bool bAlsoMute = true;
	bool bSent = false;
};
