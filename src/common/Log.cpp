#include <sgl/Log.hpp>

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <ostream>
#include <string>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  ifdef ERROR
#    undef ERROR
#  endif
#endif

namespace sgl {
namespace {

const char *LevelName(const Logger::LogLevel level) noexcept {
  switch (level) {
    case Logger::LogLevel::FATAL:   return "FATAL";
    case Logger::LogLevel::ERROR:   return "ERROR";
    case Logger::LogLevel::ALERT:   return "ALERT";
    case Logger::LogLevel::WARNING: return "WARN ";
    case Logger::LogLevel::INFO:    return "INFO ";
    case Logger::LogLevel::DEBUG:   return "DEBUG";
    case Logger::LogLevel::TRACE:   return "TRACE";
    case Logger::LogLevel::INVALID: return "INVAL";
  }
  return "INVAL";
}

const char *LevelColor(const Logger::LogLevel level) noexcept {
  switch (level) {
    case Logger::LogLevel::FATAL:   return "\033[1;97;41m"; // weiß auf rot
    case Logger::LogLevel::ERROR:   return "\033[1;31m";    // hellrot
    case Logger::LogLevel::ALERT:   return "\033[1;35m";    // magenta
    case Logger::LogLevel::WARNING: return "\033[1;33m";    // gelb
    case Logger::LogLevel::INFO:    return "\033[1;32m";    // grün
    case Logger::LogLevel::DEBUG:   return "\033[1;36m";    // cyan
    case Logger::LogLevel::TRACE:   return "\033[0;37m";    // grau
    case Logger::LogLevel::INVALID: return "";
  }
  return "";
}

bool ColorsEnabled() noexcept {
  static const bool enabled = std::getenv("NO_COLOR") == nullptr;
  return enabled;
}

void EnableAnsiColors() noexcept {
#if defined(_WIN32)
  static const bool enabled = [] {
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode) != 0) {
      SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    return true;
  }();
  (void)enabled;
#endif
}

std::string Timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto seconds = std::chrono::system_clock::to_time_t(now);
  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch())
                          .count() %
                      1000;

  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &seconds);
#else
  localtime_r(&seconds, &local);
#endif

  char dateTime[32]{};
  std::strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", &local);

  char result[48]{};
  std::snprintf(result, sizeof(result), "%s.%03lld", dateTime,
                static_cast<long long>(millis));
  return result;
}

} // namespace

std::atomic<Logger::LogLevel> Logger::sMaxLevel{LogLevel::INFO};
std::atomic<std::ostream *> Logger::sOutput{nullptr};
std::mutex Logger::sMutex;

Logger::Logger(const LogLevel logLevel, const std::string_view file, const i32 line) noexcept
    : level_(logLevel), file_(file), line_(line), active_(IsEnabled(logLevel)) {}

Logger::~Logger() {
  flush();
}

Logger &Logger::operator()(const std::string_view text) {
  append(text);
  return *this;
}

Logger &Logger::operator<<(const std::string_view text) {
  append(text);
  return *this;
}

Logger &Logger::operator<<(std::ostream &(*manip)(std::ostream &)) {
  if (active_ && manip != nullptr) {
    flush();
  }
  return *this;
}

void Logger::SetMaxLogLevel(const LogLevel level) noexcept {
  sMaxLevel.store(level, std::memory_order_relaxed);
}

Logger::LogLevel Logger::MaxLogLevel() noexcept {
  return sMaxLevel.load(std::memory_order_relaxed);
}

bool Logger::IsEnabled(const LogLevel level) noexcept {
  const LogLevel maximum = sMaxLevel.load(std::memory_order_relaxed);
  return level != LogLevel::INVALID && maximum != LogLevel::INVALID &&
         static_cast<u8>(level) <= static_cast<u8>(maximum);
}

void Logger::SetOutputStream(std::ostream *stream) noexcept {
  sOutput.store(stream, std::memory_order_relaxed);
}

std::ostream *Logger::OutputStream() noexcept {
  std::ostream *stream = sOutput.load(std::memory_order_relaxed);
  return stream != nullptr ? stream : &std::cout;
}

void Logger::append(const std::string_view text) {
  if (!active_) {
    return;
  }
  msg_.append(text);
}

void Logger::append(const char c) {
  if (!active_) {
    return;
  }
  msg_.push_back(c);
}

void Logger::flush() {
  if (!active_) {
    return;
  }
  active_ = false;

  while (!msg_.empty() && (msg_.back() == '\n' || msg_.back() == '\r')) {
    msg_.pop_back();
  }

  std::string prefix;
  prefix.reserve(40);
  prefix += '[';
  prefix += Timestamp();
  prefix += "] ";

  std::string body;
  body.reserve(32 + file_.size() + msg_.size());
  body += '[';
  body += LevelName(level_);
  body += "] [";
  body += file_;
  body += ':';
  body += std::to_string(line_);
  body += "] ";
  body += msg_;

  try {
    std::lock_guard lock(sMutex);
    std::ostream *out = OutputStream();
    EnableAnsiColors();

    *out << prefix;
    if (ColorsEnabled()) {
      *out << LevelColor(level_) << body << "\033[0m\n";
    } else {
      *out << body << '\n';
    }
    out->flush();
  } catch (...) {
    // Logging must never throw from a destructor.
  }

  msg_.clear();
}

} // namespace sgl
