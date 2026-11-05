// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKRootLayout.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "CommonActivatableWidget.h"
#include "Components/Overlay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKRootLayout)

void UALKRootLayout::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("RootOverlay"));
	Tree.RootWidget = Root;

	GameLayer = FALKWidgetBuilder::Make<UCommonActivatableWidgetStack>(Tree, TEXT("GameLayer"));
	GameMenuLayer = FALKWidgetBuilder::Make<UCommonActivatableWidgetStack>(Tree, TEXT("GameMenuLayer"));
	MenuLayer = FALKWidgetBuilder::Make<UCommonActivatableWidgetStack>(Tree, TEXT("MenuLayer"));
	ModalLayer = FALKWidgetBuilder::Make<UCommonActivatableWidgetStack>(Tree, TEXT("ModalLayer"));

	for (UCommonActivatableWidgetStack* Stack : { GameLayer.Get(), GameMenuLayer.Get(), MenuLayer.Get(), ModalLayer.Get() })
	{
		FALKWidgetBuilder::AddToOverlay(Root, Stack);
	}
}

void UALKRootLayout::NativeOnLayoutReady()
{
	for (UCommonActivatableWidgetStack* Stack : { GameLayer.Get(), GameMenuLayer.Get(), MenuLayer.Get(), ModalLayer.Get() })
	{
		if (Stack)
		{
			// Screens animate themselves, so the stack switches instantly.
			Stack->SetTransitionDuration(0.f);
			Stack->OnDisplayedWidgetChanged().AddUObject(this, &ThisClass::HandleDisplayedWidgetChanged);
		}
	}
}

UCommonActivatableWidgetContainerBase* UALKRootLayout::GetLayer(EALKLayer Layer) const
{
	switch (Layer)
	{
	case EALKLayer::Game: return GameLayer;
	case EALKLayer::GameMenu: return GameMenuLayer;
	case EALKLayer::Menu: return MenuLayer;
	case EALKLayer::Modal: return ModalLayer;
	default: return nullptr;
	}
}

UCommonActivatableWidget* UALKRootLayout::PushWidget(EALKLayer Layer, TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (UCommonActivatableWidgetContainerBase* Container = GetLayer(Layer))
	{
		return Container->AddWidget<UCommonActivatableWidget>(WidgetClass);
	}
	return nullptr;
}

bool UALKRootLayout::IsAnyMenuOpen() const
{
	for (UCommonActivatableWidgetContainerBase* Container : { static_cast<UCommonActivatableWidgetContainerBase*>(GameMenuLayer), static_cast<UCommonActivatableWidgetContainerBase*>(MenuLayer), static_cast<UCommonActivatableWidgetContainerBase*>(ModalLayer) })
	{
		if (Container && Container->GetNumWidgets() > 0)
		{
			return true;
		}
	}
	return false;
}

UCommonActivatableWidget* UALKRootLayout::GetTopWidget() const
{
	for (UCommonActivatableWidgetContainerBase* Container : { static_cast<UCommonActivatableWidgetContainerBase*>(ModalLayer), static_cast<UCommonActivatableWidgetContainerBase*>(MenuLayer), static_cast<UCommonActivatableWidgetContainerBase*>(GameMenuLayer), static_cast<UCommonActivatableWidgetContainerBase*>(GameLayer) })
	{
		if (Container)
		{
			if (UCommonActivatableWidget* Active = Container->GetActiveWidget())
			{
				return Active;
			}
		}
	}
	return nullptr;
}

void UALKRootLayout::ClearMenuLayers()
{
	for (UCommonActivatableWidgetContainerBase* Container : { static_cast<UCommonActivatableWidgetContainerBase*>(ModalLayer), static_cast<UCommonActivatableWidgetContainerBase*>(MenuLayer), static_cast<UCommonActivatableWidgetContainerBase*>(GameMenuLayer) })
	{
		if (Container)
		{
			Container->ClearWidgets();
		}
	}
}

void UALKRootLayout::HandleDisplayedWidgetChanged(UCommonActivatableWidget* Widget)
{
	OnLayersChanged.Broadcast();
}
