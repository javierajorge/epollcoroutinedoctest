/*******************************************************************************
 * RetroShare debugging utilities                                              *
 *                                                                             *
 * Copyright (C) 2004-2008  Robert Fernie <retroshare@lunamutt.com>            *
 * Copyright (C) 2019-2022  Gioacchino Mazzurco <gio@eigenlab.org>             *
 * Copyright (C) 2020-2022  Asociación Civil Altermundi <info@altermundi.net>  *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Lesser General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Lesser General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/
#pragma once

#include <string>
#include <sstream>
#include <system_error>

#ifdef __ANDROID__
#include <android/log.h>
#else // def __ANDROID__
#include <iostream>
#include <chrono>
#include <iomanip>
#endif // def __ANDROID__

#ifdef __ANDROID__
enum class RsLoggerCategories
{
	DEBUG = ANDROID_LOG_DEBUG,
	INFO = ANDROID_LOG_INFO,
	WARNING = ANDROID_LOG_WARN,
	ERROR = ANDROID_LOG_ERROR,
	FATAL = ANDROID_LOG_FATAL
};
#else  // def __ANDROID__
enum class RsLoggerCategories
{
	DEBUG = 'D',
	INFO = 'I',
	WARNING = 'W',
	ERROR = 'E',
	FATAL = 'F'
};
#endif // def __ANDROID__

/** Stream helper for std::error_condition */
std::ostream &operator<<(std::ostream &out, const std::error_condition &err);

/** Provide unkown error message for all error categories to avoid duplicating
 * the message around */
std::string rsErrorNotInCategory(int errNum, const std::string &categoryName);

/** Convert C errno codes to modern C++11 std::error_condition, this is quite
 * useful to use toghether with C functions used around the code like `malloc`,
 * `socket` etc to let errors bubble up comprensibly to upper layers C++11 code
 */
std::error_condition rs_errno_to_condition(int errno_code);

template <RsLoggerCategories CATEGORY>
struct t_RsLogger : std::ostringstream
{
	t_RsLogger() { setPrefix(); }
	~t_RsLogger() { flush(); }

	/** Offer variadic style, this doesn't supports things like std::endl as
	 * paramether but when used toghether with conditional debugging macros
	 * reduces binary size as paramethers of suppressed calls are not evaluated
	 * and literally disappear in preprocessing fase @see RsDbg */
	template <typename... Args>
	explicit inline t_RsLogger(Args &&...args)
	{
		setPrefix();

		/* Combine initializer list and comma operator so the compiler unpack
		 * template arguments and feed our own stream without recursion
		 * see https://stackoverflow.com/a/27375675 */
		using expander = int[];
		(void)expander{0, (void((*this) << std::forward<Args>(args)), 0)...};
	}

	/** Dump buffer stream to log */
	void flush()
	{
#ifdef __ANDROID__
		__android_log_write(
			static_cast<int>(CATEGORY),
			"RetroShare", str().c_str());
#else  // def __ANDROID__
		(*this) << std::endl;
		std::cerr << str();
#endif // def __ANDROID__
		str() = "";
	}

private:
#ifdef __ANDROID__
	inline void setPrefix()
	{
	}
#else  // def __ANDROID__
	void setPrefix()
	{
		using namespace std::chrono;
		const auto now = system_clock::now();
		const auto sec = time_point_cast<seconds>(now);
		const auto msec = duration_cast<milliseconds>(now - sec);
		(*this) << static_cast<char>(CATEGORY) << " "
				<< sec.time_since_epoch().count() << "."
				<< std::setfill('0') << std::setw(3) << msec.count() << " ";
	}
#endif // def __ANDROID__
};

/**
 * Comfortable debug message logging, supports both variadic style and chaining
 * style like std::cerr.
 * Can be easly and selectively disabled at compile time.
 * To reduce generated binary size and performance impact when debugging is
 * disabled without too many \#ifdef around the code combining the variadic
 * style with the leveled debugging macros is the way to go.
 *
 * To selectively debug your file you just need to include the header of desired
 * debugging level (0 to 4)
@code{.cpp}
#include "util/rsdebuglevel2.h"
@endcode
 * Then where you want to print debug messages use
@code{.cpp}
RS_DBG0("Hello 0 ", "my debug ", my_variable) << " message " << variable2;
RS_DBG1("Hello 1 ", "my debug ", my_variable) << " message " << variable2;
RS_DBG2("Hello 2 ", "my debug ", my_variable) << " message " << variable2;
RS_DBG3("Hello 3 ", "my debug ", my_variable) << " message " << variable2;
RS_DBG4("Hello 4 ", "my debug ", my_variable) << " message " << variable2;
@endcode
 * To change the debugging level just include a different level header like
 * `util/rsdebuglevel1.h`, debug messages with lower or equal level then the
 * included header will be printed, the others will not.
 * Remember then on messages with debug level higher then the included the
 * paramethers you pass as macro arguments (variadic style) will disappear in
 * the preprocessing phase, so their evaluation will not be included in the
 * final binary and not executed at runtime, instead the paramether passed with
 * `<<` (chaining style) will be in the compiled binary and evaluated at runtime
 * even if are not printed, due to how C++ is made it is not possible to avoid
 * this, so we suggest to use variadic style for debug messages.
 */
using RsDbg = t_RsLogger<RsLoggerCategories::DEBUG>;
#define RS_DBG(...) RsDbg(__PRETTY_FUNCTION__, " ", __VA_ARGS__)

/**
 * Comfortable log information reporting helper, supports chaining like
 * std::cerr.
 * To report an information message you can just write:
@code{.cpp}
RsInfo() << __PRETTY_FUNCTION__ << "My information message" << std::cerr;
@endcode
 */
using RsInfo = t_RsLogger<RsLoggerCategories::INFO>;
#define RS_INFO(...) RsInfo(__PRETTY_FUNCTION__, " ", __VA_ARGS__)

/// Similar to @see RsInfo but for warning messages
using RsWarn = t_RsLogger<RsLoggerCategories::WARNING>;
#define RS_WARN(...) RsWarn(__PRETTY_FUNCTION__, " ", __VA_ARGS__)

/// Similar to @see RsInfo but for error messages
using RsErr = t_RsLogger<RsLoggerCategories::ERROR>;
#define RS_ERR(...) RsErr(__PRETTY_FUNCTION__, " ", __VA_ARGS__)

/** Similar to @see RsInfo but for fatal errors (the ones which cause RetroShare
 * to terminate) messages */
using RsFatal = t_RsLogger<RsLoggerCategories::FATAL>;
#define RS_FATAL(...) RsFatal(__PRETTY_FUNCTION__, " ", __VA_ARGS__)

/**
 * Keeps compatible syntax with RsDbg but explicitely do nothing in a way that
 * any modern compiler should be smart enough to optimize out all the function
 * calls.
 */
struct RsNoDbg
{
	inline RsNoDbg() = default;
	template <typename... Args>
	inline explicit RsNoDbg(Args...) {}

	/** This match most of the types, but might be not enough for templated
	 * types */
	template <typename T>
	inline RsNoDbg &operator<<(const T &) { return *this; }

	/// needed for manipulators and things like std::endl
	inline RsNoDbg &operator<<(std::ostream &(* /*pf*/)(std::ostream &))
	{
		return *this;
	}

	/** Do nothing. Just for code compatibility with other logging classes */
	inline void flush() {}
};

// From https://codereview.stackexchange.com/a/165162
/**
 * @brief hex_dump: Send Hexadecimal Dump to stream
 * @param os: Output Stream
 * @param buffer: Buffer to send
 * @param bufsize: Buffer's size
 * @param showPrintableChars: If must send printable Char too
 * @return
 * basic string:
 * 61 62 63 64 65 66 31 32  | abcdef12
 * 33 34 35 36 00 7a 79 78  | 3456.zyx
 * 77 76 75 39 38 37 36 35  | wvu98765
 * 34 45 64 77 61 72 64 00  | 4Edward.
 *
 * wide string:
 * 41 00 00 00 20 00 00 00  | A... ...
 * 77 00 00 00 69 00 00 00  | w...i...
 * 64 00 00 00 65 00 00 00  | d...e...
 * 20 00 00 00 73 00 00 00  |  ...s...
 * 74 00 00 00 72 00 00 00  | t...r...
 * 69 00 00 00 6e 00 00 00  | i...n...
 * 67 00 00 00 2e 00 00 00  | g.......
 *
 * a double
 * 49 92 24 49 92 24 09 40  | I.$I.$.@
 */
std::ostream &hex_dump(std::ostream &os, const void *buffer,
					   std::size_t bufsize, bool showPrintableChars = true);

/**
 * @brief The hexDump struct
 * Enable to print dump calling like that:
 * const char test[] = "abcdef123456\0zyxwvu987654Edward";
 * RsDbg()<<hexDump(test, sizeof(test))<<std::endl;
 */
struct hexDump
{
	const void *buffer;
	std::size_t bufsize;
	hexDump(const void *buf, std::size_t bufsz) : buffer{buf}, bufsize{bufsz} {}
	friend std::ostream &operator<<(std::ostream &out, const hexDump &hd)
	{
		return hex_dump(out, hd.buffer, hd.bufsize, true);
	}
};

/**
 * @def rs_error_bubble_or_exit
 * Bubbling up an error condition to be handled upstream if possible or dealing
 * it fatally here, is a very common pattern, @see rs_malloc as an example, so
 * instead of rewriting the same snippet over and over, increasing the
 * possibility of introducing bugs, use this macro to properly deal with that
 * situation.
 * @param p_error_condition expect something convertible to an
 *	std::error_condition to be dealt with
 * @param p_bubble_storage pointer to a location to store the
 *	std::error_condition to be bubbled up upstream, if it is nullptr the error
 *	will be handled with a fatal report end then exiting here
 * @param ... optional additional information you want to be printed toghether
 *	with the error report when is fatal (aka not bubbled up) */
#define rs_error_bubble_or_exit(p_error_condition, p_bubble_storage, ...) \
	if (p_bubble_storage)                                                 \
	{                                                                     \
		*p_bubble_storage = p_error_condition;                            \
	}                                                                     \
	else                                                                  \
	{                                                                     \
		RS_FATAL(p_error_condition, " " RS_OPT_VA_ARGS(__VA_ARGS__));     \
		print_stacktrace();                                               \
		exit(std::error_condition(p_error_condition).value());            \
	}
