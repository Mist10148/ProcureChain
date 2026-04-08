#ifndef UI_H
#define UI_H

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ui {

inline bool isColorEnabled() {
    static bool initialized = false;
    static bool enabled = false;

    if (initialized) {
        return enabled;
    }

    initialized = true;

#ifdef _WIN32
    HANDLE outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (outHandle == INVALID_HANDLE_VALUE) {
        enabled = false;
        return enabled;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(outHandle, &mode)) {
        enabled = false;
        return enabled;
    }

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    enabled = SetConsoleMode(outHandle, mode) != 0;
#else
    enabled = true;
#endif

    return enabled;
}

inline void initializeUi() {
    (void)isColorEnabled();
}

inline std::string paint(const std::string& text, const char* colorCode) {
    if (!isColorEnabled()) {
        return text;
    }

    return std::string("\x1b[") + colorCode + "m" + text + "\x1b[0m";
}

inline std::string bold(const std::string& text) {
    return paint(text, "1");
}

inline std::string success(const std::string& text) {
    return paint(text, "32");
}

inline std::string warning(const std::string& text) {
    return paint(text, "33");
}

inline std::string error(const std::string& text) {
    return paint(text, "31");
}

inline std::string info(const std::string& text) {
    return paint(text, "36");
}

inline std::string muted(const std::string& text) {
    return paint(text, "90");
}

inline std::string truncate(const std::string& text, size_t width) {
    if (text.size() <= width) {
        return text;
    }

    if (width <= 3) {
        return text.substr(0, width);
    }

    return text.substr(0, width - 3) + "...";
}

inline void printSectionTitle(const std::string& title) {
    std::cout << "\n" << info("==============================================================") << "\n";
    std::cout << "  " << bold(title) << "\n";
    std::cout << info("==============================================================") << "\n";
}

inline void printTableRule(const std::vector<int>& widths) {
    std::cout << '+';
    for (size_t i = 0; i < widths.size(); ++i) {
        std::cout << std::string(static_cast<size_t>(widths[i] + 2), '-') << '+';
    }
    std::cout << '\n';
}

inline void printTableHeader(const std::vector<std::string>& columns, const std::vector<int>& widths) {
    printTableRule(widths);
    std::cout << '|';
    for (size_t i = 0; i < columns.size(); ++i) {
        std::cout << ' ' << std::left << std::setw(widths[i]) << truncate(columns[i], static_cast<size_t>(widths[i])) << " |";
    }
    std::cout << '\n';
    printTableRule(widths);
}

inline void printTableRow(const std::vector<std::string>& cells, const std::vector<int>& widths) {
    std::cout << '|';
    for (size_t i = 0; i < cells.size(); ++i) {
        std::cout << ' ' << std::left << std::setw(widths[i]) << truncate(cells[i], static_cast<size_t>(widths[i])) << " |";
    }
    std::cout << '\n';
}

inline void printTableFooter(const std::vector<int>& widths) {
    printTableRule(widths);
}

inline void printBar(const std::string& label, double value, double maxValue, int width) {
    if (maxValue <= 0.0) {
        maxValue = 1.0;
    }

    double ratio = value / maxValue;
    if (ratio < 0.0) {
        ratio = 0.0;
    }
    if (ratio > 1.0) {
        ratio = 1.0;
    }

    int fill = static_cast<int>(ratio * width + 0.5);
    if (fill < 0) {
        fill = 0;
    }
    if (fill > width) {
        fill = width;
    }

    std::string bar = std::string(static_cast<size_t>(fill), '#') +
                      std::string(static_cast<size_t>(width - fill), '.');

    std::cout << "  " << std::left << std::setw(22) << truncate(label, 22)
              << " [" << paint(bar, "34") << "] "
              << std::fixed << std::setprecision(2) << value << '\n';
}

} // namespace ui

#endif
