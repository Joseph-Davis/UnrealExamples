// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKPaint.h"
#include "Brushes/SlateColorBrush.h"
#include "CoreGlobals.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "TextureResource.h"
#include "Textures/SlateShaderResource.h"

namespace ALKPaintPrivate
{
	/** One brush per generated gradient; empty (no resource) until FALKPaint::PrepareGradients has run. */
	static FSlateBrush* GetGradientBrushes()
	{
		static FSlateBrush Brushes[static_cast<int32>(EALKGradient::Count)];
		return Brushes;
	}
	static bool bGradientsReady = false;

	/** A small texture filled pixel by pixel (white with an alpha ramp, mostly). */
	static UTexture2D* CreateGradientTexture(int32 Width, int32 Height, TFunctionRef<FColor(float, float)> Pixel)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = true;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;
		Texture->LODGroup = TEXTUREGROUP_UI;
		if (FTexturePlatformData* PlatformData = Texture->GetPlatformData(); PlatformData && PlatformData->Mips.Num() > 0)
		{
			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			if (FColor* Pixels = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE)))
			{
				for (int32 Y = 0; Y < Height; ++Y)
				{
					for (int32 X = 0; X < Width; ++X)
					{
						Pixels[Y * Width + X] = Pixel(Width > 1 ? X / static_cast<float>(Width - 1) : 0.f, Height > 1 ? Y / static_cast<float>(Height - 1) : 0.f);
					}
				}
			}
			Mip.BulkData.Unlock();
		}
		Texture->UpdateResource();
		// Shared by every frame for as long as the game runs: a few hundred bytes that are never collected.
		Texture->AddToRoot();
		return Texture;
	}

	static FColor WhiteWithAlpha(float Alpha)
	{
		return FColor(255, 255, 255, static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Alpha * 255.f), 0, 255)));
	}

	static const FSlateColorBrush& WhiteBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return Brush;
	}

	static void AddVertex(TArray<FSlateVertex>& Verts, const FSlateRenderTransform& Transform, const FVector2f& Position, const FColor& Color)
	{
		Verts.Add(FSlateVertex::Make(Transform, Position, FVector2f(0.f, 0.f), Color));
	}

	static void Draw(FSlateWindowElementList& Out, int32 Layer, const TArray<FSlateVertex>& Verts, const TArray<SlateIndex>& Indices)
	{
		if (Verts.Num() >= 3 && Indices.Num() >= 3)
		{
			// No texture: Slate draws the vertex colours over its white texture.
			FSlateDrawElement::MakeCustomVerts(Out, Layer, FSlateResourceHandle(), Verts, Indices, nullptr, 0, 0);
		}
	}

	static bool bReduced = false;
}

void FALKPaint::RoundedBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
	const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth)
{
	const bool bOutlined = Outline.A > 0.001f && OutlineWidth > 0.f;
	if ((Fill.A <= 0.001f && !bOutlined) || Max.X <= Min.X || Max.Y <= Min.Y)
	{
		return;
	}
	const FVector2f Size = Max - Min;
	const float R = FMath::Clamp(Radius, 0.f, FMath::Min(Size.X, Size.Y) * 0.5f);
	// Slate copies what it needs from the brush while the element is made, so a brush on the stack is fine.
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(FLinearColor::White);
	Brush.OutlineSettings.CornerRadii = FVector4(R, R, R, R);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.Color = FSlateColor(bOutlined ? Outline : FLinearColor::Transparent);
	Brush.OutlineSettings.Width = bOutlined ? OutlineWidth : 0.f;
	FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Min)), &Brush, ESlateDrawEffect::None, Fill);
}

void FALKPaint::Shadow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
	const FLinearColor& Color, const FVector2f& Offset, float Blur)
{
	if (Color.A <= 0.001f)
	{
		return;
	}
	if (Blur < 1.f)
	{
		RoundedBox(Out, Layer, Geometry, Min + Offset, Max + Offset, Radius, Color);
		return;
	}
	// Stacked boxes, the widest first: together they reach the shadow's full strength under the panel and thin out
	// towards the edge.
	constexpr int32 Steps = 8;
	const float StepAlpha = 1.f - FMath::Pow(1.f - FMath::Clamp(Color.A, 0.f, 0.98f), 1.f / Steps);
	const FLinearColor StepColor = Color.CopyWithNewOpacity(StepAlpha);
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		const float Grow = Blur * (1.f - static_cast<float>(Step) / (Steps - 1)) - Blur * 0.35f;
		const FVector2f Pad(Grow, Grow);
		RoundedBox(Out, Layer, Geometry, Min + Offset - Pad, Max + Offset + Pad, FMath::Max(0.f, Radius + Grow), StepColor);
	}
}

void FALKPaint::PrepareGradients()
{
	using namespace ALKPaintPrivate;
	if (bGradientsReady || !IsInGameThread() || GExitPurge)
	{
		return;
	}
	bGradientsReady = true;

	auto Set = [](EALKGradient Gradient, UTexture2D* Texture)
	{
		FSlateBrush& Brush = GetGradientBrushes()[static_cast<int32>(Gradient)];
		Brush = FSlateBrush();
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.Width = 0.f;
		if (Texture)
		{
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		}
	};

	// Eased ramps: a straight ramp shows where it starts and ends.
	Set(EALKGradient::FadeDown, CreateGradientTexture(4, 64, [](float U, float V) { return WhiteWithAlpha(FMath::Pow(1.f - V, 1.7f)); }));
	Set(EALKGradient::FadeUp, CreateGradientTexture(4, 64, [](float U, float V) { return WhiteWithAlpha(FMath::Pow(V, 1.7f)); }));
	Set(EALKGradient::FadeRight, CreateGradientTexture(64, 4, [](float U, float V) { return WhiteWithAlpha(0.1f + 0.9f * FMath::Pow(1.f - U, 1.8f)); }));
	Set(EALKGradient::Depth, CreateGradientTexture(4, 64, [](float U, float V)
	{
		// Light over the upper half, shade over the lower half, nothing in the middle.
		if (V < 0.5f)
		{
			return WhiteWithAlpha(FMath::Pow(1.f - V * 2.f, 1.5f));
		}
		FColor Shade = WhiteWithAlpha(0.85f * FMath::Pow(V * 2.f - 1.f, 1.5f));
		Shade.R = Shade.G = Shade.B = 0;
		return Shade;
	}));
}

void FALKPaint::GradientBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
	EALKGradient Gradient, const FLinearColor& Color)
{
	using namespace ALKPaintPrivate;
	if (Color.A <= 0.001f || Max.X <= Min.X || Max.Y <= Min.Y || Gradient >= EALKGradient::Count)
	{
		return;
	}
	const FSlateBrush& Shared = GetGradientBrushes()[static_cast<int32>(Gradient)];
	if (!Shared.GetResourceObject())
	{
		return;
	}
	const FVector2f Size = Max - Min;
	const float R = FMath::Clamp(Radius, 0.f, FMath::Min(Size.X, Size.Y) * 0.5f);
	FSlateBrush Brush = Shared;
	Brush.OutlineSettings.CornerRadii = FVector4(R, R, R, R);
	FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Min)), &Brush, ESlateDrawEffect::None, Color);
}

void FALKPaint::TopEdge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
	const FLinearColor& Color, float Inset)
{
	const FVector2f Pad(Inset, Inset);
	const FVector2f BoxMin = Min + Pad;
	const FVector2f BoxMax = Max - Pad;
	const FVector2f Size = BoxMax - BoxMin;
	if (Color.A <= 0.001f || Size.X < 4.f || Size.Y < 4.f)
	{
		return;
	}
	// The box's outline, cut off where its corners have turned a third of the way down: what is left is the top line
	// with its two ends curving away.
	const float R = FMath::Clamp(Radius - Inset, 0.f, FMath::Min(Size.X, Size.Y) * 0.5f);
	const float Band = FMath::Min(Size.Y, FMath::Max(1.5f, R * 0.3f + 1.f));
	Out.PushClip(FSlateClippingZone(Geometry.ToPaintGeometry(FVector2f(Size.X, Band), FSlateLayoutTransform(BoxMin))));
	RoundedBox(Out, Layer, Geometry, BoxMin, BoxMax, R, FLinearColor::Transparent, Color, 1.f);
	Out.PopClip();
}

void FALKPaint::Glow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
	const FLinearColor& Color, float Size)
{
	if (Color.A <= 0.001f || Size < 1.f || Max.X <= Min.X || Max.Y <= Min.Y)
	{
		return;
	}
	// Rings round the box, each as wide as it reaches: many overlap at the box's edge and few further out, so the glow
	// is strongest there and thins out. Nothing is drawn under the box itself.
	constexpr int32 Steps = 7;
	const float StepAlpha = 1.f - FMath::Pow(1.f - FMath::Clamp(Color.A, 0.f, 0.98f), 1.f / Steps);
	const FLinearColor StepColor = Color.CopyWithNewOpacity(StepAlpha);
	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		const float Grow = FMath::Max(1.f, Size * FMath::Pow(static_cast<float>(Step) / Steps, 1.7f));
		const FVector2f Pad(Grow, Grow);
		RoundedBox(Out, Layer, Geometry, Min - Pad, Max + Pad, Radius + Grow, FLinearColor::Transparent, StepColor, Grow);
	}
}

void FALKPaint::Circle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Center, float Radius,
	const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth)
{
	const FVector2f Half(Radius, Radius);
	RoundedBox(Out, Layer, Geometry, Center - Half, Center + Half, Radius, Fill, Outline, OutlineWidth);
}

void FALKPaint::Arc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Center, float Radius, float StartDegrees, float EndDegrees,
	const FLinearColor& Color, float Width)
{
	if (Color.A <= 0.001f || Radius <= 0.f || Width <= 0.f)
	{
		return;
	}
	const float Sweep = EndDegrees - StartDegrees;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(FMath::Abs(Sweep) / 10.f), 2, 48);
	TArray<FVector2f> Points;
	Points.Reserve(Steps + 1);
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		const float Angle = FMath::DegreesToRadians(StartDegrees + Sweep * Step / Steps);
		Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
	Lines(Out, Layer, Geometry, MoveTemp(Points), Color, Width);
}

void FALKPaint::FillConvex(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points, const FLinearColor& Color)
{
	using namespace ALKPaintPrivate;
	if (Points.Num() < 3 || Color.A <= 0.001f)
	{
		return;
	}
	const FSlateRenderTransform& Transform = Geometry.GetAccumulatedRenderTransform();
	const FColor Packed = Color.ToFColor(true);
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indices;
	Verts.Reserve(Points.Num());
	Indices.Reserve((Points.Num() - 2) * 3);
	for (const FVector2f& Point : Points)
	{
		AddVertex(Verts, Transform, Point, Packed);
	}
	for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
	{
		Indices.Add(0);
		Indices.Add(static_cast<SlateIndex>(Index));
		Indices.Add(static_cast<SlateIndex>(Index + 1));
	}
	Draw(Out, Layer, Verts, Indices);
}

void FALKPaint::FillRect(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, const FLinearColor& Color)
{
	if (Color.A <= 0.001f || Max.X <= Min.X || Max.Y <= Min.Y)
	{
		return;
	}
	FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Max - Min, FSlateLayoutTransform(Min)), &ALKPaintPrivate::WhiteBrush(), ESlateDrawEffect::None, Color);
}

void FALKPaint::Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Points, const FLinearColor& Color, float Width, bool bClosed)
{
	if (Points.Num() < 2 || Color.A <= 0.001f || Width <= 0.f)
	{
		return;
	}
	// A line with no length (an icon in a widget squeezed to no size, a point that is not a number) must not reach Slate:
	// it makes a draw batch first and then finds nothing to put in it, which the renderer treats as a fatal error.
	float Length = 0.f;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		Length += FVector2f::Distance(Points[Index - 1], Points[Index]);
	}
	if (!FMath::IsFinite(Length) || Length < 0.05f)
	{
		return;
	}
	if (bClosed)
	{
		// A copy: adding an element of the array to itself is not allowed when the add grows it.
		const FVector2f First = Points[0];
		Points.Add(First);
	}
	FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, true, Width);
}

float FALKPaint::Hash(int32 X, int32 Seed)
{
	uint32 H = static_cast<uint32>(X) * 374761393u + static_cast<uint32>(Seed) * 668265263u;
	H = (H ^ (H >> 13)) * 1274126177u;
	H ^= H >> 16;
	return static_cast<float>(H & 0xFFFFFFu) / static_cast<float>(0x1000000);
}

FALKQuadBatch::FALKQuadBatch(const FGeometry& InGeometry)
	: Transform(InGeometry.GetAccumulatedRenderTransform())
{
}

void FALKQuadBatch::AddQuad(const FVector2f& P0, const FVector2f& P1, const FVector2f& P2, const FVector2f& P3, const FLinearColor& Color)
{
	using namespace ALKPaintPrivate;
	// Indices may be 16 bit: one batch holds at most 16k quads (callers stay far below that).
	if (Color.A <= 0.001f || Verts.Num() + 4 > 65535)
	{
		return;
	}
	const SlateIndex Base = static_cast<SlateIndex>(Verts.Num());
	const FColor Packed = Color.ToFColor(true);
	AddVertex(Verts, Transform, P0, Packed);
	AddVertex(Verts, Transform, P1, Packed);
	AddVertex(Verts, Transform, P2, Packed);
	AddVertex(Verts, Transform, P3, Packed);
	Indices.Append({ Base, static_cast<SlateIndex>(Base + 1), static_cast<SlateIndex>(Base + 2), Base, static_cast<SlateIndex>(Base + 2), static_cast<SlateIndex>(Base + 3) });
}

void FALKQuadBatch::AddRect(const FVector2f& Min, const FVector2f& Max, const FLinearColor& Color)
{
	AddQuad(Min, FVector2f(Max.X, Min.Y), Max, FVector2f(Min.X, Max.Y), Color);
}

void FALKQuadBatch::Flush(FSlateWindowElementList& Out, int32 Layer)
{
	ALKPaintPrivate::Draw(Out, Layer, Verts, Indices);
	Verts.Reset();
	Indices.Reset();
}

bool FALKEffects::AreReduced()
{
	return ALKPaintPrivate::bReduced;
}

void FALKEffects::SetReduced(bool bInReduced)
{
	if (ALKPaintPrivate::bReduced != bInReduced)
	{
		ALKPaintPrivate::bReduced = bInReduced;
		OnChanged().Broadcast();
	}
}

double FALKEffects::Now()
{
	return FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetCurrentTime() : FPlatformTime::Seconds();
}

FSimpleMulticastDelegate& FALKEffects::OnChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}
