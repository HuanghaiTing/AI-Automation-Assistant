#pragma once
#include "pch.h"
#include <winrt/Microsoft.UI.Xaml.h>

namespace winrt::ModernUI::implementation {
	struct App : winrt::implements<App, winrt::Microsoft::UI::Xaml::ApplicationT<App>> {
		App();
	};
}
namespace winrt::ModernUI::factory_implementation {
	struct App : AppT<App, implementation::App> {};
}
