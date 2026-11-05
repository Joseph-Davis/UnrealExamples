// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineConfig.h"
#include "Screens/ALKScreen.h"
#include "ALKLoadoutScreen.generated.h"

class AALKLobbyGameState;
class AALKLobbyPlayerState;
class UALKButton;
class UALKCardButton;
class UALKFrame;
class UALKIconWidget;
class UALKPlayerNetComponent;
class UALKThumbnail;
class UBorder;
class UImage;
class UScrollBox;
class UTextBlock;
class UWrapBox;

/**
 * The loadout screen: the kit's hook for a class, character or kit picker. It shows a card per loadout of the Online
 * Config (picture or generated art, name, a "Level n" tag and a dimmed card while the player's level is too low, an
 * "Equipped" tag on the chosen one), a summary of the focused loadout and a row of colours to pick from.
 *
 * Equip sends the choice to the server (the player state's Loadout Id and Color Index hold it); what a loadout does in
 * your game is up to you. Outside a game the screen still opens and shows the cards, with Equip turned off.
 *
 * Designer widget names: BackgroundImage, TitleText, NoticeText, CardScroll (Scroll Box), CardGrid (Wrap Box),
 * SummaryPanel, SummaryPicture, SummaryName, SummaryLock, SummaryDescription, ColorLabel, ColorGrid (Wrap Box),
 * EquipButton, EmptyText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKLoadoutScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKLoadoutScreen(const FObjectInitializer& ObjectInitializer);

	/**
	 * The colour behind a player's Color Index: an even ramp round the colour wheel, so every index of Color Count gets a
	 * colour of its own. Use the same function wherever a player's colour is shown.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Loadout")
	static FLinearColor GetPlayerColor(int32 ColorIndex, int32 ColorCount);

	/** Equips the loadout whose card has focus (when the player's level allows it and the player is in a game). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Loadout")
	void EquipFocused();

	/** Equips a loadout by its place in the Online Config's Loadouts list. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Loadout")
	void EquipLoadout(int32 LoadoutIndex);

	/** Picks a colour by its number. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Loadout")
	void PickColor(int32 ColorIndex);

	/** Reads the config and the local player again now. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Loadout")
	void RefreshLoadouts();

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** "Join a lobby to equip a loadout", outside a game. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NoticeText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> CardScroll;

	/** The loadout cards are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> CardGrid;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SummaryPanel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UALKThumbnail> SummaryPicture;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SummaryName;

	/** "Unlocks at level 5", for a loadout the player cannot use yet. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SummaryLock;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SummaryDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ColorLabel;

	/** The colour swatches are added here. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UWrapBox> ColorGrid;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> EquipButton;

	/** Shown when the Online Config lists no loadouts: where to add them. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Loadout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EmptyText;

private:
	AALKLobbyGameState* ResolveGameState();
	void UnbindGameState();
	void MarkDirty();
	UALKPlayerNetComponent* GetNetComponent() const;
	AALKLobbyPlayerState* GetLocalLobbyPlayer() const;
	int32 GetLocalLevel() const;

	void RebuildCards();
	void RebuildSwatches();
	void RefreshState();
	void UpdateSummary();
	void HandleCardFocused(UALKCardButton* Card);

	/** Right from the cards' edge: the player's colour (Equip when the colours cannot be picked). */
	UFUNCTION()
	UWidget* HandleNavigateToColors(EUINavigation Direction);

	/** Left from the colours and from Equip: the loadout on show. */
	UFUNCTION()
	UWidget* HandleNavigateToCards(EUINavigation Direction);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKCardButton>> Cards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> SwatchButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKIconWidget>> SwatchTicks;

	TArray<FALKLoadoutDefinition> Loadouts;
	TWeakObjectPtr<AALKLobbyGameState> BoundGameState;
	int32 FocusedIndex = 0;
	bool bDirty = true;
};
