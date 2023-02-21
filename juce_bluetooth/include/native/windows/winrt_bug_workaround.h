#pragma once

#include <winrt/base.h>

// Fun workaround for a circular dependency in the Windows headers:
// https://github.com/microsoft/Windows.UI.Composition-Win32-Samples/issues/47
namespace winrt::impl {
	template<typename Async>
	auto wait_for(Async const& async, Windows::Foundation::TimeSpan const& timeout);
}
