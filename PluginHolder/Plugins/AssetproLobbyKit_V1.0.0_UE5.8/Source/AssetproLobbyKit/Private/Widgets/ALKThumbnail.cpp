// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKThumbnail.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKThumbnail)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

/** Draws a texture cropped to cover the widget, with rounded corners. */
class SALKThumbnail : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SALKThumbnail) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
	}

	void SetStyle(UTexture2D* InTexture, float InRadius, const FLinearColor& InTint, const FVector2D& InDesiredSize, const FLinearColor& InVeil)
	{
		Veil = InVeil;
		Brush = FSlateBrush();
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = 0.f;
		if (InTexture)
		{
			Brush.SetResourceObject(InTexture);
			Brush.ImageSize = FVector2D(InTexture->GetSizeX(), InTexture->GetSizeY());
		}
		Radius = InRadius;
		Tint = InTint;
		DesiredSize = InDesiredSize;
		Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return DesiredSize;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FVector2f Size = FVector2f(AllottedGeometry.GetLocalSize());
		if (Size.X < 1.f || Size.Y < 1.f || !Brush.GetResourceObject() || Brush.ImageSize.X < 1.0 || Brush.ImageSize.Y < 1.0)
		{
			return LayerId;
		}
		// Cover the box: crop the picture's longer side instead of squashing it.
		const float WidgetAspect = Size.X / Size.Y;
		const float PictureAspect = static_cast<float>(Brush.ImageSize.X / Brush.ImageSize.Y);
		FBox2f UV(FVector2f(0.f, 0.f), FVector2f(1.f, 1.f));
		if (WidgetAspect > PictureAspect)
		{
			const float Visible = PictureAspect / WidgetAspect;
			UV.Min.Y = (1.f - Visible) * 0.5f;
			UV.Max.Y = 1.f - UV.Min.Y;
		}
		else
		{
			const float Visible = WidgetAspect / PictureAspect;
			UV.Min.X = (1.f - Visible) * 0.5f;
			UV.Max.X = 1.f - UV.Min.X;
		}
		FSlateBrush Shown = Brush;
		Shown.SetUVRegion(UV);
		const float R = FMath::Clamp(Radius, 0.f, FMath::Min(Size.X, Size.Y) * 0.5f);
		Shown.OutlineSettings.CornerRadii = FVector4(R, R, R, R);
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &Shown, ESlateDrawEffect::None, Tint * InWidgetStyle.GetColorAndOpacityTint());
		if (Veil.A > 0.f)
		{
			// The veil shares the picture's lower corners, so the rounding stays exact.
			FALKPaint::GradientBox(OutDrawElements, LayerId + 1, AllottedGeometry, FVector2f(0.f, Size.Y * 0.38f), Size, R, EALKGradient::FadeUp,
				Veil * InWidgetStyle.GetColorAndOpacityTint());
			return LayerId + 1;
		}
		return LayerId;
	}

private:
	FSlateBrush Brush;
	float Radius = 8.f;
	FLinearColor Tint = FLinearColor::White;
	FLinearColor Veil = FLinearColor::Transparent;
	FVector2D DesiredSize = FVector2D(320.0, 180.0);
};

TSharedRef<SWidget> UALKThumbnail::RebuildWidget()
{
	FALKPaint::PrepareGradients();
	MyThumbnail = SNew(SALKThumbnail);
	if (!ThemeChangedHandle.IsValid())
	{
		ThemeChangedHandle = UALKTheme::OnAnyThemeChanged().AddUObject(this, &ThisClass::HandleThemeChanged);
	}
	SynchronizeProperties();
	return MyThumbnail.ToSharedRef();
}

void UALKThumbnail::BeginDestroy()
{
	UALKTheme::OnAnyThemeChanged().Remove(ThemeChangedHandle);
	ThemeChangedHandle.Reset();
	Super::BeginDestroy();
}

void UALKThumbnail::HandleThemeChanged()
{
	if (bThemeVeil)
	{
		SynchronizeProperties();
	}
}

void UALKThumbnail::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyThumbnail.Reset();
}

void UALKThumbnail::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (!Picture)
	{
		Placeholder = FALKWidgetBuilder::GetPlaceholderArt(PlaceholderId);
	}
	else
	{
		Placeholder = nullptr;
	}
	if (MyThumbnail.IsValid())
	{
		FLinearColor ShownVeil = Veil;
		if (bThemeVeil)
		{
			const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
			ShownVeil = Theme ? Theme->Panel.CopyWithNewOpacity(Theme->Panel.A * Theme->PictureVeil) : FLinearColor::Transparent;
		}
		MyThumbnail->SetStyle(Picture ? Picture.Get() : Placeholder.Get(), CornerRadius, Tint, ThumbnailSize, ShownVeil);
	}
}

void UALKThumbnail::SetPicture(UTexture2D* InPicture, FName InPlaceholderId)
{
	Picture = InPicture;
	PlaceholderId = InPlaceholderId;
	SynchronizeProperties();
}

void UALKThumbnail::SetCornerRadius(float InRadius)
{
	CornerRadius = FMath::Max(0.f, InRadius);
	SynchronizeProperties();
}

void UALKThumbnail::SetTint(const FLinearColor& InTint)
{
	Tint = InTint;
	SynchronizeProperties();
}

void UALKThumbnail::SetVeil(const FLinearColor& InVeil)
{
	Veil = InVeil;
	bThemeVeil = false;
	SynchronizeProperties();
}

void UALKThumbnail::SetThemeVeil(bool bInThemeVeil)
{
	bThemeVeil = bInThemeVeil;
	SynchronizeProperties();
}

#if WITH_EDITOR
const FText UALKThumbnail::GetPaletteCategory()
{
	return LOCTEXT("PaletteCategory", "Assetpro Lobby Kit");
}
#endif

#undef LOCTEXT_NAMESPACE
