// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKMenuListScreen.generated.h"

class UPanelWidget;
class UImage;
class UTextBlock;
class UALKButton;

/**
 * A screen that shows a vertical list of menu entries (main menu, settings menu, pause menu), the focused one lit in a
 * rounded frame, under the screen's title.
 * Designer layouts provide a panel named EntriesBox (a Vertical Box works best); buttons are generated into it. The
 * code generated layout also names BackgroundImage, SystemLabelText, LogoImage, TitleText, SubtitleText and FooterNote.
 */
UCLASS(Abstract, Blueprintable)
class ASSETPROLOBBYKIT_API UALKMenuListScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKMenuListScreen(const FObjectInitializer& ObjectInitializer);

	/** Recreates the entry buttons from Get Menu Entries. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Menu")
	void RebuildEntries();

	/** The entries to show. Defaults to the menu config; override to add, remove or disable entries at runtime. */
	UFUNCTION(BlueprintNativeEvent, Category = "Assetpro Menu|Menu")
	TArray<FALKMenuEntry> GetMenuEntries() const;

	/** Runs when an entry is clicked. The default runs the entry's action through the menu subsystem. */
	UFUNCTION(BlueprintNativeEvent, Category = "Assetpro Menu|Menu")
	void OnEntryClicked(const FALKMenuEntry& Entry);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Menu")
	UALKButton* GetEntryButton(FName EntryId) const;

	/**
	 * Turns each entry button on or off again (Required Save Slot, Set Menu Entry Enabled) and makes the first enabled
	 * entry the one that gets focus. Runs whenever the screen is shown.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Menu")
	void RefreshEntryStates();

protected:
	virtual void NativeOnLayoutReady() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;

	virtual TArray<FALKMenuEntry> NativeGetConfigEntries() const { return TArray<FALKMenuEntry>(); }

	/** Horizontal alignment of the entry buttons inside EntriesBox. */
	virtual EHorizontalAlignment GetEntryAlignment(const UALKTheme& Theme) const;

	/**
	 * Builds the layout the list screens share: the atmosphere (the theme's scene, or only its fog, dust, grain and
	 * vignette over a blurred game with bOverGame), a column with the logo or title, a subtitle and the entries, and a
	 * footer with Hints and a note on the other side.
	 */
	void BuildListLayout(UWidgetTree& Tree, bool bOverGame, const TArray<FALKInputHint>& Hints);

	/** Styles the shared layout: fonts, the column's alignment and padding, the background (Opacity fades its scene). */
	void StyleListLayout(const UALKTheme& Theme, float BackgroundOpacity = 1.f);

	/**
	 * Fills the title (or the logo instead when Logo has a picture), the subtitle and the footer note; empty ones hide.
	 * The small label over them shows the screen's System Label, when you set one.
	 */
	void SetListTexts(const FText& Title, const FText& Subtitle, const FText& Note, const FSlateBrush* Logo = nullptr, const FText& LabelName = FText::GetEmpty());

	/**
	 * Shows the shared layout's content (the column or panel with the title and the entries, the header and the footer)
	 * at Opacity, leaving the background as it is: the pause menu steps out with it while another screen covers it.
	 */
	void SetListOpacity(float Opacity);

	/** Kept for screens that number themselves in their own labels; the built-in screens do not show it. */
	int32 SystemCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> EntriesBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	/** Shown instead of the title when the screen has a logo (the menu config's Game Logo on the main menu). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UImage> LogoImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The optional small label at the top (see System Label). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	/** Small text on the far side of the footer (the version on the main menu). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FooterNote;

	/** Title style of the list: the big Title font on the main menu, Heading on the others. */
	EALKTextRole TitleRole = EALKTextRole::Heading;


	UPROPERTY(Transient, BlueprintReadOnly, Category = "Assetpro Menu|Menu")
	TArray<TObjectPtr<UALKButton>> EntryButtons;

	TArray<FALKMenuEntry> DisplayedEntries;

private:
	void HandleEntryClicked(int32 EntryIndex);
	void LayoutEntryButtons(const UALKTheme& Theme);

	UPROPERTY(Transient)
	TObjectPtr<UWidget> ContentColumn;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Footer;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> Header;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UALKRule>> Rules;

	UPROPERTY(Transient)
	TObjectPtr<class UALKRule> TitleRule;

	UPROPERTY(Transient)
	TObjectPtr<class UALKFrame> Panel;

	FDelegateHandle EntriesChangedHandle;
};
