// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/SlateBrush.h"
#include "ALKTypes.h"

class UALKTheme;
class UALKFrame;
class UALKIconWidget;
class UALKThumbnail;
enum class EALKIcon : uint8;
class UTextBlock;
class UVerticalBox;
class UVerticalBoxSlot;
class UHorizontalBox;
class UHorizontalBoxSlot;
class UOverlay;
class UOverlaySlot;
class USizeBox;
class UTexture2D;
class UWidget;

/**
 * Small helpers used to build the default (code generated) layouts of every plugin widget.
 * Handy for your own C++ subclasses too.
 */
struct ASSETPROLOBBYKIT_API FALKWidgetBuilder
{
	template <typename TWidget>
	static TWidget* Make(UWidgetTree& Tree, FName Name = NAME_None)
	{
		return Tree.ConstructWidget<TWidget>(TWidget::StaticClass(), Name);
	}

	static UTextBlock* MakeText(UWidgetTree& Tree, FName Name, const FText& Text = FText::GetEmpty());

	/** Applies the font and colour of a text role. Titles and buttons follow the theme's uppercase switches. */
	static void StyleText(UTextBlock* Text, const UALKTheme& Theme, EALKTextRole Role);
	static void StyleText(UTextBlock* Text, const UALKTheme& Theme, EALKTextRole Role, const FLinearColor& Color);

	/**
	 * Sets a text block's font and trims its line box (see GetLineBoxTrim). Use it instead of Set Font wherever a font
	 * is put on a text block, so rows, buttons and titles keep their height whatever face the theme uses.
	 */
	static void SetTextFont(UTextBlock* Text, const FSlateFontInfo& Font);

	/**
	 * The (negative) top and bottom margin that brings a font's line box down to what a text face of that size takes.
	 * A display face such as Oswald keeps a lot of room over its capitals and under its baseline, which would make every
	 * row it stands in taller and push its text off centre; text faces get no margin.
	 */
	static FMargin GetLineBoxTrim(const FSlateFontInfo& Font);

	/** From the top of a text block in Font, as SetTextFont lays it out, down to its baseline (pixels at 1080p). */
	static float GetBaselineFromTop(const FSlateFontInfo& Font);

	/**
	 * Puts texts that stand side by side in a horizontal box, each aligned to the top, on one baseline whatever their
	 * fonts and sizes (a time, a name in the display face, a sentence in the text face): every slot gets the top padding
	 * that brings its text's baseline down to the lowest one. Call it after the fonts are set and the texts are added.
	 */
	static void AlignBaselines(const TArray<UTextBlock*>& Texts);

	static UVerticalBoxSlot* AddToVerticalBox(UVerticalBox* Box, UWidget* Child, const FMargin& Padding = FMargin(), EHorizontalAlignment HAlign = HAlign_Fill, EVerticalAlignment VAlign = VAlign_Fill, float FillWeight = 0.f);

	/** FillWeight 0 = auto size. */
	static UHorizontalBoxSlot* AddToHorizontalBox(UHorizontalBox* Box, UWidget* Child, const FMargin& Padding = FMargin(), EHorizontalAlignment HAlign = HAlign_Fill, EVerticalAlignment VAlign = VAlign_Center, float FillWeight = 0.f);

	static UOverlaySlot* AddToOverlay(UOverlay* Overlay, UWidget* Child, EHorizontalAlignment HAlign = HAlign_Fill, EVerticalAlignment VAlign = VAlign_Fill, const FMargin& Padding = FMargin());

	/** Wraps a widget in a size box. Values <= 0 are left unset. */
	static USizeBox* WrapInSizeBox(UWidgetTree& Tree, UWidget* Child, float Width, float Height, bool bMinimum = false);

	/** A vertical gradient texture (1 x 512) used for screen backgrounds. */
	static UTexture2D* CreateGradientTexture(UObject* Outer, const FLinearColor& Top, const FLinearColor& Bottom);

	/** The theme's background brush, or a gradient brush built from Background Top/Bottom. */
	static FSlateBrush MakeBackgroundBrush(const UALKTheme& Theme, UObject* Outer, TObjectPtr<UTexture2D>& InOutGradientTexture);

	/** Which generated texture to get. The backdrop (UALKAtmosphere) draws them. */
	enum class EOverlayTexture : uint8
	{
		/** Tileable fine grain (grey, opaque); drawn repeated at low opacity. */
		Grain,
		/** Black towards the edges, clear in the middle; stretched over the screen. */
		Vignette,
		/** Black on the left fading out to the right, behind a menu on the left. */
		ScrimLeft,
		/** Black on the right fading out to the left. */
		ScrimRight,
		/** Black in the middle fading out to both sides. */
		ScrimCenter,
		/** A large, smooth round glow (white, see-through, dithered so it does not band when stretched). */
		Haze,
		Count
	};

	/** A generated overlay texture, shared by every screen. Keep the returned pointer in a UPROPERTY while you use it. */
	static UTexture2D* GetOverlayTexture(EOverlayTexture Type);

	/**
	 * Adds the menu background to an overlay: an image named BaseName (the theme's gradient or brush), a key art picture
	 * (BackgroundPicture, in a scale box that covers the screen) with bWithPicture, the game seen through a blur and a
	 * dim (GameBlur, DimImage; shown over a paused game), the backdrop (Atmosphere: the theme's glows, pattern and
	 * vignette) and a blur and dim over it for pages (SceneBlur, SceneDim). Style them with
	 * StyleBackgroundLayers. Returns the base image.
	 */
	static class UImage* AddBackgroundLayers(UWidgetTree& Tree, UOverlay* Root, FName BaseName = TEXT("BackgroundImage"), bool bWithPicture = true);

	/**
	 * Styles the layers added by AddBackgroundLayers (BaseName must match). Over a paused game (bOverGame) the game
	 * shows blurred and dimmed, with the backdrop's vignette on it; otherwise the theme's gradient and backdrop, or
	 * (bKeyArt, the main menu) the menu config's key art in its place. Opacity fades the gradient and the backdrop, for
	 * a main menu over a 3D level. bDimScene blurs the scene and lays the theme's Page Dim over it (SceneBlur, SceneDim),
	 * for screens full of panels and text; the others get the theme's Menu Scrim on the menu's side instead.
	 * InOutOverlayTextures keeps the key art alive.
	 */
	static void StyleBackgroundLayers(UWidgetTree& Tree, const UALKTheme& Theme, UObject* Outer, TObjectPtr<UTexture2D>& InOutGradientTexture,
		TArray<TObjectPtr<UTexture2D>>& InOutOverlayTextures, float Opacity = 1.f, FName BaseName = TEXT("BackgroundImage"), bool bOverGame = false,
		bool bDimScene = false, bool bKeyArt = true);

	/** A code-drawn icon (see EALKIcon). */
	static UALKIconWidget* MakeIcon(UWidgetTree& Tree, FName Name, EALKIcon Icon, float Size = 20.f);

	/**
	 * A small tag: a chip holding an icon (collapsed when Icon is None) and a text ("4v4", "Ready", a lobby code).
	 * Colour it with StyleChip, on build and whenever the theme changes.
	 */
	static UALKFrame* MakeChip(UWidgetTree& Tree, FName Name, const FText& Text, EALKIcon Icon);

	/** Colours a chip made by MakeChip: its frame, its text (the theme's data font) and its icon. */
	static void StyleChip(UALKFrame* Chip, const UALKTheme& Theme, const FLinearColor& Fill, const FLinearColor& Outline, const FLinearColor& TextColor);

	/** Changes the text of a chip made by MakeChip. */
	static void SetChipText(UALKFrame* Chip, const FText& Text);

	/**
	 * Generated placeholder art for things without a picture (a map, a playlist): a calm block-out skyline in one of
	 * six colourings, the same every time for the same Id. Keep the returned pointer in a UPROPERTY while you use it.
	 */
	static UTexture2D* GetPlaceholderArt(FName Id);

	/**
	 * Gives a panel a picture band across its top (a map's picture behind the top of a dialog): the panel's content
	 * moves into an overlay with ContentPadding round it and the band, Height tall, lies behind it from edge to edge.
	 * The band starts collapsed; show it with SetPanelBandPicture and style it with StylePanelBand. Returns the band.
	 */
	static UALKThumbnail* AddPanelBand(UWidgetTree& Tree, UALKFrame* Panel, UWidget* Content, const FMargin& ContentPadding, float Height, FName Name = TEXT("PictureBand"));

	/** Styles a band made by AddPanelBand: it follows the panel's corners and melts into the panel (the theme's Picture Veil). */
	static void StylePanelBand(UALKThumbnail* Band, const UALKTheme& Theme);

	/** Shows a picture in a band made by AddPanelBand, or hides the band when there is none. */
	static void SetPanelBandPicture(UALKThumbnail* Band, UTexture2D* Picture);

	/**
	 * An icon in a soft disc, Size pixels across: what stands over the line of an empty list or a status ("No games
	 * found"). Colour it with StyleIconDisc, on build and whenever the theme changes; SetIconDiscIcon changes its icon.
	 */
	static UALKFrame* MakeIconDisc(UWidgetTree& Tree, FName Name, EALKIcon Icon, float Size = 56.f);

	/** Colours a disc made by MakeIconDisc: the theme's raised colour with the icon in the muted text colour. */
	static void StyleIconDisc(UALKFrame* Disc, const UALKTheme& Theme);

	/** Changes the icon of a disc made by MakeIconDisc. */
	static void SetIconDiscIcon(UALKFrame* Disc, EALKIcon Icon);

	/**
	 * An empty state: an icon in a soft disc, centred over the line that says a list has nothing to show ("No messages
	 * yet"). Returns the column holding both: add it where the line alone would go. Colour it with StyleEmptyState and
	 * show or hide it with SetEmptyStateShown, both by its line.
	 */
	static UVerticalBox* MakeEmptyState(UWidgetTree& Tree, FName Name, EALKIcon Icon, UTextBlock* Line, float DiscSize = 56.f);

	/** Colours the icon of an empty state made by MakeEmptyState (style the line itself as any text). */
	static void StyleEmptyState(UTextBlock* Line, const UALKTheme& Theme);

	/** Changes the icon of an empty state made by MakeEmptyState (different reasons for an empty list). */
	static void SetEmptyStateIcon(UTextBlock* Line, EALKIcon Icon);

	/**
	 * Shows or hides an empty state by its line: the icon over it shows and hides with it. A line that is not part of
	 * an empty state (a designer layout with only the text) is shown or hidden itself.
	 */
	static void SetEmptyStateShown(UTextBlock* Line, bool bShown);

	/** A brush that draws a texture once, stretched over the widget. */
	static FSlateBrush MakeTextureBrush(UTexture2D* Texture, const FLinearColor& Tint = FLinearColor::White, bool bTile = false);
};
