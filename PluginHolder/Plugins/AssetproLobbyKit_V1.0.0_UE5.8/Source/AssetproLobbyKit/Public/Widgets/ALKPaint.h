// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Rendering/RenderingCommon.h"

class FSlateWindowElementList;

/** The generated gradients a rounded box can be filled with (see FALKPaint::GradientBox). */
enum class EALKGradient : uint8
{
	/** Solid at the top, fading out towards the bottom. */
	FadeDown,
	/** Solid at the bottom, fading out towards the top: the veil under a picture. */
	FadeUp,
	/** Strongest at the left, thinning out to the right: the tint of a focused row. */
	FadeRight,
	/** White at the top fading out, black at the bottom fading in: laid thinly over a fill it reads as a lit surface. */
	Depth,
	Count
};

/**
 * Drawing helpers for the kit's look: rounded boxes with outlines, soft and hard shadows, circles, arcs and lines.
 * Everything is drawn in the widget's local space with solid colours (the gradients use tiny textures made in code),
 * so it stays sharp at any size and follows the theme's colours. Handy for your own Slate widgets too.
 */
struct ASSETPROLOBBYKIT_API FALKPaint
{
	/** A box with rounded corners, filled and outlined. A transparent fill draws the outline alone. */
	static void RoundedBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
		const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);

	/**
	 * The shadow of a rounded box, moved by Offset. Blur 0 gives a hard copy of the box (a chunky, printed look); a
	 * larger Blur spreads it softly.
	 */
	static void Shadow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
		const FLinearColor& Color, const FVector2f& Offset, float Blur);

	/**
	 * A rounded box filled with one of the generated gradients in Color (its alpha sets the strength). Drawn over a fill
	 * it gives a sheen, a tint or a veil that keeps the rounded corners. Draws nothing until Prepare Gradients has run.
	 */
	static void GradientBox(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
		EALKGradient Gradient, const FLinearColor& Color);

	/**
	 * A one pixel line along the top of a rounded box that follows its corners a little way down: the lit edge of a
	 * panel. Inset moves it inside the box (past an outline).
	 */
	static void TopEdge(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
		const FLinearColor& Color, float Inset = 0.f);

	/**
	 * A soft glow round a rounded box, spreading Size pixels. It is drawn under the box, so a see-through fill is tinted
	 * by it.
	 */
	static void Glow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, float Radius,
		const FLinearColor& Color, float Size);

	/**
	 * Makes the gradient textures Gradient Box draws (once; they are tiny and shared). Call it from the game thread
	 * while a widget is built, never while painting.
	 */
	static void PrepareGradients();

	/** A filled and outlined circle. */
	static void Circle(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Center, float Radius,
		const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);

	/** Part of a circle's outline, from StartDegrees to EndDegrees (0 points right, 90 down). */
	static void Arc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Center, float Radius, float StartDegrees, float EndDegrees,
		const FLinearColor& Color, float Width);

	/** Fills a convex polygon with a solid colour. */
	static void FillConvex(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const TArray<FVector2f>& Points, const FLinearColor& Color);

	/** Fills an axis-aligned rectangle. */
	static void FillRect(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2f& Min, const FVector2f& Max, const FLinearColor& Color);

	/** Draws an antialiased polyline; bClosed joins the last point back to the first. */
	static void Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, TArray<FVector2f> Points, const FLinearColor& Color, float Width, bool bClosed = false);

	/** Integer hash to [0, 1), stable across frames (for patterns and placeholder colours). */
	static float Hash(int32 X, int32 Seed);
};

/** Collects many solid quads (a dot pattern, bars) and draws them in one call. */
struct ASSETPROLOBBYKIT_API FALKQuadBatch
{
	explicit FALKQuadBatch(const FGeometry& InGeometry);

	/** A quad with corners in the widget's local space, in order around it. */
	void AddQuad(const FVector2f& P0, const FVector2f& P1, const FVector2f& P2, const FVector2f& P3, const FLinearColor& Color);

	void AddRect(const FVector2f& Min, const FVector2f& Max, const FLinearColor& Color);

	/** Draws everything added so far on Layer and empties the batch. */
	void Flush(FSlateWindowElementList& Out, int32 Layer);

	int32 Num() const { return Verts.Num() / 4; }

private:
	FSlateRenderTransform Transform;
	TArray<FSlateVertex> Verts;
	TArray<SlateIndex> Indices;
};

/**
 * Motion shared by every widget: screens and panels that rise in, the drifting backdrop, pulsing indicators. The Reduce
 * Effects setting (Settings > Accessibility) turns the motion off; things then simply appear.
 */
struct ASSETPROLOBBYKIT_API FALKEffects
{
	/** True when Reduce Effects is on. */
	static bool AreReduced();

	/** Set by the settings subsystem from the Reduce Effects setting. */
	static void SetReduced(bool bInReduced);

	/** Seconds on the Slate clock: keeps running while the game is paused. */
	static double Now();

	/** Fired when Reduce Effects changes, so widgets that animate by themselves (the backdrop) update. */
	static FSimpleMulticastDelegate& OnChanged();
};
