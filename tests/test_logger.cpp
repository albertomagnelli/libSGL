#include <catch_amalgamated.hpp>

#include <cstdlib>
#include <sstream>
#include <string>

#include <sgl/Log.hpp>

using namespace sgl;

namespace {

struct LoggerGuard {
    LoggerGuard()
        : output(Logger::outputStream()), level(Logger::maxLogLevel()) {}

    ~LoggerGuard() {
        Logger::setOutputStream(output);
        Logger::setMaxLogLevel(level);
    }

    std::ostream* output;
    Logger::LogLevel level;
};

} // namespace

TEST_CASE("basename liefert den Dateinamen", "[logger]") {
    REQUIRE(detail::basename("a/b/c.cpp") == "c.cpp");
    REQUIRE(detail::basename("a\\b\\c.cpp") == "c.cpp");
    REQUIRE(detail::basename("c.cpp") == "c.cpp");
}

TEST_CASE("Logger schreibt Zeitstempel, Level, Datei und Nachricht", "[logger]") {
    LoggerGuard guard;
    std::ostringstream stream;
    Logger::setOutputStream(&stream);
    Logger::setMaxLogLevel(Logger::LogLevel::TRACE);

    const int line = __LINE__ + 1;
    SLOGI("value %d", 42);

    const std::string text = stream.str();
    REQUIRE(text.find("[INFO ]") != std::string::npos);
    REQUIRE(text.find("[test_logger.cpp:" + std::to_string(line) + "]") != std::string::npos);
    REQUIRE(text.find("value 42") != std::string::npos);
    REQUIRE(text.find('\n') != std::string::npos);

    // Datum/Uhrzeit am Zeilenanfang.
    REQUIRE(text.size() > 1);
    REQUIRE(text.front() == '[');
    REQUIRE(text.find('-') != std::string::npos);
    REQUIRE(text.find(':') != std::string::npos);

    if (std::getenv("NO_COLOR") == nullptr) {
        REQUIRE(text.find("\033[1;32m") != std::string::npos);
        REQUIRE(text.find("\033[0m") != std::string::npos);
    }
}

TEST_CASE("Logger filtert nach Maximallevel", "[logger]") {
    LoggerGuard guard;
    std::ostringstream stream;
    Logger::setOutputStream(&stream);
    Logger::setMaxLogLevel(Logger::LogLevel::WARNING);

    SLOGI("wird gefiltert");
    REQUIRE(stream.str().empty());

    SLOGW("sichtbar");
    REQUIRE(stream.str().find("[WARN ]") != std::string::npos);
    REQUIRE(stream.str().find("sichtbar") != std::string::npos);
    REQUIRE(stream.str().find("wird gefiltert") == std::string::npos);
}

TEST_CASE("Logger unterstützt Stream-Operatoren und std::endl", "[logger]") {
    LoggerGuard guard;
    std::ostringstream stream;
    Logger::setOutputStream(&stream);
    Logger::setMaxLogLevel(Logger::LogLevel::TRACE);

    SLOGD << "x=" << 7 << " flag=" << true << std::endl;
    SLOGE << std::string_view{"zweite Zeile"};

    const std::string text = stream.str();
    REQUIRE(text.find("[DEBUG]") != std::string::npos);
    REQUIRE(text.find("x=7 flag=true") != std::string::npos);
    REQUIRE(text.find("[ERROR]") != std::string::npos);
    REQUIRE(text.find("zweite Zeile") != std::string::npos);

    const std::size_t first = text.find('\n');
    REQUIRE(first != std::string::npos);
    REQUIRE(text.find('\n', first + 1) != std::string::npos);
}
