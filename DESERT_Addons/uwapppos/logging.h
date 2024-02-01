#pragma once

#include <iostream>
#include <string_view>

namespace ConsoleColours
{

#ifdef _WIN32
#error Not implemented.
#else
	inline constexpr std::string_view Red = "\x1b[1;31m";
	inline constexpr std::string_view Green = "\x1b[1;32m";
	inline constexpr std::string_view Yellow = "\x1b[1;33m";
	inline constexpr std::string_view Blue = "\x1b[1;34m";
	inline constexpr std::string_view Magenta = "\x1b[1;35m";
	inline constexpr std::string_view Cyan = "\x1b[1;36m";
	inline constexpr std::string_view Reset = "\x1b[0m";
#endif

};

#define LOG_MSG(color, msg) std::cout << color << msg << ConsoleColours::Reset << endl;

#define LOG_MSG_ONCE(color, msg)       \
	{                                  \
		static bool msg_shown = false; \
		if (!msg_shown)                \
		{                              \
			msg_shown = true;          \
			LOG_MSG(color, msg)        \
		}                              \
	}

#define LOG_MSG_INFO(msg) LOG_MSG(ConsoleColours::Green, msg)
#define LOG_MSG_WARN(msg) LOG_MSG(ConsoleColours::Yellow, msg)
#define LOG_MSG_ERROR(msg) LOG_MSG(ConsoleColours::Red, msg)

#define LOG_MSG_INFO_ONCE(msg) LOG_MSG_ONCE(ConsoleColours::Green, msg)
#define LOG_MSG_WARN_ONCE(msg) LOG_MSG_ONCE(ConsoleColours::Yellow, msg)
#define LOG_MSG_ERROR_ONCE(msg) LOG_MSG_ONCE(ConsoleColours::Red, msg)
