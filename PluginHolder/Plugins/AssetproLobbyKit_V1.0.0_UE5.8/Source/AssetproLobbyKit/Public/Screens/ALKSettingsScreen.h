// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "Settings/ALKSetting.h"
#include "ALKSettingsScreen.generated.h"

class UImage;
class UTextBlock;
class UBorder;
class UPanelWidget;
class UScrollBox;
class UALKButton;
class UALKSettingRow;
class UALKSettingsSubsystem;

/**
 * Settings screen. Shown as one page from the settings menu (Open Settings Page: a title and the settings of a few
 * categories in one list), or with a tab per category (Open Settings Tabs). Rows are generated per setting type using the
 * row classes set in the menu config. Left / Right change values, Y resets, Q / E or LB / RB switch tabs.
 * Leaving with changes that were not applied asks whether to apply them.
 *
 * Designer widget names: BackgroundImage, TitleText, TabRow, TabBox, SettingsList (Scroll Box), DescriptionPanel,
 * DescriptionTitle, DescriptionText, ApplyButton, ResetButton, BackButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSettingsScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKSettingsScreen(const FObjectInitializer& ObjectInitializer);

	/**
	 * Shows one page instead of tabs: Title on top and the settings of Categories, in that order, in one list. Call it
	 * before the screen is shown (Open Settings Page does). Empty Categories shows every category.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void SetPage(const FText& Title, const TArray<EALKSettingsCategory>& Categories);

	/** Shows a tab for every settings category (the default; undoes Set Page). Open Settings calls it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void SetTabbed();

	/** True when the screen shows a single page (Set Page) rather than tabs. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsPage() const { return bPageMode; }

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void SetActiveTab(int32 TabIndex);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void SetActiveCategory(EALKSettingsCategory Category);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	int32 GetActiveTabIndex() const { return ActiveTab; }

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	int32 GetNumTabs() const { return TabCategories.Num(); }

	/** Applies and saves all changes. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void ApplyChanges();

	/** Asks for confirmation, then resets every setting on the current tab (or the whole page). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void ResetActiveTabToDefaults();

	/** Closes the screen, asking to apply or discard unsaved changes first. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void RequestClose();

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual bool OnBackRequested_Implementation() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The optional small label over the title (see System Label). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	/** The dark panel behind the settings list. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<class UALKFrame> ListPanel;

	/** A small line under the description title: the section of the focused setting ("Display"). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionRef;

	/** Sets the technical label from the page title or the tab shown. */
	void UpdateSystemLabel();

	/** The row holding the tabs and their key caps; hidden on a single page. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TabRow;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> TabBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Separator;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> SettingsList;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DescriptionPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionTitle;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ApplyButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ResetButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> BackButton;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Assetpro Menu|Settings")
	TArray<TObjectPtr<UALKSetting>> AllSettings;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> TabButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKSettingRow>> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SectionHeaders;

	TArray<EALKSettingsCategory> TabCategories;
	int32 ActiveTab = 0;
	bool bFocusFirstRowPending = false;

	bool bPageMode = false;
	FText PageTitle;
	TArray<EALKSettingsCategory> PageCategories;

private:
	UALKSettingsSubsystem* GetSettingsSubsystem() const;
	TSubclassOf<UALKSettingRow> GetRowClass(EALKSettingType Type) const;
	void RebuildTabs();
	void RebuildList();
	void RefreshRows();
	void UpdateFooter();
	void UpdateDescription(const UALKSetting* Setting);
	void AskToKeepDisplayMode();
	void StyleSectionHeader(UTextBlock* Header, const UALKTheme& Theme) const;
	void HandleAnySettingChanged(UALKSetting* Setting);
	void ShowPreviousTab();
	void ShowNextTab();
	void HandleRowFocused(UALKSettingRow* Row, bool bFromNavigation);

	/** Up from the footer buttons: back to the setting the player came from. */
	UFUNCTION()
	UWidget* HandleFooterNavigateUp(EUINavigation Direction);

	FDelegateHandle SettingChangedHandle;
	TWeakObjectPtr<UALKSettingRow> LastFocusedRow;
};
