#pragma once
/**
 * @brief Basename of the current source file, without directory components.
 */
#define SHL_FILENAME (::sgl::detail::Basename(__FILE__))

#ifndef FILENAME
/**
 * @brief Legacy alias for @ref SHL_FILENAME. Prefer @ref SHL_FILENAME.
 * @deprecated Use SHL_FILENAME instead.
 */
#  define FILENAME SHL_FILENAME
#endif

#ifndef SGL_FILENAME
/**
 * @brief Alias used by the short logging macros.
 */
#  define SGL_FILENAME SHL_FILENAME
#endif

#define SLOGF (::sgl::Logger{::sgl::Logger::LogLevel::FATAL,   SGL_FILENAME, __LINE__})
#define SLOGE (::sgl::Logger{::sgl::Logger::LogLevel::ERROR,   SGL_FILENAME, __LINE__})
#define SLOGA (::sgl::Logger{::sgl::Logger::LogLevel::ALERT,   SGL_FILENAME, __LINE__})
#define SLOGW (::sgl::Logger{::sgl::Logger::LogLevel::WARNING, SGL_FILENAME, __LINE__})
#define SLOGI (::sgl::Logger{::sgl::Logger::LogLevel::INFO,    SGL_FILENAME, __LINE__})
#define SLOGD (::sgl::Logger{::sgl::Logger::LogLevel::DEBUG,   SGL_FILENAME, __LINE__})
#define SLOGT (::sgl::Logger{::sgl::Logger::LogLevel::TRACE,   SGL_FILENAME, __LINE__})

#include <array>
#include <atomic>
#include <charconv>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include "Types.hpp"

namespace sgl::detail {

  /**
   * @brief Returns the file name part of a path.
   * @param path Path or file name.
   * @return Substring after the last '/' or '\\'.
   */
  constexpr std::string_view Basename(const std::string_view path) noexcept {
    const auto separator = path.find_last_of("/\\");
    return separator == std::string_view::npos ? path : path.substr(separator + 1);
  }

} // namespace sgl::detail

namespace sgl {

  /**
 * @brief Formats a value into a new `std::string` using printf syntax.
 *
 * Example:
 * @code
 *   auto s = sgl::FormatString("%04i-%02i-%02i", 2026, 11, 25);
 * @endcode
 *
 * @tparam Args Argument types; must match the conversion specifiers.
 * @param strToFormat printf-style format string. Must be null-terminated.
 * @param args Values referenced by the format string.
 * @return The formatted string.
 * @throws std::runtime_error If the format string is invalid.
 * @warning The format string is not type checked. Passing a `std::string` to
 *          `%s` is undefined behaviour; use `.c_str()`.
 */
  template<typename... Args>
  std::string FormatString(const std::string_view strToFormat, const Args &...args) {
    if (strToFormat.data() == nullptr) throw std::runtime_error("FormatString: null format string");

    if constexpr (sizeof...(Args) == 0) {
      return std::string{strToFormat};
    } else {
      std::array<char, 256> stackBuffer{};
      const int needed = std::snprintf(stackBuffer.data(), stackBuffer.size(), strToFormat.data(), args...);
      if (needed < 0) throw std::runtime_error("FormatString: formatting failed");

      if (static_cast<std::size_t>(needed) < stackBuffer.size())
        return std::string{stackBuffer.data(), static_cast<std::size_t>(needed)};

      std::string result(static_cast<std::size_t>(needed), '\0');
      std::snprintf(result.data(), result.size() + 1, strToFormat.data(), args...);
      return result;
    }
  }

class Logger {
public:
  /**
   * @brief Severity of a log message, ordered from most to least severe.
   */
  enum class LogLevel : u8 {
    FATAL   = 0, ///< Unrecoverable error.
    ERROR   = 1, ///< Recoverable error.
    ALERT   = 2, ///< Action must be taken immediately.
    WARNING = 3, ///< Unexpected but non-critical condition.
    INFO    = 4, ///< Informational message.
    DEBUG   = 5, ///< Diagnostic message.
    TRACE   = 6, ///< Fine grained diagnostics.
    INVALID = 7  ///< Invalid/off value; disables all output when used as maximum.
  };

  /**
   * @brief Creates a logger for one statement.
   * @param logLevel Severity of the message.
   * @param file Source file name, usually @ref SHL_FILENAME.
   * @param line Source line, usually `__LINE__`.
   * @note The message is dropped immediately if @p logLevel is above the
   *       configured maximum level (see @ref SetMaxLogLevel).
   */
  Logger(LogLevel logLevel, std::string_view file, i32 line) noexcept;

  /**
   * @brief Emits the buffered line, if any.
   */
  ~Logger();

  Logger(const Logger &)            = delete;
  Logger &operator=(const Logger &) = delete;

  /**
   * @brief Logs a printf-style formatted message.
   * @param fmt Null-terminated printf format string.
   * @param args Values referenced by @p fmt.
   * @return `*this` for chaining.
   */
  template<typename... Args>
  Logger &operator()(const char *fmt, Args &&...args) {
    if (!active_ || fmt == nullptr) return *this;
    if constexpr (sizeof...(Args) == 0)
      append(std::string_view{fmt});
    else
      append(FormatString(fmt, std::forward<Args>(args)...));
    return *this;
  }

  /**
   * @brief Logs an already formatted message.
   * @param text Message text.
   * @return `*this` for chaining.
   */
  Logger &operator()(std::string_view text);

  /**
   * @brief Appends text to the current log line.
   * @param text Text to append.
   * @return `*this` for chaining.
   */
  Logger &operator<<(std::string_view text);

  /**
   * @brief Appends any streamable value to the current log line.
   *
   * Arithmetic values are converted with `std::to_chars` (no allocation);
   * strings and string-like types are appended directly. Other types fall
   * back to `operator<<` on a temporary stream.
   *
   * @tparam T Value type.
   * @param value Value to append.
   * @return `*this` for chaining.
   */
  template<typename T>
  Logger &operator<<(const T &value) {
    if (!active_) return *this;

    using U = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<U, char>) {
      append(value);
    } else if constexpr (std::is_same_v<U, bool>) {
      append(std::string_view{value ? "true" : "false"});
    } else if constexpr (std::is_integral_v<U> || std::is_floating_point_v<U>) {
      std::array<char, 64> buffer{};
      const auto           result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
      if (result.ec == std::errc{}) append(std::string_view{buffer.data(), static_cast<std::size_t>(result.ptr - buffer.data())});
    } else if constexpr (std::is_convertible_v<const U &, std::string_view>) {
      append(std::string_view{value});
    } else {
      std::ostringstream stream;
      stream << value;
      append(stream.str());
    }
    return *this;
  }

  /**
   * @brief Handles iostream manipulators such as `std::endl` and `std::flush`.
   *
   * The manipulator terminates the current line; other manipulators are
   * ignored because formatting is not stream based.
   *
   * @param manip Manipulator function.
   * @return `*this` for chaining.
   */
  Logger &operator<<(std::ostream &(*manip)(std::ostream &));

  /**
   * @brief Sets the maximum level that is emitted.
   * @param level Highest (most verbose) level to print. Messages with a
   *        numerically greater level are dropped. @ref LogLevel::INVALID
   *        disables all log output.
   */
  static void SetMaxLogLevel(LogLevel level) noexcept;

  /// @brief Returns the current maximum level (see @ref SetMaxLogLevel).
  [[nodiscard]] static LogLevel GetMaxLogLevel() noexcept;

  /**
   * @brief Checks whether messages of @p level are currently emitted.
   * @param level Level to test.
   * @return `true` if @p level is at or below the maximum level.
   */
  [[nodiscard]] static bool IsEnabled(LogLevel level) noexcept;

  /**
   * @brief Redirects log output.
   * @param stream Destination stream; `nullptr` restores `std::cout`.
   * @note The caller keeps ownership and must keep @p stream alive while it is
   *       installed.
   */
  static void SetOutputStream(std::ostream *stream) noexcept;

  /// @brief Returns the current output stream (`std::cout` by default).
  [[nodiscard]] static std::ostream *GetOutputStream() noexcept;

private:
  /// @brief Appends text to the internal buffer.
  void append(std::string_view text);
  /// @brief Appends a single character to the internal buffer.
  void append(char c);

  /// @brief Flushes the buffered line to the configured output stream.
  void flush();

  LogLevel         level_;  ///< Severity of this message.
  std::string_view file_;   ///< Source file; references static storage.
  i32              line_;   ///< Source line.
  bool             active_; ///< False if the level is filtered out.
  std::string      msg_;    ///< Buffered message text.

  static std::atomic<LogLevel>      sMaxLevel; ///< Current maximum level.
  static std::atomic<std::ostream *> sOutput;   ///< Current output stream.
  static std::mutex                 sMutex;     ///< Serialises line output.
};
}