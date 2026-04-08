#ifndef UI_H
#define UI_H

#include <cstddef>
#include <string>
#include <vector>

namespace ui {

bool isColorEnabled();
void initializeUi();

std::string paint(const std::string& text, const char* colorCode);
std::string bold(const std::string& text);
std::string success(const std::string& text);
std::string warning(const std::string& text);
std::string error(const std::string& text);
std::string info(const std::string& text);
std::string muted(const std::string& text);

std::string truncate(const std::string& text, std::size_t width);

void printSectionTitle(const std::string& title);
void printTableRule(const std::vector<int>& widths);
void printTableHeader(const std::vector<std::string>& columns, const std::vector<int>& widths);
void printTableRow(const std::vector<std::string>& cells, const std::vector<int>& widths);
void printTableFooter(const std::vector<int>& widths);
void printBar(const std::string& label, double value, double maxValue, int width);

} // namespace ui

#endif
