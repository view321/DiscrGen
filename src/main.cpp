#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    const TruthTable truth_table = formula.BuildTruthTable();
    output = "Формула пропарсена";

    table_values.clear();
    table_values.emplace_back(truth_table.headers);
    for (const auto& values : truth_table.rows) {
      std::vector<std::string> row;
      row.reserve(values.size());
      for (const bool value : values) {
        row.emplace_back(value ? "1" : "0");
      }
      table_values.emplace_back(std::move(row));
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
