// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Screens/ALKScreen.h"
#include "ALKProfileDialog.generated.h"

class UALKAvatar;
class UALKButton;
class UALKFrame;
class UALKIconWidget;
class UALKSpinner;
class UALKValueBar;
class UBorder;
class UImage;
class UTextBlock;
class UVerticalBox;

/**
 * A player's profile card, as a dialog: picture, name, what they are doing, level and progress, and the stats the
 * online provider gives (name over value), with buttons to invite the player to the party, mute them or report them.
 *
 * Open it with Show Profile on the Online Flow subsystem; it reads the player from the flow when it opens. Opened for
 * nobody it shows the local player's own card.
 *
 * Designer widget names: DimImage, Panel, Avatar, NameText, PlatformIcon, PresenceText, LevelChip, LevelBar,
 * LevelText, StatGrid (Vertical Box), Spinner, StatusText, InviteButton, MuteButton, ReportButton, CloseButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKProfileDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKProfileDialog(const FObjectInitializer& ObjectInitializer);

	/** Shows another player's card in the open dialog. An empty user id shows the local player. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Profile")
	void ShowPlayer(const FString& InUserId, const FString& InDisplayName);

	/** Invites the shown player to the local player's party. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Profile")
	void InviteToParty();

	/** Mutes the shown player, or lets them be heard again. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Profile")
	void ToggleMute();

	/** Opens the report dialog for the shown player. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Profile")
	void ReportPlayer();

	/** The user id of the player whose card is shown. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Profile")
	const FString& GetShownUserId() const { return UserId; }

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKAvatar> Avatar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	/** The generic platform badge in front of the presence line. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> PlatformIcon;

	/** "Online", "In a game", "In your game"... */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PresenceText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> LevelChip;

	/** Progress to the next level. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKValueBar> LevelBar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	/** The stat tiles are added here, three to a line. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> StatGrid;

	/** Turns until the profile is there. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKSpinner> Spinner;

	/** "No profile available". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> InviteButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> MuteButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ReportButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Profile", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CloseButton;

private:
	void StartLoading();
	bool TryLoadProfile();
	void RefreshCard();
	void RebuildStats(const UALKTheme& Theme);
	void RefreshButtons();
	FString ResolveShownName() const;
	bool IsLocalUser() const;

	FString UserId;
	FString ContextName;
	FALKPlayerProfile Profile;
	bool bHasProfile = false;
	bool bLoading = false;
	float LoadTime = 0.f;
	float RetryTime = 0.f;
};
