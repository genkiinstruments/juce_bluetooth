#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <gsl/byte>

#if _WIN32
#pragma warning(push, 0)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#endif
#include <fmt/ranges.h>
#if _WIN32
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

template<>
struct fmt::formatter<juce::String>
{
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const juce::String& str, FormatContext& ctx) -> decltype(ctx.out()) { return fmt::format_to(ctx.out(), "{}", str.getCharPointer()); }
};

template<>
struct fmt::formatter<juce::ValueTree>
{
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const juce::ValueTree& vt, FormatContext& ctx) -> decltype(ctx.out())
    {
        juce::XmlElement::TextFormat xml_fmt;
        xml_fmt.addDefaultHeader = false;

        return fmt::format_to(ctx.out(), "{}", vt.toXmlString(xml_fmt));
    }
};

template<>
struct fmt::formatter<juce::Identifier>
{
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const juce::Identifier& id, FormatContext& ctx) -> decltype(ctx.out()) { return fmt::format_to(ctx.out(), "{}", id.toString()); }
};

template<>
struct fmt::formatter<gsl::byte>
{
    constexpr auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const gsl::byte& byte, FormatContext& ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{:02x}", static_cast<const unsigned char>(byte));
    }
};

template<>
struct fmt::formatter<juce::Uuid>
{
    bool dashed = false;

    auto parse(fmt::format_parse_context& ctx) -> decltype(ctx.begin())
    {
        auto it = ctx.begin();

        if (it != ctx.end() && *it++ == '-')
            dashed = true;

        jassert(it == ctx.end());

        return it;
    }

    template<typename FormatContext>
    auto format(const juce::Uuid& uuid, FormatContext& ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{}", dashed ? uuid.toDashedString() : uuid.toString());
    }
};
