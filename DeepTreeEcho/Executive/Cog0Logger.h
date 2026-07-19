/*
// Vendored from cogpy/cog-zero standalone cog0 (header-only logger).
 * standalone/include/cog0/Logger.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Leveled, structured logger for the standalone cog0 application.
 *
 * Supports two output modes:
 *   - Plain text:   "HH:MM:SS [LEVEL] message"
 *   - JSON-lines:   {"ts":"…","level":"…","msg":"…","fields":{…}}
 *       suitable for log-aggregation pipelines (ELK, Loki, Datadog, …).
 *
 * Key API additions vs. original:
 *   setSink(ostream*)   — redirect all output to a custom stream
 *   setJsonMode(bool)   — toggle JSON-lines output
 *   logJson(level, msg, {key,val}…) — emit a structured log line with
 *                          arbitrary string key/value fields
 */

#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

namespace cog0 {

enum class LogLevel { DEBUG = 0, INFO, WARN, ERROR };

// -------------------------------------------------------------------------
// Field — a single structured key/value pair carried by logJson()
// -------------------------------------------------------------------------
struct LogField {
    std::string key;
    std::string value;
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    // ----------------------------------------------------------------
    // Configuration

    void setLevel(LogLevel l) { _level = l; }
    LogLevel level() const    { return _level; }

    /// Redirect all output to *out* (pass nullptr to restore defaults).
    void setSink(std::ostream* out) {
        std::lock_guard<std::mutex> lk(_mutex);
        _sink = out;
    }

    /// When true, every log line is a JSON object (JSON-lines format).
    void setJsonMode(bool on) {
        std::lock_guard<std::mutex> lk(_mutex);
        _jsonMode = on;
    }
    bool jsonMode() const { return _jsonMode; }

    // ----------------------------------------------------------------
    // Core log methods

    void log(LogLevel l, const std::string& msg) {
        logJson(l, msg, {});
    }

    /// Emit a structured log line with optional key/value fields.
    void logJson(LogLevel l, const std::string& msg,
                 std::vector<LogField> fields) {
        if (l < _level) return;
        std::lock_guard<std::mutex> lk(_mutex);

        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostream& out = _sink ? *_sink
                                  : (l >= LogLevel::WARN ? std::cerr : std::cout);

        if (_jsonMode) {
            // Build ISO-8601 timestamp
            char tsBuf[32];
            std::strftime(tsBuf, sizeof(tsBuf), "%Y-%m-%dT%H:%M:%SZ", &tm);

            out << '{'
                << "\"ts\":\"" << tsBuf << "\","
                << "\"level\":\"" << levelStrJson(l) << "\","
                << "\"msg\":" << jsonEscape(msg);
            if (!fields.empty()) {
                out << ",\"fields\":{";
                for (size_t i = 0; i < fields.size(); ++i) {
                    if (i) out << ',';
                    out << jsonEscape(fields[i].key) << ':'
                        << jsonEscape(fields[i].value);
                }
                out << '}';
            }
            out << "}\n";
        } else {
            out << std::put_time(&tm, "%H:%M:%S")
                << " [" << levelStr(l) << "] "
                << msg;
            for (const auto& f : fields) {
                out << ' ' << f.key << '=' << f.value;
            }
            out << '\n';
        }
        out.flush();
    }

    // ----------------------------------------------------------------
    // Convenience wrappers

    void debug(const std::string& m) { log(LogLevel::DEBUG, m); }
    void info(const std::string& m)  { log(LogLevel::INFO,  m); }
    void warn(const std::string& m)  { log(LogLevel::WARN,  m); }
    void error(const std::string& m) { log(LogLevel::ERROR, m); }

    void debug(const std::string& m, std::vector<LogField> f)
        { logJson(LogLevel::DEBUG, m, std::move(f)); }
    void info(const std::string& m, std::vector<LogField> f)
        { logJson(LogLevel::INFO,  m, std::move(f)); }
    void warn(const std::string& m, std::vector<LogField> f)
        { logJson(LogLevel::WARN,  m, std::move(f)); }
    void error(const std::string& m, std::vector<LogField> f)
        { logJson(LogLevel::ERROR, m, std::move(f)); }

private:
    Logger() : _level(LogLevel::INFO), _sink(nullptr), _jsonMode(false) {}

    LogLevel      _level;
    std::ostream* _sink;
    bool          _jsonMode;
    std::mutex    _mutex;

    static const char* levelStr(LogLevel l) {
        switch (l) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
        }
        return "?????";
    }

    static const char* levelStrJson(LogLevel l) {
        switch (l) {
            case LogLevel::DEBUG: return "debug";
            case LogLevel::INFO:  return "info";
            case LogLevel::WARN:  return "warn";
            case LogLevel::ERROR: return "error";
        }
        return "unknown";
    }

    /// Produce a JSON-encoded quoted string.
    static std::string jsonEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 2);
        out += '"';
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                      static_cast<unsigned char>(c));
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        out += '"';
        return out;
    }
};

inline Logger& logger() { return Logger::instance(); }

} // namespace cog0
