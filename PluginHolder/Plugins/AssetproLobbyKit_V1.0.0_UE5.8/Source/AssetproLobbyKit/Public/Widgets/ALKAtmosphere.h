// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "ALKAtmosphere.generated.h"

class SALKAtmosphere;
class UALKTheme;
class UTexture2D;

/** Everything the backdrop draws, resolved from the theme. Sizes are in pixels at 1080p. */
struct FALKAtmosphereStyle
{
	struct FLayer
	{
		FSlateBrush Brush;
		FLinearColor Color = FLinearColor::Black;
		float Depth = 0.5f;
		float DriftSpeed = 0.f;
		bool bInFront = false;
		/** A full picture: drawn first, under the glows. */
		bool bBehindGlows = false;
	};

	TArray<FLayer> Layers;

	bool bDrawScene = true;
	float SceneOpacity = 1.f;
	float Parallax = 0.f;

	/** A big smooth glow, drawn a few times in the accent colour. */
	FSlateBrush GlowBrush;
	FLinearColor GlowColor = FLinearColor::Transparent;
	float GlowDrift = 1.f;

	FLinearColor PatternColor = FLinearColor::Transparent;
	float PatternSpacing = 38.f;
	float PatternDotSize = 2.4f;

	FSlateBrush ScrimBrush;
	FLinearColor ScrimColor = FLinearColor::Transparent;
	/** Scales the scrim: 0 on screens that dim the whole scene instead. */
	float ScrimOpacity = 1.f;
	/** Where the menu sits: the scrim darkens from that side. */
	EHorizontalAlignment ScrimSide = HAlign_Left;

	FSlateBrush VignetteBrush;
	float Vignette = 0.f;

	FSlateBrush GrainBrush;
	float Grain = 0.f;
	/** New grain patterns per second; 0 keeps the grain still. */
	float GrainSpeed = 0.f;
};

/**
 * The calm backdrop behind every menu: the theme's picture layers (each skin brings one full picture), a few large glows
 * in the accent colour that drift slowly, a faint dot pattern, a darkening behind the menu and a vignette. Everything
 * but the pictures is drawn in code, and it keeps moving while the game is paused. Screens add it through
 * FALKWidgetBuilder::AddBackgroundLayers; place one in your own UMG layouts too (it restyles itself from the active
 * theme).
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKAtmosphere : public UWidget
{
	GENERATED_BODY()

public:
	UALKAtmosphere(const FObjectInitializer& ObjectInitializer);

	/** Draws the theme's glows, pattern and picture layers. Off over a paused game, or behind key art that is the scene itself. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Atmosphere")
	void SetDrawScene(bool bInDrawScene);

	/** Fades the glows, pattern and picture layers; the vignette stays (a main menu over a 3D level). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Atmosphere")
	void SetSceneOpacity(float InOpacity);

	/**
	 * Scales the theme's Menu Scrim (the darkening on the menu's side): 1 on menus that show the backdrop clearly, 0 on
	 * screens that dim the whole scene anyway.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Atmosphere")
	void SetScrimOpacity(float InOpacity);

	/** Restyles from a theme. Runs by itself when the widget is built and whenever the active theme changes. */
	void ApplyTheme(const UALKTheme& Theme);

	/** The shared parallax offset every backdrop uses this frame (-1..1 on each axis), for widgets that want to follow it. */
	static FVector2D GetParallaxOffset();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere")
	bool bDrawScene = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere", meta = (ClampMin = "0", ClampMax = "1"))
	float SceneOpacity = 1.f;

	/** Scales the theme's Menu Scrim on this backdrop (see Set Scrim Opacity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atmosphere", meta = (ClampMin = "0", ClampMax = "1"))
	float ScrimOpacity = 1.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

private:
	void HandleThemeChanged();
	void PushStyle();

	FALKAtmosphereStyle Style;
	TSharedPtr<SALKAtmosphere> MyAtmosphere;
	FDelegateHandle ThemeChangedHandle;

	/** Keeps the layer textures and the generated glow, grain and vignette textures alive while drawn. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> Textures;
};
