// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Screens/ALKScreen.h"
#include "Settings/ALKSetting.h"
#include "Types/SlateEnums.h"
#include "ALKHostGameScreen.generated.h"

class UALKFrame;
class UALKSettingRow;
class UALKTextField;
class UALKThumbnail;
class UBorder;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWrapBox;

/** A slider setting that prints its value its own way: a match rule shows "10 min" or "Unlimited" instead of a bare number. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKSetting_RuleSlider : public UALKSetting_Slider
{
	GENERATED_BODY()

public:
	virtual FText GetValueText() const override { return ValueText ? ValueText() : Super::GetValueText(); }

	/** Gives the text to show for the current value. */
	TFunction<FText()> ValueText;
};

/**
 * The Host Game screen: everything a host decides before the lobby opens. On the left the settings (server name, game
 * mode, map, player limit, local network game, private game and password), the mode's match rules from the Online
 * Config, "Randomise rules" and the saved rule presets. On the right the map's picture, the mode and map, chips for the
 * main rules and what the focused setting does, with Create Lobby, Reset to Defaults and Back.
 *
 * Opened from a lobby with Open Host Game (Edit Current Game) it reads "Match Settings", starts from the running game's
 * settings and has Apply instead of Create Lobby.
 *
 * Designer widget names: BackgroundImage, SystemLabelText, TitleText, ListPanel, SettingsList, CardPanel, MapPicture,
 * ServerText, ModeText, MapText, ChipBox, DescriptionTitle, DescriptionText, ActionColumn, ConfirmButton, ResetButton,
 * BackButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKHostGameScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKHostGameScreen(const FObjectInitializer& ObjectInitializer);

	/** Create Lobby: hosts a game with the settings on screen. While editing a lobby: sends them to the server and closes. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Game")
	void ConfirmSettings();

	/** Puts every setting back to the Online Config's defaults. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Game")
	void ResetToDefaults();

	/** Gives every match rule a random value. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Game")
	void RandomizeRules();

	/** Asks for a name and saves the rules on screen as a preset. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Game")
	void SaveRulesAsPreset();

	/** Deletes the preset that is picked in the Preset row, after asking. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Game")
	void DeleteSelectedPreset();

	/** Applies a saved rule preset by name (and its game mode, when it was saved for one). False when there is none. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Game")
	bool ApplyRulePreset(const FString& PresetName);

	/** The settings as they are on screen now. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Host Game")
	const FALKHostSettings& GetHostSettings() const { return HostSettings; }

	/** Replaces the settings on screen. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Host Game")
	void SetHostSettings(const FALKHostSettings& InSettings);

	/** True when the screen edits the lobby that is open instead of creating a game. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Host Game")
	bool IsEditingCurrentGame() const { return bEditMode; }

	/** How many match rules get a chip on the card at the right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Lobby|Host Game", meta = (ClampMin = "0", ClampMax = "12"))
	int32 MaxRuleChips = 6;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual void NativeOnInputMethodChanged(bool bUsingGamepad) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ListPanel;

	/** The setting rows go in here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> SettingsList;

	/** The card at the right: picture, mode, map, rule chips and the focused setting's description. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> CardPanel;

	/** The map's thumbnail, or generated art when it has none. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UALKThumbnail> MapPicture;

	/** The server name, in small capitals over the mode. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ServerText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeText;

	/** "Arena - 8 players". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapText;

	/** Holds a chip per main rule. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> ChipBox;

	/** The focused setting's name. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionTitle;

	/** What the focused setting does. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	/** Holds the card and the buttons under it. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> ActionColumn;

	/** "Create lobby", or "Apply" while editing a lobby. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ResetButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Host Game", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> BackButton;

private:
	/** A row with a text box (server name, password, a text rule): built here, as setting rows have no text kind. */
	struct FTextRow
	{
		FName Id;
		FText DisplayName;
		FText Description;
		TWeakObjectPtr<UWidget> RowRoot;
		TWeakObjectPtr<UTextBlock> NameText;
		TWeakObjectPtr<UALKTextField> Field;
		TFunction<FString()> Getter;
		TFunction<void(const FString&)> Setter;
		/** Unset: always shown. The password row is only there for a private game. */
		TFunction<bool()> IsShown;
	};

	void LoadStartSettings();
	void RebuildRows();
	void RefreshRows();
	void UpdateCard();
	void UpdateTitles();
	void UpdateDescription(const FText& Name, const FText& Description);
	void StyleTextRow(const FTextRow& TextRow, const UALKTheme& Theme, bool bTyping) const;
	void StyleSectionHeader(UTextBlock* Header, const UALKTheme& Theme) const;
	/** Makes the rows again on the next frame. Focus After names the row that takes the focus then (None: the focused one). */
	void RequestRebuild(FName FocusAfter = NAME_None);
	void ChangeMode(FName NewModeId);
	FName GetFocusedRowId() const;
	UWidget* FindRowWidget(FName Id) const;
	FText GetSettingDescription(const UALKSetting* Setting) const;
	FString GetLocalPlayerName() const;
	FString GetFallbackServerName() const;
	FALKHostSettings MakeFinalSettings() const;

	void HandleAnySettingChanged(UALKSetting* Setting);
	void HandleRowFocused(UALKSettingRow* Row, bool bFromNavigation);
	void HandleTextRowChanged(const FText& Text, int32 Index);
	void HandleListsChanged();
	void HandleBackClicked();

	/** Down past the last row goes to the buttons; Left and Up from the buttons come back to the rows. */
	UFUNCTION()
	UWidget* HandleNavigateToActions(EUINavigation Direction);

	UFUNCTION()
	UWidget* HandleNavigateToRows(EUINavigation Direction);

	/** The settings behind the rows. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKSetting>> AllSettings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKSettingRow>> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SectionHeaders;

	/** The chips on the card, to recolour them with the theme. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKFrame>> Chips;

	/** The footer hints only a gamepad needs (the buttons at the right do the same for the mouse). */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> PadHints;

	/** The "Create lobby" / "Apply" hint among them. */
	UPROPERTY(Transient)
	TObjectPtr<UALKButton> ConfirmHint;

	TArray<FTextRow> TextRows;

	FALKHostSettings HostSettings;
	/** The preset shown in the Preset row; cleared as soon as a rule is changed by hand. */
	FString SelectedPreset;
	/** The row that takes the focus once the rows are made again. */
	FName RefocusId;
	/** The row that had the focus last, to come back to from the buttons. */
	FName LastRowId;
	/** The text row that is being typed in, or -1. */
	int32 TypingRow = INDEX_NONE;
	FDelegateHandle SettingChangedHandle;
	FDelegateHandle ListsChangedHandle;
	bool bEditMode = false;
	bool bFreshOpen = true;
	bool bRebuildPending = false;
	bool bFocusPending = false;
	bool bUpdatingTextRows = false;
};
