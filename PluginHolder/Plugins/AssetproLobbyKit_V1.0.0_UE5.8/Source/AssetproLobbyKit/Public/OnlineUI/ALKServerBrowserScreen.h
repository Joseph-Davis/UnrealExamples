// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineTypes.h"
#include "Screens/ALKScreen.h"
#include "Templates/SubclassOf.h"
#include "Types/SlateEnums.h"
#include "ALKServerBrowserScreen.generated.h"

class UALKButton;
class UALKFrame;
class UALKIconWidget;
class UALKRule;
class UALKServerRow;
class UALKSpinner;
class UALKTextField;
class UALKThumbnail;
class UBorder;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

/** The lists the server browser can show. */
UENUM(BlueprintType)
enum class EALKServerBrowserTab : uint8
{
	/** Every game the search found. */
	All,
	/** The games the player marked with the star. */
	Favourites,
	/** The games the player was in lately, newest first. */
	Recent,
	/** Games on the local network. */
	LAN
};

/**
 * Browse Games: a table of the games that were found, with tabs (All, Favourites, Recent, LAN), filter chips (Not
 * full, Not empty, No password, a region), a search box that filters as you type, columns that sort when their header
 * is picked, and the details of the focused game on the right with Join game, Add favourite and Copy address.
 *
 * Keys: Enter or the accept button joins the focused game, F / the left face button toggles its star, R / the top face
 * button searches again, Q and E / the bumpers switch tabs. The screen searches when it opens and follows the online
 * subsystem's events. Passwords are asked for by the Online Flow subsystem; connection errors are shown by it too.
 *
 * Designer widget names: BackgroundImage, TitleText, CountText, TabBox, NotFullChip, NotEmptyChip, NoPasswordChip, RegionChip,
 * SearchField, RefreshSpinner, RefreshButton, TablePanel, HeaderRow, HeaderRule, ServerList, StateBox, StateSpinner,
 * StateIcon, StateText, StateDetailText, DetailPanel, DetailBox, DetailEmptyText, DetailPicture, DetailStatusChip, DetailNameText, DetailLockIcon,
 * DetailSubText, DetailRows, JoinButton, FavouriteButton, CopyButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKServerBrowserScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKServerBrowserScreen(const FObjectInitializer& ObjectInitializer);

	/** Shows one of the lists. All and LAN search again. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void SetActiveTab(EALKServerBrowserTab Tab);

	/** The list that is shown. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Server Browser")
	EALKServerBrowserTab GetActiveTab() const { return ActiveTab; }

	/** Moves one tab to the right (or to the left with a negative number), wrapping round. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void StepTab(int32 Direction);

	/** Searches for games again (the Refresh button). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void RefreshGames();

	/** Sorts by a column; picking the column that already sorts flips the direction. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void SortBy(EALKSessionSort Column);

	/** Joins the game the details show (asks for its password first when it has one). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void JoinFocusedGame();

	/** Adds the game the details show to the favourites, or takes it off them. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void ToggleFocusedFavourite();

	/** Copies the address of the game the details show, when it is known. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	void CopyFocusedAddress();

	/** The game the details show. False when the list is empty. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Server Browser")
	bool GetFocusedGame(FALKSessionInfo& OutSession) const;

	/** The widget made for each line of the table. Point it at your Blueprint child of ALK Server Row to restyle the lines. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser")
	TSubclassOf<UALKServerRow> RowClass;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual bool OnBackRequested_Implementation() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** "128 games found". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

	/** The tab buttons are made in here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> TabBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> NotFullChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> NotEmptyChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> NoPasswordChip;

	/** Cycles the regions the online provider offers. Hidden when it offers none. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> RegionChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKTextField> SearchField;

	/** Turns while a search runs. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKSpinner> RefreshSpinner;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> RefreshButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TablePanel;

	/** The column headers are made in here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HeaderRow;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKRule> HeaderRule;

	/** One ALK Server Row per game. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ServerList;

	/** Shown in the middle of the table while it is empty: searching, or nothing found. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> StateBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKSpinner> StateSpinner;

	/** The icon over the line that says why the table is empty (made by FALKWidgetBuilder::MakeIconDisc). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> StateIcon;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StateDetailText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DetailPanel;

	/** Everything about the focused game. Hidden while the list is empty. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> DetailBox;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailEmptyText;

	/** The map's picture, or placeholder art picked by the map's Id. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKThumbnail> DetailPicture;

	/** "Match in progress", over the picture, while the focused game's match already runs. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKFrame> DetailStatusChip;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailNameText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> DetailLockIcon;

	/** "Capture Point - Dockside". */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DetailSubText;

	/** The "Players / 3 of 8" lines are made in here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> DetailRows;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> JoinButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> FavouriteButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Server Browser", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CopyButton;

private:
	/** One column header: a small button, the arrow that shows the sort direction, and its focus outline. */
	void AddHeaderCell(UWidgetTree& Tree, EALKSessionSort Column, const FText& Title, float Width, bool bRightAligned, float EndPadding);
	void BuildTabs();
	void UpdateTabs();
	void UpdateChips();
	void UpdateHeader();
	void UpdateState();

	/** Asks the online subsystem for games (All and LAN only). */
	void StartSearch();
	/** The games of the shown tab, filtered and sorted. */
	void CollectEntries(TArray<FALKServerBookmark>& OutEntries) const;
	/** Fills the table again and keeps the focus on the same game when it is still listed. */
	void RebuildList();
	void ShowDetails();
	void SetDetailRow(int32& InOutRow, const FText& Name, const FText& Value);
	FText GetRegionName(const FString& RegionId) const;
	bool GetEntryAddress(const FALKServerBookmark& Entry, FString& OutAddress) const;
	static FString MakeEntryKey(const FALKSessionInfo& Session);
	bool IsShowingBookmarks() const { return ActiveTab == EALKServerBrowserTab::Favourites || ActiveTab == EALKServerBrowserTab::Recent; }
	bool IsTypingSearch() const;
	bool IsAnyRowFocused() const;
	void FocusList();

	void JoinEntry(int32 EntryIndex);
	void HandleRowFocused(UALKServerRow* Row);
	void HandleRowClicked(UALKServerRow* Row);
	void HandleSearchChanged(const FText& Text);
	void HandleSearchCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	void HandleSessionsFound(const TArray<FALKSessionInfo>& Sessions);
	void HandleStateChanged(EALKOnlineState NewState, EALKOnlineState OldState);
	void HandleListsChanged();
	void ToggleFilter(int32 ChipIndex);
	void CycleRegion();

	EALKServerBrowserTab ActiveTab = EALKServerBrowserTab::All;
	/** The chips, the region and the search text. */
	FALKSessionFilter Filter;
	EALKSessionSort SortColumn = EALKSessionSort::Ping;
	bool bSortAscending = true;
	/** The player picked a column: Recent then sorts too instead of staying newest first. */
	bool bSortPicked = false;

	/** What the table shows, in order. Live games have no Connect String. */
	TArray<FALKServerBookmark> Entries;
	/** The entry the details show. */
	int32 FocusedIndex = INDEX_NONE;
	/** What tells that entry from the others, to find it again after the table is filled again. */
	FString FocusedKey;
	/** A row has to take the focus once the table is laid out. */
	bool bFocusPending = false;
	/** The room the header leaves for the table's scroll bar while it shows. */
	float HeaderInset = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKServerRow>> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> TabButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> HeaderButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKIconWidget>> HeaderArrows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKFrame>> HeaderFrames;

	/** The details' lines: a name and a value each, reused for every game. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DetailValueTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> DetailRowWidgets;

	TArray<EALKSessionSort> HeaderColumns;
	TArray<bool> HeaderFocused;

	FDelegateHandle SessionsHandle;
	FDelegateHandle StateHandle;
	FDelegateHandle ListsHandle;
};
