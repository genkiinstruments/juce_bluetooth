#pragma once

#include <juce_core/juce_core.h>
#include <fmt/format.h>

#ifndef GENKI_BLUETOOTH_LOG_ENABLED
#define GENKI_BLUETOOTH_LOG_ENABLED 1
#endif

#define LOG(text) JUCE_BLOCK_WITH_FORCED_SEMICOLON(if (GENKI_BLUETOOTH_LOG_ENABLED) { DBG(text); })
