#include <stdexcept>
#include <string>

#include "boolean_formula.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"

using namespace ftxui;

void TryParsing(BooleanFormula& formula,
                bool& success,
                const std::string& input,
                std::string& output,
                std::vector<std::vector<std::string>>& table_values) {
  try {
    formula.Parse(input);
    int vars_count;
    bool** out_table;
    formula.BuildTable(vars_count, out_table);
    output = "Формула пропарсена";
    std::vector<std::string> header;
    for(auto& val : formula.vars)
    {
      header.emplace_back(val);
    }
    header.emplace_back("Значение");
    table_values.clear();
    int rows_count = (1 << vars_count);
    table_values.emplace_back(header);
    for(int i = 0; i < rows_count; i++)
    {
      std::vector<std::string> row;
      for(int j = 0; j < vars_count + 1; j++)
      {
        row.emplace_back(out_table[i][j] == 1 ? "1" : "0");
      }
      table_values.emplace_back(row);
    }
    if (out_table != nullptr) {
      for(int i = 0; i < rows_count; i++) {
        delete[] out_table[i];
      }
      delete[] out_table;
    }
    success = true;
  } catch (const std::runtime_error& error) {
    output = error.what();
    success = false;
  }
}

int main() {
  BooleanFormula formula;
  bool parsed = false;
  std::string button_label = "Пропарсить";
  std::string input_formula;
  std::string output = "Используйте &, ||, |, ^, !, скобки и имена переменных";
  std::vector<std::vector<std::string>> table_values;

  auto parse_button =
      Button(&button_label,
             [&] {
                TryParsing(formula, parsed, input_formula, output, table_values); 
              });
  auto input_field = Input(&input_formula, "Enter formula");
  
  auto layout = Container::Horizontal({input_field, parse_button});
  auto component = Renderer(layout, [&] {
    auto table = Table(table_values);
    if(table_values.size() > 0)
    {
      table.SelectAll().Border(LIGHT);
      table.SelectAll().Separator(LIGHT);
    }   
    const auto status = parsed ? "Пропарсено" : "Формула";
    return vbox({
               hbox({filler(), text(status), filler()}),
               separator(),
               table.Render() | flex_grow | size(WIDTH, GREATER_THAN, 70) | size(HEIGHT, GREATER_THAN, 8),
               separator(),
               text(output) | flex,
               separator(),
               hbox({input_field->Render() | flex, parse_button->Render()}),
           }) |
           flex_grow | border | size(WIDTH, GREATER_THAN, 90) |
           size(HEIGHT, GREATER_THAN, 30);
  });

  ScreenInteractive::FitComponent().Loop(component);
}
