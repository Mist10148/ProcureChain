#ifndef STORAGE_UTILS_H
#define STORAGE_UTILS_H

#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace storage {
struct OperationResult {
    bool ok;
    std::string message;
    bool rollbackAttempted;
    bool rollbackSucceeded;

    OperationResult(bool okValue = true,
                    const std::string& messageValue = "",
                    bool rollbackAttemptedValue = false,
                    bool rollbackSucceededValue = true)
        : ok(okValue),
          message(messageValue),
          rollbackAttempted(rollbackAttemptedValue),
          rollbackSucceeded(rollbackSucceededValue) {}
};

inline OperationResult makeSuccess(const std::string& message = "") {
    return OperationResult(true, message, false, true);
}

inline OperationResult makeFailure(const std::string& message,
                                   bool rollbackAttempted = false,
                                   bool rollbackSucceeded = true) {
    return OperationResult(false, message, rollbackAttempted, rollbackSucceeded);
}

namespace detail {
inline std::string trimCopy(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }

    const std::size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

inline bool isLeapYear(int year) {
    return (year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0));
}

inline int daysInMonth(int year, int month) {
    static const int kDaysByMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }

    if (month == 2 && isLeapYear(year)) {
        return 29;
    }

    return kDaysByMonth[month - 1];
}

inline bool isHexDigit(char ch) {
    return (ch >= '0' && ch <= '9') ||
           (ch >= 'a' && ch <= 'f') ||
           (ch >= 'A' && ch <= 'F');
}

inline int hexValue(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    return 10 + (ch - 'A');
}

inline bool finalizeStreamWrite(std::ostream& out) {
    out.flush();
    return out.good();
}

inline bool writeTextFileDirect(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }

    out << contents;
    return finalizeStreamWrite(out);
}
} // namespace detail

inline std::string sanitizeSingleLineInput(const std::string& value, bool trim = true) {
    std::string cleaned;
    cleaned.reserve(value.size());

    bool previousSpace = false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(value[i]);
        if (value[i] == '\r' || value[i] == '\n') {
            if (!previousSpace) {
                cleaned.push_back(' ');
                previousSpace = true;
            }
            continue;
        }

        if (std::iscntrl(ch) != 0 && value[i] != '\t') {
            continue;
        }

        if (value[i] == '\t') {
            if (!previousSpace) {
                cleaned.push_back(' ');
                previousSpace = true;
            }
            continue;
        }

        cleaned.push_back(value[i]);
        previousSpace = (value[i] == ' ');
    }

    return trim ? detail::trimCopy(cleaned) : cleaned;
}

inline std::string encodeField(const std::string& raw) {
    static const std::string kEncodedPrefix = "@pcx:";
    bool needsEncoding = raw.find('|') != std::string::npos ||
                         raw.find('\n') != std::string::npos ||
                         raw.find('\r') != std::string::npos ||
                         raw.find(kEncodedPrefix) == 0;

    if (!needsEncoding) {
        return raw;
    }

    static const char kHex[] = "0123456789ABCDEF";
    std::string encoded = kEncodedPrefix;
    encoded.reserve(kEncodedPrefix.size() + raw.size() * 2);

    for (std::size_t i = 0; i < raw.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(raw[i]);
        encoded.push_back(kHex[(ch >> 4) & 0x0F]);
        encoded.push_back(kHex[ch & 0x0F]);
    }

    return encoded;
}

inline std::string decodeField(const std::string& raw) {
    static const std::string kEncodedPrefix = "@pcx:";
    if (raw.find(kEncodedPrefix) != 0) {
        return raw;
    }

    const std::string payload = raw.substr(kEncodedPrefix.size());
    if ((payload.size() % 2) != 0) {
        return raw;
    }

    for (std::size_t i = 0; i < payload.size(); ++i) {
        if (!detail::isHexDigit(payload[i])) {
            return raw;
        }
    }

    std::string decoded;
    decoded.reserve(payload.size() / 2);

    for (std::size_t i = 0; i < payload.size(); i += 2) {
        const int high = detail::hexValue(payload[i]);
        const int low = detail::hexValue(payload[i + 1]);
        decoded.push_back(static_cast<char>((high << 4) | low));
    }

    return decoded;
}

inline std::vector<std::string> splitPipeRow(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream parser(line);
    std::string token;

    while (std::getline(parser, token, '|')) {
        tokens.push_back(decodeField(token));
    }

    return tokens;
}

inline std::string joinPipeRow(const std::vector<std::string>& fields) {
    std::ostringstream out;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            out << '|';
        }
        out << encodeField(fields[i]);
    }
    return out.str();
}

inline bool parseDateTextStrict(const std::string& value, std::tm& outTm) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
        return false;
    }

    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(value[i])) == 0) {
            return false;
        }
    }

    const int year = std::atoi(value.substr(0, 4).c_str());
    const int month = std::atoi(value.substr(5, 2).c_str());
    const int day = std::atoi(value.substr(8, 2).c_str());
    const int maxDay = detail::daysInMonth(year, month);

    if (year < 1900 || maxDay == 0 || day < 1 || day > maxDay) {
        return false;
    }

    std::tm parsed = {};
    parsed.tm_year = year - 1900;
    parsed.tm_mon = month - 1;
    parsed.tm_mday = day;
    outTm = parsed;
    return true;
}

inline bool isDateTextValidStrict(const std::string& value, bool allowEmpty = false) {
    if (allowEmpty && value.empty()) {
        return true;
    }

    std::tm tmValue = {};
    return parseDateTextStrict(value, tmValue);
}

inline bool parseDateTimeTextStrict(const std::string& value, std::tm& outTm) {
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' ||
        value[10] != ' ' || value[13] != ':' || value[16] != ':') {
        return false;
    }

    std::tm parsed = {};
    if (!parseDateTextStrict(value.substr(0, 10), parsed)) {
        return false;
    }

    for (std::size_t i = 11; i < value.size(); ++i) {
        if (i == 13 || i == 16) {
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(value[i])) == 0) {
            return false;
        }
    }

    const int hour = std::atoi(value.substr(11, 2).c_str());
    const int minute = std::atoi(value.substr(14, 2).c_str());
    const int second = std::atoi(value.substr(17, 2).c_str());

    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }

    parsed.tm_hour = hour;
    parsed.tm_min = minute;
    parsed.tm_sec = second;
    outTm = parsed;
    return true;
}

inline bool tryParseIntStrict(const std::string& raw, int& out) {
    const std::string value = detail::trimCopy(raw);
    if (value.empty()) {
        return false;
    }

    std::stringstream in(value);
    int parsed = 0;
    char trailing = '\0';
    if (!(in >> parsed) || (in >> trailing)) {
        return false;
    }

    out = parsed;
    return true;
}

inline bool tryParseLongLongStrict(const std::string& raw, long long& out) {
    const std::string value = detail::trimCopy(raw);
    if (value.empty()) {
        return false;
    }

    std::stringstream in(value);
    long long parsed = 0;
    char trailing = '\0';
    if (!(in >> parsed) || (in >> trailing)) {
        return false;
    }

    out = parsed;
    return true;
}

inline bool tryParseDoubleStrict(const std::string& raw, double& out) {
    const std::string value = detail::trimCopy(raw);
    if (value.empty()) {
        return false;
    }

    std::stringstream in(value);
    double parsed = 0.0;
    char trailing = '\0';
    if (!(in >> parsed) || (in >> trailing)) {
        return false;
    }

    out = parsed;
    return true;
}

inline std::string resolveWritePath(const std::string& primaryPath, const std::string& fallbackPath) {
    std::ifstream primary(primaryPath);
    if (primary.is_open()) {
        return primaryPath;
    }

    std::ifstream fallback(fallbackPath);
    if (fallback.is_open()) {
        return fallbackPath;
    }

    return primaryPath;
}

inline bool writeTextFileAtomically(const std::string& targetPath, const std::string& contents) {
    namespace fs = std::filesystem;

    const fs::path target(targetPath);
    fs::path temp = target;
    temp += ".tmp";
    fs::path backup = target;
    backup += ".bak";

    if (!detail::writeTextFileDirect(temp.string(), contents)) {
        return false;
    }

    std::error_code ec;
    const bool targetExists = fs::exists(target, ec) && !ec;

    ec.clear();
    if (fs::exists(backup, ec) && !ec) {
        fs::remove(backup, ec);
        if (ec) {
            fs::remove(temp, ec);
            return false;
        }
    }

    ec.clear();
    if (targetExists) {
        fs::rename(target, backup, ec);
        if (ec) {
            fs::remove(temp, ec);
            return false;
        }
    }

    ec.clear();
    fs::rename(temp, target, ec);
    if (ec) {
        std::error_code restoreEc;
        if (targetExists && fs::exists(backup, restoreEc) && !restoreEc) {
            fs::rename(backup, target, restoreEc);
        }
        fs::remove(temp, restoreEc);
        return false;
    }

    ec.clear();
    if (targetExists && fs::exists(backup, ec) && !ec) {
        fs::remove(backup, ec);
        if (ec) {
            return false;
        }
    }

    return true;
}

inline bool writeTextFileWithFallback(const std::string& primaryPath,
                                      const std::string& fallbackPath,
                                      const std::string& contents) {
    return writeTextFileAtomically(resolveWritePath(primaryPath, fallbackPath), contents);
}

inline bool writePipeFileWithFallback(const std::string& primaryPath,
                                      const std::string& fallbackPath,
                                      const std::string& headerLine,
                                      const std::vector<std::string>& rows) {
    std::ostringstream out;
    out << headerLine << '\n';
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out << rows[i] << '\n';
    }

    return writeTextFileWithFallback(primaryPath, fallbackPath, out.str());
}

inline bool appendLineToFileWithFallback(const std::string& primaryPath,
                                         const std::string& fallbackPath,
                                         const std::string& line) {
    const std::string targetPath = resolveWritePath(primaryPath, fallbackPath);
    std::ofstream out(targetPath, std::ios::app);
    if (!out.is_open()) {
        const std::string alternate = (targetPath == primaryPath) ? fallbackPath : primaryPath;
        out.clear();
        out.open(alternate, std::ios::app);
        if (!out.is_open()) {
            return false;
        }
    }

    out << line << '\n';
    return detail::finalizeStreamWrite(out);
}
} // namespace storage

#endif
