#include "../include/ui.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
// UI state is process-global and initialized lazily on first paint call.
bool g_uiInitialized = false;
bool g_colorEnabled = false;
bool g_compactLayout = false;

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

bool isCompactLayout() {
    return g_compactLayout;
}

void setCompactLayout(bool compact) {
    g_compactLayout = compact;
}

void toggleCompactLayout() {
    g_compactLayout = !g_compactLayout;
}

std::string layoutModeLabel() {
    return g_compactLayout ? "Compact" : "Full";
}

int tablePageSize() {
    return g_compactLayout ? 8 : 12;
}

int preferredChartHeight() {
    return g_compactLayout ? 6 : 8;
}

int preferredChartWidth() {
    return g_compactLayout ? 36 : 52;
}

int preferredBarWidth() {
    return g_compactLayout ? 22 : 30;
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

void printBreadcrumb(const std::vector<std::string>& segments) {
    if (segments.empty()) {
        return;
    }

    std::string joined;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            joined += " > ";
        }
        joined += segments[i];
    }

    std::cout << "  " << muted(joined) << "\n";
}

void printKpiTiles(const std::vector<std::pair<std::string, std::string>>& tiles) {
    if (tiles.empty()) {
        return;
    }

    const int tileWidth = g_compactLayout ? 22 : 28;
    const std::size_t maxTilesPerRow = g_compactLayout ? 2U : 3U;
    const std::string topRule = "+" + std::string(static_cast<std::size_t>(tileWidth), '-') + "+";

    std::cout << "\n";
    for (std::size_t start = 0; start < tiles.size(); start += maxTilesPerRow) {
        const std::size_t end = std::min(tiles.size(), start + maxTilesPerRow);

        for (std::size_t i = start; i < end; ++i) {
            std::cout << topRule;
            if (i + 1 < end) {
                std::cout << "  ";
            }
        }
        std::cout << "\n";

        for (std::size_t i = start; i < end; ++i) {
            std::string label = truncate(tiles[i].first, static_cast<std::size_t>(tileWidth - 2));
            std::cout << "| " << std::left << std::setw(tileWidth - 2) << muted(label) << "|";
            if (i + 1 < end) {
                std::cout << "  ";
            }
        }
        std::cout << "\n";

        for (std::size_t i = start; i < end; ++i) {
            std::string value = truncate(tiles[i].second, static_cast<std::size_t>(tileWidth - 2));
            std::cout << "| " << std::left << std::setw(tileWidth - 2) << bold(value) << "|";
            if (i + 1 < end) {
                std::cout << "  ";
            }
        }
        std::cout << "\n";

        for (std::size_t i = start; i < end; ++i) {
            std::cout << topRule;
            if (i + 1 < end) {
                std::cout << "  ";
            }
        }
        std::cout << "\n";
    }
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

void printLineChart(const std::string& title,
                    const std::vector<std::string>& labels,
                    const std::vector<double>& values,
                    int height,
                    int width) {
    if (values.empty() || labels.empty() || values.size() != labels.size()) {
        return;
    }

    height = std::max(4, height);
    width = std::max(20, width);

    double maxValue = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (values[i] > maxValue) {
            maxValue = values[i];
        }
    }
    if (maxValue <= 0.0) {
        maxValue = 1.0;
    }

    std::vector<std::string> grid(static_cast<std::size_t>(height), std::string(static_cast<std::size_t>(width), ' '));

    int prevX = -1;
    int prevY = -1;
    const int denominator = static_cast<int>(values.size() > 1 ? values.size() - 1 : 1);

    for (std::size_t i = 0; i < values.size(); ++i) {
        const int x = static_cast<int>((static_cast<long long>(i) * (width - 1)) / denominator);
        const double normalized = values[i] / maxValue;
        const int y = static_cast<int>(std::lround((height - 1) * normalized));
        const int row = (height - 1) - y;

        grid[static_cast<std::size_t>(row)][static_cast<std::size_t>(x)] = '*';

        if (prevX >= 0 && prevY >= 0) {
            const int deltaX = x - prevX;
            const int steps = std::max(1, std::abs(deltaX));
            for (int step = 1; step < steps; ++step) {
                const int interpX = prevX + ((deltaX > 0 ? step : -step));
                const double ratio = static_cast<double>(step) / static_cast<double>(steps);
                const double interpY = static_cast<double>(prevY) + ratio * static_cast<double>(row - prevY);
                const int interpRow = static_cast<int>(std::lround(interpY));
                char& cell = grid[static_cast<std::size_t>(interpRow)][static_cast<std::size_t>(interpX)];
                if (cell == ' ') {
                    cell = '.';
                }
            }
        }

        prevX = x;
        prevY = row;
    }

    std::cout << "\n" << bold(title) << "\n";
    for (int r = 0; r < height; ++r) {
        std::cout << "  |" << primary(grid[static_cast<std::size_t>(r)]) << "|\n";
    }
    std::cout << "  +" << std::string(static_cast<std::size_t>(width), '-') << "+\n";

    std::string left = truncate(labels.front(), 12);
    std::string right = truncate(labels.back(), 12);
    if (left == right) {
        std::cout << "   " << muted(left) << "\n";
    } else {
        std::cout << "   " << muted(left);
        const int spacer = std::max(1, width - static_cast<int>(left.size()) - static_cast<int>(right.size()));
        std::cout << std::string(static_cast<std::size_t>(spacer), ' ') << muted(right) << "\n";
    }

    std::cout << "   " << muted("Peak: ") << std::fixed << std::setprecision(2) << maxValue << "\n";
}

} // namespace ui
