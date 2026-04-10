#ifndef UI_H
#define UI_H

#include <cstddef>
#include <utility>
#include <string>
#include <vector>

namespace ui {

bool isColorEnabled();
void initializeUi();
void showStartupSplash();
bool isCompactLayout();
void setCompactLayout(bool compact);
void toggleCompactLayout();
std::string layoutModeLabel();
int tablePageSize();
int preferredChartHeight();
int preferredChartWidth();
int preferredBarWidth();

std::string paint(const std::string& text, const char* colorCode);
std::string bold(const std::string& text);
std::string success(const std::string& text);
std::string warning(const std::string& text);
std::string error(const std::string& text);
std::string info(const std::string& text);
std::string muted(const std::string& text);
std::string primary(const std::string& text);
std::string accent(const std::string& text);
std::string roleLabel(const std::string& roleName);
std::string consensusStatus(const std::string& status);
const char* consensusColorCode(const std::string& status);

std::string truncate(const std::string& text, std::size_t width);

void printSectionTitle(const std::string& title);
void printBreadcrumb(const std::vector<std::string>& segments);
void printKpiTiles(const std::vector<std::pair<std::string, std::string>>& tiles);
void printTableRule(const std::vector<int>& widths);
void printTableHeader(const std::vector<std::string>& columns, const std::vector<int>& widths);
void printTableRow(const std::vector<std::string>& cells, const std::vector<int>& widths);
void printTableFooter(const std::vector<int>& widths);
void printBar(const std::string& label, double value, double maxValue, int width);
void printBar(const std::string& label, double value, double maxValue, int width, const char* colorCode);
void printLineChart(const std::string& title,
					const std::vector<std::string>& labels,
					const std::vector<double>& values,
					int height = 8,
					int width = 48);

} // namespace ui

#endif
