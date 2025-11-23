#pragma once

namespace pserv
{
	namespace console
	{
#ifdef USE_VIRTUAL_CONSOLE_COMMANDS
#define VESC_SEQUENCE(__VESC_CODE__) "\x1b[" #__VESC_CODE__ "m"
#define CONSOLE_STANDARD                    VESC_SEQUENCE(0)	// Default	Returns all attributes to the default state prior to modification
#define CONSOLE_BOLD                        VESC_SEQUENCE(1)	// Bold / Bright	Applies brightness / intensity flag to foreground color
#define CONSOLE_NO_BOLD                     VESC_SEQUENCE(22)   //	No bold / bright	Removes brightness / intensity flag from foreground color
#define CONSOLE_UNDERLINE                   VESC_SEQUENCE(4)    // Underline	Adds underline
#define CONSOLE_NO_UNDERLINE                VESC_SEQUENCE(24)	// No underline	Removes underline
#define CONSOLE_NEGATIVE                    VESC_SEQUENCE(7)    // Negative	Swaps foreground and background colors
#define CONSOLE_POSITIVE                    VESC_SEQUENCE(27)	// Positive(No negative)	Returns foreground / background to normal
#define CONSOLE_FOREGROUND_BLACK            VESC_SEQUENCE(30)	// Foreground Black	Applies non - bold / bright black to foreground
#define CONSOLE_FOREGROUND_RED              VESC_SEQUENCE(31)	// Foreground Red	Applies non - bold / bright red to foreground
#define CONSOLE_FOREGROUND_GREEN            VESC_SEQUENCE(32)	// Foreground Green	Applies non - bold / bright green to foreground
#define CONSOLE_FOREGROUND_YELLOW           VESC_SEQUENCE(33)	// Foreground Yellow	Applies non - bold / bright yellow to foreground
#define CONSOLE_FOREGROUND_BLUE             VESC_SEQUENCE(34)	// Foreground Blue	Applies non - bold / bright blue to foreground
#define CONSOLE_FOREGROUND_MAGENTA          VESC_SEQUENCE(35)	// Foreground Magenta	Applies non - bold / bright magenta to foreground
#define CONSOLE_FOREGROUND_CYAN             VESC_SEQUENCE(36)	// Foreground Cyan	Applies non - bold / bright cyan to foreground
#define CONSOLE_FOREGROUND_WHITE            VESC_SEQUENCE(37)	// Foreground White	Applies non - bold / bright white to foreground
#define CONSOLE_FOREGROUND_DEFAULT          VESC_SEQUENCE(39)	// Foreground Default	Applies only the foreground portion of the defaults(see 0)
#define CONSOLE_BACKGROUND_BLACK            VESC_SEQUENCE(40)  // Background Black	Applies non - bold / bright black to background
#define CONSOLE_BACKGROUND_RED              VESC_SEQUENCE(41)  // Background Red	Applies non - bold / bright red to background
#define CONSOLE_BACKGROUND_GREEN            VESC_SEQUENCE(42)  // Background Green	Applies non - bold / bright green to background
#define CONSOLE_BACKGROUND_YELLOW           VESC_SEQUENCE(43)  // Background Yellow	Applies non - bold / bright yellow to background
#define CONSOLE_BACKGROUND_BLUE             VESC_SEQUENCE(44)  // Background Blue	Applies non - bold / bright blue to background
#define CONSOLE_BACKGROUND_MAGENTA          VESC_SEQUENCE(45)  // Background Magenta	Applies non - bold / bright magenta to background
#define CONSOLE_BACKGROUND_CYAN             VESC_SEQUENCE(46)  // Background Cyan	Applies non - bold / bright cyan to background
#define CONSOLE_BACKGROUND_WHITE            VESC_SEQUENCE(47)  // Background White	Applies non - bold / bright white to background
#define CONSOLE_BACKGROUND_DEFAULT          VESC_SEQUENCE(49)  // Background Default	Applies only the background portion of the defaults(see 0)
#define CONSOLE_FOREGROUND_BRIGHT_BLACK     VESC_SEQUENCE(90)  // Bright Foreground Black	Applies bold / bright black to foreground
#define CONSOLE_FOREGROUND_BRIGHT_RED       VESC_SEQUENCE(91)  // Bright Foreground Red	Applies bold / bright red to foreground
#define CONSOLE_FOREGROUND_BRIGHT_GREEN     VESC_SEQUENCE(92)  // Bright Foreground Green	Applies bold / bright green to foreground
#define CONSOLE_FOREGROUND_BRIGHT_YELLOW    VESC_SEQUENCE(93)  // Bright Foreground Yellow	Applies bold / bright yellow to foreground
#define CONSOLE_FOREGROUND_BRIGHT_BLUE      VESC_SEQUENCE(94)  // Bright Foreground Blue	Applies bold / bright blue to foreground
#define CONSOLE_FOREGROUND_BRIGHT_MAGENTA   VESC_SEQUENCE(95)  // Bright Foreground Magenta	Applies bold / bright magenta to foreground
#define CONSOLE_FOREGROUND_BRIGHT_CYAN      VESC_SEQUENCE(96)  // Bright Foreground Cyan	Applies bold / bright cyan to foreground
#define CONSOLE_FOREGROUND_BRIGHT_WHITE     VESC_SEQUENCE(97)  // Bright Foreground White	Applies bold / bright white to foreground
#define CONSOLE_BACKGROUND_BRIGHT_BLACK     VESC_SEQUENCE(100)	Bright Background Black	Applies bold / bright black to background
#define CONSOLE_BACKGROUND_BRIGHT_RED       VESC_SEQUENCE(101)	Bright Background Red	Applies bold / bright red to background
#define CONSOLE_BACKGROUND_BRIGHT_GREEN     VESC_SEQUENCE(102)	Bright Background Green	Applies bold / bright green to background
#define CONSOLE_BACKGROUND_BRIGHT_YELLOW    VESC_SEQUENCE(103)	Bright Background Yellow	Applies bold / bright yellow to background
#define CONSOLE_BACKGROUND_BRIGHT_BLUE      VESC_SEQUENCE(104)	Bright Background Blue	Applies bold / bright blue to background
#define CONSOLE_BACKGROUND_BRIGHT_MAGENTA   VESC_SEQUENCE(105)	Bright Background Magenta	Applies bold / bright magenta to background
#define CONSOLE_BACKGROUND_BRIGHT_CYAN      VESC_SEQUENCE(106)	Bright Background Cyan	Applies bold / bright cyan to background
#define CONSOLE_BACKGROUND_BRIGHT_WHITE     VESC_SEQUENCE(107)	Bright Background White	Applies bold / bright white to background

#else

#define CONSOLE_FOREGROUND_BRIGHT_BLACK "\x1b\x00"
#define CONSOLE_FOREGROUND_BRIGHT_BLUE "\x1b\x09"
#define CONSOLE_FOREGROUND_BRIGHT_CYAN "\x1b\x0b"
#define CONSOLE_FOREGROUND_BLUE "\x1b\x01"
#define CONSOLE_FOREGROUND_CYAN "\x1b\x03"
#define CONSOLE_FOREGROUND_GRAY "\x1b\x07"
#define CONSOLE_FOREGROUND_GREEN "\x1b\x02"
#define CONSOLE_FOREGROUND_MAGENTA "\x1b\x05"
#define CONSOLE_FOREGROUND_RED "\x1b\x04"
#define CONSOLE_FOREGROUND_YELLOW "\x1b\x06"
#define CONSOLE_FOREGROUND_BRIGHT_GRAY "\x1b\x08"
#define CONSOLE_FOREGROUND_BRIGHT_GREEN "\x1b\x0a"
#define CONSOLE_FOREGROUND_BRIGHT_INTENSITY "\x1b\x08"
#define CONSOLE_FOREGROUND_BRIGHT_MAGENTA "\x1b\x0d"
#define CONSOLE_FOREGROUND_BRIGHT_RED "\x1b\x0c"
#define CONSOLE_FOREGROUND_BRIGHT_WHITE "\x1b\x0f"
#define CONSOLE_FOREGROUND_BRIGHT_YELLOW "\x1b\x0e"

#define CONSOLE_STANDARD "\x1b\xFF"
#endif

		std::string encode_as_output_bytes(std::wstring_view text);
		bool ensure_output_handle();
		bool write(std::wstring_view utf16_encoded_string);
		bool write(std::string_view utf8_encoded_string);

		template <typename T>
		bool write_line(T text)
		{
            return write(std::string{text} + "\n");
		}

		template <typename... Args>
		constexpr bool format_line(std::string_view text, Args&&... args)
		{
			return write_line(std::vformat(text, std::make_format_args(args...)));
		}
	};
}
