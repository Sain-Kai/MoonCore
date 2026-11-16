#pragma once
#include "MoonCore/Utility/Details/FormatIncludes.inl"

#define MOON_FMT_FORMATTER_VALUE() object
#define MOON_FMT_FORMATTER_MEMBER(member) object.member
#define MOON_FMT_FORMATTER_METHOD(method) object.method()

#define MOON_FMT_FORMATTER_STRUCT_TEMPLATE(chType, templateParameters, type, str, ...)          \
	template templateParameters														          \
    struct MC::External::fmt::formatter<type, chType>								          \
	{																				          \
	    constexpr auto parse(MC::External::fmt::parse_context<chType>& ctx) { return ctx.begin(); }	          \
	    template <typename FormatContext>											          \
	    auto format(const type& object, FormatContext& ctx) const noexcept \
		{																			          \
	        return format_to(ctx.out(), str, __VA_ARGS__);							          \
	    }																			          \
	};																					
#define MOON_FMT_FORMATTER(type, str, ...)									   \
    MOON_FMT_FORMATTER_STRUCT_TEMPLATE(char7, <>, type, str, __VA_ARGS__);	   \
    MOON_FMT_FORMATTER_STRUCT_TEMPLATE(char8, <>, type, TEXT(str), __VA_ARGS__);		
#define MOON_FMT_FORMATTER_TEMPLATE(templateParameters, type, str, ...)						   \
    MOON_FMT_FORMATTER_STRUCT_TEMPLATE(char7, templateParameters, type, str, __VA_ARGS__);	   \
    MOON_FMT_FORMATTER_STRUCT_TEMPLATE(char8, templateParameters, type, TEXT(str), __VA_ARGS__);	

#define MOON_FMT_FORMATTER_UTF7(type, str, ...) MOON_FMT_FORMATTER_STRUCT_TEMPLATE(char7, <>, type, str, __VA_ARGS__);
#define MOON_FMT_FORMATTER_UTF8(type, str, ...) MOON_FMT_FORMATTER_STRUCT_TEMPLATE(char8, <>, type, TEXT(str), __VA_ARGS__);