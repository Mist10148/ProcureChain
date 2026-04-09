#include "../include/ui.h"

#include <iomanip>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
// UI state is process-global and initialized lazily on first paint call.
bool g_uiInitialized = false;
bool g_colorEnabled = false;

std::string toLowerCopy(std::string value) {
    // ASCII lowercase conversion used by consensus status helpers.
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] >= 'A' && value[i] <= 'Z') {
            value[i] = static_cast<char>(value[i] + ('a' - 'A'));
        }
    }

    return value;
}
}

namespace ui {

bool isColorEnabled() {
    // Enables ANSI escape processing once and caches the result.
    if (g_uiInitialized) {
        return g_colorEnabled;
    }

    g_uiInitialized = true;

#ifdef _WIN32
    HANDLE outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (outHandle == INVALID_HANDLE_VALUE) {
        g_colorEnabled = false;
        return g_colorEnabled;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(outHandle, &mode)) {
        g_colorEnabled = false;
        return g_colorEnabled;
    }

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    g_colorEnabled = SetConsoleMode(outHandle, mode) != 0;
#else
    g_colorEnabled = true;
#endif

    return g_colorEnabled;
}

void initializeUi() {
    // Explicit warm-up so first interactive screen already has color state set.
    (void)isColorEnabled();
}

std::string paint(const std::string& text, const char* colorCode) {
    // Wraps text with ANSI color code if console capabilities are available.
    if (!isColorEnabled()) {
        return text;
    }

    return std::string("\x1b[") + colorCode + "m" + text + "\x1b[0m";
}

std::string bold(const std::string& text) {
    return paint(text, "1");
}

std::string success(const std::string& text) {
    return paint(text, "32");
}

std::string warning(const std::string& text) {
    return paint(text, "33");
}

std::string error(const std::string& text) {
    return paint(text, "31");
}

std::string info(const std::string& text) {
    return paint(text, "36");
}

std::string muted(const std::string& text) {
    return paint(text, "90");
}

std::string primary(const std::string& text) {
    return paint(text, "34");
}

std::string accent(const std::string& text) {
    return paint(text, "35");
}

const char* consensusColorCode(const std::string& status) {
    // Maps logical approval states to stable color channels.
    const std::string normalized = toLowerCopy(status);

    if (normalized == "approved" || normalized == "published") {
        return "32";
    }

    if (normalized == "denied" || normalized == "rejected") {
        return "31";
    }

    if (normalized == "pending" || normalized == "pending_approval") {
        return "33";
    }

    return "36";
}

std::string consensusStatus(const std::string& status) {
    // User-facing status text with shared color semantics.
    const std::string normalized = toLowerCopy(status);

    if (normalized == "approved" || normalized == "published") {
        return paint("Approved", consensusColorCode(normalized));
    }

    if (normalized == "denied" || normalized == "rejected") {
        return paint("Denied", consensusColorCode(normalized));
    }

    if (normalized == "pending" || normalized == "pending_approval") {
        return paint("Pending", consensusColorCode(normalized));
    }

    return paint(status, consensusColorCode(normalized));
}

std::string roleLabel(const std::string& roleName) {
    // Role tinting improves scanability in admin dashboard headers.
    if (roleName == "Super Admin") {
        return accent(roleName);
    }

    if (roleName == "Procurement Officer") {
        return info(roleName);
    }

    if (roleName == "Budget Officer") {
        return primary(roleName);
    }

    if (roleName == "Municipal Administrator") {
        return success(roleName);
    }

    return muted(roleName);
}

std::string truncate(const std::string& text, std::size_t width) {
    // Ensures fixed-width table rendering without overflow.
    if (text.size() <= width) {
        return text;
    }

    if (width <= 3) {
        return text.substr(0, width);
    }

    return text.substr(0, width - 3) + "...";
}

void printSectionTitle(const std::string& title) {
    // Standardized page heading frame used across all screens.
    std::cout << "\n" << info("==============================================================") << "\n";
    std::cout << "  " << bold(title) << "\n";
    std::cout << info("==============================================================") << "\n";
}

void printTableRule(const std::vector<int>& widths) {
    // Draws one horizontal table border from column widths.
    std::cout << '+';
    for (std::size_t i = 0; i < widths.size(); ++i) {
        std::cout << std::string(static_cast<std::size_t>(widths[i] + 2), '-') << '+';
    }
    std::cout << '\n';
}

void printTableHeader(const std::vector<std::string>& columns, const std::vector<int>& widths) {
    // Header renderer for plain text tables.
    printTableRule(widths);
    std::cout << '|';
    for (std::size_t i = 0; i < columns.size(); ++i) {
        std::cout << ' ' << std::left << std::setw(widths[i]) << truncate(columns[i], static_cast<std::size_t>(widths[i])) << " |";
    }
    std::cout << '\n';
    printTableRule(widths);
}

void printTableRow(const std::vector<std::string>& cells, const std::vector<int>& widths) {
    // Row renderer shared by all table-producing modules.
    std::cout << '|';
    for (std::size_t i = 0; i < cells.size(); ++i) {
        std::cout << ' ' << std::left << std::setw(widths[i]) << truncate(cells[i], static_cast<std::size_t>(widths[i])) << " |";
    }
    std::cout << '\n';
}

void printTableFooter(const std::vector<int>& widths) {
    printTableRule(widths);
}

void printBar(const std::string& label, double value, double maxValue, int width) {
    // Convenience overload using the default bar color.
    printBar(label, value, maxValue, width, "34");
}

void printBar(const std::string& label, double value, double maxValue, int width, const char* colorCode) {
    // Scales a value into fixed-width bar text. Complexity is O(width) because
    // it builds two strings totaling exactly width characters.
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

    std::string bar = std::string(static_cast<std::size_t>(fill), '#') +
                      std::string(static_cast<std::size_t>(width - fill), '.');

    std::cout << "  " << std::left << std::setw(22) << truncate(label, 22)
              << " [" << paint(bar, colorCode) << "] "
              << std::fixed << std::setprecision(2) << value << '\n';
}

} // namespace ui
