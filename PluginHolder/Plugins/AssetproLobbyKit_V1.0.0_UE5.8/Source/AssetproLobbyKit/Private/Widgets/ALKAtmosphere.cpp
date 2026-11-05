// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKAtmosphere.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "CoreGlobals.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKAtmosphere)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKAtmospherePrivate
{
	/** Every backdrop on screen shares one smoothed pointer offset, so the scene does not jump between screens. */
	static FVector2f SharedParallax(0.f, 0.f);
	static uint64 SharedParallaxFrame = 0;
	static double SharedParallaxTime = 0.0;

	static FVector2f UpdateParallax(const FGeometry& Geometry, double Time)
	{
		if (SharedParallaxFrame == GFrameCounter)
		{
			return SharedParallax;
		}
		const double Delta = SharedParallaxTime > 0.0 ? FMath::Clamp(Time - SharedParallaxTime, 0.0, 0.1) : 0.0;
		SharedParallaxFrame = GFrameCounter;
		SharedParallaxTime = Time;

		FVector2f Target(0.f, 0.f);
		const FVector2f Size = FVector2f(Geometry.GetLocalSize());
		if (FSlateApplication::IsInitialized() && Size.X > 1.f && Size.Y > 1.f)
		{
			const FVector2f Local = FVector2f(Geometry.AbsoluteToLocal(FVector2D(FSlateApplication::Get().GetCursorPos())));
			Target.X = FMath::Clamp(Local.X / Size.X * 2.f - 1.f, -1.f, 1.f);
			Target.Y = FMath::Clamp(Local.Y / Size.Y * 2.f - 1.f, -1.f, 1.f);
		}
		const float Alpha = 1.f - FMath::Exp(-static_cast<float>(Delta) * 1.6f);
		SharedParallax = FMath::Lerp(SharedParallax, Target, Alpha);
		return SharedParallax;
	}

	/** Where each glow rests (share of the screen), how wide it is (share of the width) and how strong. */
	struct FGlowSpot
	{
		FVector2f Center;
		float Radius;
		float Strength;
	};
	static const FGlowSpot GlowSpots[] = {
		{ FVector2f(0.82f, 0.16f), 0.36f, 1.f },
		{ FVector2f(0.10f, 0.94f), 0.32f, 0.85f },
		{ FVector2f(0.56f, 0.62f), 0.24f, 0.6f }
	};
}

/** Paints the backdrop every frame (an active timer keeps it repainting, also while the game is paused). */
class SALKAtmosphere : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SALKAtmosphere) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SetClipping(EWidgetClipping::ClipToBounds);
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SALKAtmosphere::Animate));
	}

	void SetStyle(const FALKAtmosphereStyle& InStyle)
	{
		Style = InStyle;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(64.0, 64.0);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		using namespace ALKAtmospherePrivate;

		const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
		if (Size.X < 1.f || Size.Y < 1.f)
		{
			return LayerId;
		}
		const double Now = FALKEffects::Now();
		const bool bReduced = FALKEffects::AreReduced();
		const float Time = bReduced ? 0.f : static_cast<float>(FMath::Fmod(Now, 100000.0));
		const FLinearColor WidgetTint = InWidgetStyle.GetColorAndOpacityTint();
		const FLinearColor SceneTint = WidgetTint * FLinearColor(1.f, 1.f, 1.f, Style.SceneOpacity);
		const FVector2f Pointer = bReduced ? FVector2f(0.f, 0.f) : UpdateParallax(AllottedGeometry, Now);
		// Sizes are authored for 1080 pixels of height.
		const float Scale = Size.Y / 1080.f;
		int32 Layer = LayerId;

		// Each group of draws goes on its own layer, so Slate cannot reorder them while batching.
		auto Draw = [&](const FSlateBrush& Brush, const FVector2f& Position, const FVector2f& BoxSize, const FLinearColor& Color)
		{
			if (Color.A > 0.001f && Brush.GetResourceObject())
			{
				FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Position)),
					&Brush, ESlateDrawEffect::None, Color);
			}
		};

		// Three groups of layers: full pictures (under the glows), then shapes behind and in front of the dot pattern.
		enum class EGroup : uint8 { Pictures, Back, Front };
		auto DrawLayers = [&](EGroup Group)
		{
			for (const FALKAtmosphereStyle::FLayer& Backdrop : Style.Layers)
			{
				const EGroup LayerGroup = Backdrop.bBehindGlows ? EGroup::Pictures : (Backdrop.bInFront ? EGroup::Front : EGroup::Back);
				if (LayerGroup != Group || !Backdrop.Brush.GetResourceObject())
				{
					continue;
				}
				// Cover the screen, with room to move: near layers move further, so they are drawn a little larger.
				const FVector2f TextureSize = FVector2f(Backdrop.Brush.ImageSize);
				if (TextureSize.X < 1.f || TextureSize.Y < 1.f)
				{
					continue;
				}
				const float Overscan = 1.02f + 0.05f * Backdrop.Depth;
				const float Fit = FMath::Max(Size.X / TextureSize.X, Size.Y / TextureSize.Y) * Overscan;
				const FVector2f LayerSize = TextureSize * Fit;
				const FVector2f Offset = Pointer * (Style.Parallax * Scale * Backdrop.Depth) * FVector2f(1.f, 0.35f);
				const FVector2f Position = (Size - LayerSize) * 0.5f - Offset;
				const FLinearColor Color = Backdrop.Color * SceneTint;
				if (FMath::IsNearlyZero(Backdrop.DriftSpeed))
				{
					Draw(Backdrop.Brush, Position, LayerSize, Color);
				}
				else
				{
					// Drifting layers repeat sideways.
					const float Phase = FMath::Fmod(Time * Backdrop.DriftSpeed * Scale, LayerSize.X);
					for (float X = Position.X - Phase - LayerSize.X; X < Size.X; X += LayerSize.X)
					{
						Draw(Backdrop.Brush, FVector2f(X, Position.Y), LayerSize, Color);
					}
				}
			}
			++Layer;
		};

		if (Style.bDrawScene)
		{
			DrawLayers(EGroup::Pictures);

			// Large glows in the accent colour, each drifting slowly round its resting place.
			int32 SpotIndex = 0;
			for (const FGlowSpot& Spot : GlowSpots)
			{
				const float Phase = Time * 0.05f * Style.GlowDrift + SpotIndex * 2.1f;
				const FVector2f Drift(FMath::Sin(Phase) * 0.035f, FMath::Cos(Phase * 0.8f) * 0.03f);
				const FVector2f Center = (Spot.Center + Drift) * Size - Pointer * (Style.Parallax * Scale * (0.4f + 0.3f * SpotIndex));
				const FVector2f GlowSize(Spot.Radius * Size.X * 2.6f, Spot.Radius * Size.X * 1.9f);
				Draw(Style.GlowBrush, Center - GlowSize * 0.5f, GlowSize, Style.GlowColor * SceneTint * FLinearColor(1.f, 1.f, 1.f, Spot.Strength));
				++SpotIndex;
			}
			++Layer;

			DrawLayers(EGroup::Back);

			// A faint pattern of dots across the screen.
			if (Style.PatternColor.A > 0.f && Style.PatternSpacing >= 8.f)
			{
				FALKQuadBatch Dots(AllottedGeometry);
				const float Spacing = Style.PatternSpacing * Scale;
				const float Half = FMath::Max(0.6f, Style.PatternDotSize * Scale * 0.5f);
				const FLinearColor Color = Style.PatternColor * SceneTint;
				const FVector2f Shift = -Pointer * (Style.Parallax * Scale * 0.25f);
				for (float Y = FMath::Fmod(Shift.Y, Spacing) + Spacing * 0.5f - Spacing; Y < Size.Y + Spacing; Y += Spacing)
				{
					for (float X = FMath::Fmod(Shift.X, Spacing) + Spacing * 0.5f - Spacing; X < Size.X + Spacing; X += Spacing)
					{
						Dots.AddRect(FVector2f(X - Half, Y - Half), FVector2f(X + Half, Y + Half), Color);
					}
				}
				Dots.Flush(OutDrawElements, Layer);
				++Layer;
			}

			DrawLayers(EGroup::Front);
		}

		// Darker behind the menu, so its text reads over bright picture layers.
		if (Style.ScrimColor.A > 0.f && Style.ScrimOpacity > 0.f && Style.ScrimBrush.GetResourceObject())
		{
			const float Width = Style.ScrimSide == HAlign_Center || Style.ScrimSide == HAlign_Fill ? Size.X : Size.X * 0.72f;
			const float Left = Style.ScrimSide == HAlign_Right ? Size.X - Width : 0.f;
			Draw(Style.ScrimBrush, FVector2f(Left, 0.f), FVector2f(Width, Size.Y), Style.ScrimColor * WidgetTint * FLinearColor(1.f, 1.f, 1.f, Style.ScrimOpacity));
			++Layer;
		}

		if (Style.Vignette > 0.f)
		{
			Draw(Style.VignetteBrush, FVector2f(0.f, 0.f), Size, WidgetTint * FLinearColor(1.f, 1.f, 1.f, Style.Vignette));
			++Layer;
		}

		// Optional film grain (off by default): a new pattern Grain Speed times a second, or a still texture.
		if (Style.Grain > 0.f && Style.GrainBrush.GetResourceObject())
		{
			const int32 Frame = Style.GrainSpeed > 0.f ? FMath::FloorToInt(Time * Style.GrainSpeed) : 0;
			const float TileSize = 256.f * FMath::Max(Scale, 1.f);
			GrainBrush = Style.GrainBrush;
			GrainBrush.ImageSize = FVector2D(TileSize, TileSize);
			GrainBrush.Tiling = ESlateBrushTileType::Both;
			const FVector2f Jitter(FALKPaint::Hash(Frame, 3) * TileSize, FALKPaint::Hash(Frame, 5) * TileSize);
			Draw(GrainBrush, -Jitter, Size + Jitter, WidgetTint * FLinearColor(1.f, 1.f, 1.f, Style.Grain * 0.16f));
			++Layer;
		}

		return Layer;
	}

private:
	EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
		return EActiveTimerReturnType::Continue;
	}

	FALKAtmosphereStyle Style;
	/** The grain brush with this frame's tile size. */
	mutable FSlateBrush GrainBrush;
};

UALKAtmosphere::UALKAtmosphere(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibilityInternal(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UALKAtmosphere::RebuildWidget()
{
	MyAtmosphere = SNew(SALKAtmosphere);
	if (!ThemeChangedHandle.IsValid())
	{
		ThemeChangedHandle = UALKTheme::OnAnyThemeChanged().AddUObject(this, &ThisClass::HandleThemeChanged);
	}
	HandleThemeChanged();
	return MyAtmosphere.ToSharedRef();
}

void UALKAtmosphere::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyAtmosphere.Reset();
}

void UALKAtmosphere::BeginDestroy()
{
	UALKTheme::OnAnyThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}

void UALKAtmosphere::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	Style.bDrawScene = bDrawScene;
	Style.SceneOpacity = SceneOpacity;
	Style.ScrimOpacity = ScrimOpacity;
	PushStyle();
}

#if WITH_EDITOR
const FText UALKAtmosphere::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

void UALKAtmosphere::SetDrawScene(bool bInDrawScene)
{
	bDrawScene = bInDrawScene;
	Style.bDrawScene = bDrawScene;
	PushStyle();
}

void UALKAtmosphere::SetSceneOpacity(float InOpacity)
{
	SceneOpacity = FMath::Clamp(InOpacity, 0.f, 1.f);
	Style.SceneOpacity = SceneOpacity;
	PushStyle();
}

void UALKAtmosphere::SetScrimOpacity(float InOpacity)
{
	ScrimOpacity = FMath::Clamp(InOpacity, 0.f, 1.f);
	Style.ScrimOpacity = ScrimOpacity;
	PushStyle();
}

FVector2D UALKAtmosphere::GetParallaxOffset()
{
	return FVector2D(ALKAtmospherePrivate::SharedParallax);
}

void UALKAtmosphere::HandleThemeChanged()
{
	if (const UALKTheme* Theme = UALKDeveloperSettings::GetTheme())
	{
		ApplyTheme(*Theme);
	}
}

void UALKAtmosphere::ApplyTheme(const UALKTheme& Theme)
{
	using EOverlay = FALKWidgetBuilder::EOverlayTexture;

	Textures.Reset();
	Style = FALKAtmosphereStyle();
	Style.bDrawScene = bDrawScene;
	Style.SceneOpacity = SceneOpacity;
	Style.ScrimOpacity = ScrimOpacity;

	for (const FALKBackdropLayer& Backdrop : Theme.BackdropLayers)
	{
		UTexture2D* Texture = Backdrop.Texture.LoadSynchronous();
		if (!Texture)
		{
			continue;
		}
		Textures.Add(Texture);
		FALKAtmosphereStyle::FLayer& Layer = Style.Layers.AddDefaulted_GetRef();
		Layer.Brush = FALKWidgetBuilder::MakeTextureBrush(Texture);
		Layer.Color = Backdrop.Color;
		Layer.Depth = Backdrop.Depth;
		Layer.DriftSpeed = Backdrop.DriftSpeed;
		Layer.bInFront = Backdrop.bInFront;
		Layer.bBehindGlows = Backdrop.bBehindGlows;
	}

	auto Generated = [this](EOverlay Type)
	{
		UTexture2D* Texture = FALKWidgetBuilder::GetOverlayTexture(Type);
		if (Texture)
		{
			Textures.Add(Texture);
		}
		return FALKWidgetBuilder::MakeTextureBrush(Texture);
	};

	Style.Parallax = Theme.ParallaxAmount;
	Style.GlowBrush = Generated(EOverlay::Haze);
	Style.GlowColor = Theme.BackgroundGlow;
	Style.GlowDrift = Theme.GlowDriftSpeed;
	Style.PatternColor = Theme.PatternColor;
	Style.PatternSpacing = Theme.PatternSpacing;
	Style.PatternDotSize = Theme.PatternDotSize;

	Style.ScrimSide = Theme.MainMenuAlignment.GetValue();
	Style.ScrimBrush = Generated(Style.ScrimSide == HAlign_Right ? EOverlay::ScrimRight
		: (Style.ScrimSide == HAlign_Center || Style.ScrimSide == HAlign_Fill ? EOverlay::ScrimCenter : EOverlay::ScrimLeft));
	Style.ScrimColor = Theme.MenuScrim;

	Style.VignetteBrush = Generated(EOverlay::Vignette);
	Style.Vignette = Theme.BackgroundVignette;
	Style.GrainBrush = Generated(EOverlay::Grain);
	Style.Grain = Theme.BackgroundGrain;
	Style.GrainSpeed = Theme.GrainSpeed;
	PushStyle();
}

void UALKAtmosphere::PushStyle()
{
	if (MyAtmosphere.IsValid())
	{
		MyAtmosphere->SetStyle(Style);
	}
}

#undef LOCTEXT_NAMESPACE
