// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "ALKUserWidget.generated.h"

class UALKTheme;
class UALKMenuConfig;
class UWidgetTree;

/**
 * Base for the plugin's non-screen widgets (root layout, setting rows).
 *
 * If a subclass is created in UMG with its own designer layout, that layout is used and the bound widget names are
 * looked up. Otherwise a default layout is generated in code from the active theme, so the plugin works with zero
 * assets.
 */
UCLASS(Abstract, Blueprintable)
class ASSETPROLOBBYKIT_API UALKUserWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Restyles the widget from the theme. Called on construct and whenever the theme changes. */
	UFUNCTION(BlueprintNativeEvent, Category = "Assetpro Menu")
	void ApplyTheme(const UALKTheme* Theme);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	const UALKTheme* GetTheme() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	const UALKMenuConfig* GetMenuConfig() const;

	/** True when the layout was generated in code rather than designed in UMG. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	bool IsUsingDefaultLayout() const { return bUsingDefaultLayout; }

	/** Called once after the code generated layout is built (not for designer layouts), so a Blueprint child can tweak it. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Menu")
	void OnDefaultLayoutBuilt();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Build the code generated layout. Only called when the widget has no designer layout. Must set Tree.RootWidget. */
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) {}

	/** Called after the layout exists (designer or generated). Bind events to named widgets here. */
	virtual void NativeOnLayoutReady() {}

	virtual void NativeApplyTheme(const UALKTheme& Theme) {}

	/**
	 * Restyle this widget from the theme. Untick in a UMG child if you styled everything in the designer and do not
	 * want the theme to override it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu")
	bool bApplyThemeStyles = true;

private:
	void HandleThemeChanged();

	FDelegateHandle ThemeChangedHandle;
	bool bUsingDefaultLayout = false;
};
