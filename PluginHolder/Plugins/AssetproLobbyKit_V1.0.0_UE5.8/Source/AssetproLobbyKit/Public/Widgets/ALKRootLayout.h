// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKUserWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "ALKTypes.h"
#include "ALKRootLayout.generated.h"

class UCommonActivatableWidget;

/**
 * The one widget added to the viewport per player. It owns a stack per layer (Game, GameMenu, Menu, Modal); every
 * screen is pushed into one of them.
 *
 * A UMG child can lay the stacks out itself: add Common Activatable Widget Stacks named GameLayer, GameMenuLayer,
 * MenuLayer and ModalLayer.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKRootLayout : public UALKUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Layout")
	UCommonActivatableWidgetContainerBase* GetLayer(EALKLayer Layer) const;

	/** Pushes a new instance of WidgetClass onto a layer. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Layout", meta = (DeterminesOutputType = "WidgetClass"))
	UCommonActivatableWidget* PushWidget(EALKLayer Layer, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	template <typename TWidget>
	TWidget* PushWidget(EALKLayer Layer, TSubclassOf<UCommonActivatableWidget> WidgetClass, TFunctionRef<void(TWidget&)> InitFunc);

	/** True if any menu layer (GameMenu, Menu, Modal) has a widget. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Layout")
	bool IsAnyMenuOpen() const;

	/** The top-most widget of the highest non-empty layer. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Layout")
	UCommonActivatableWidget* GetTopWidget() const;

	/** Removes every widget from every menu layer (not the Game layer). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Layout")
	void ClearMenuLayers();

	/** Fired whenever a layer shows a different widget. */
	FSimpleMulticastDelegate OnLayersChanged;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> GameLayer;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> GameMenuLayer;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> MenuLayer;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Layout", meta = (BindWidgetOptional))
	TObjectPtr<UCommonActivatableWidgetStack> ModalLayer;

private:
	void HandleDisplayedWidgetChanged(UCommonActivatableWidget* Widget);
};

template <typename TWidget>
TWidget* UALKRootLayout::PushWidget(EALKLayer Layer, TSubclassOf<UCommonActivatableWidget> WidgetClass, TFunctionRef<void(TWidget&)> InitFunc)
{
	if (UCommonActivatableWidgetContainerBase* Container = GetLayer(Layer))
	{
		return Container->AddWidget<TWidget>(WidgetClass, InitFunc);
	}
	return nullptr;
}
