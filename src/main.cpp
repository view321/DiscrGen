#include <cctype>
#include <limits>
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

namespace {

bool IsSpace(char c) {
  return std::isspace(static_cast<unsigned char>(c)) != 0;
}

int ParsePositiveInteger(const std::string& text,
                         const std::string& field_name) {
  size_t pos = 0;
  while (pos < text.size() && IsSpace(text[pos])) {
    ++pos;
  }

  const size_t begin = pos;
  while (pos < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
    ++pos;
  }
  if (begin == pos) {
    throw std::invalid_argument(field_name + " должно быть положительным числом");
  }

  const unsigned long long value = std::stoull(text.substr(begin, pos - begin));
  while (pos < text.size() && IsSpace(text[pos])) {
    ++pos;
  }
  if (pos != text.size() || value == 0 ||
      value > static_cast<unsigned long long>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(field_name + " должно быть положительным числом");
  }

  return static_cast<int>(value);
}

unsigned int ParseSeed(const std::string& text) {
  size_t pos = 0;
  while (pos < text.size() && IsSpace(text[pos])) {
    ++pos;
  }

  const size_t begin = pos;
  while (pos < text.size() &&
         std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
    ++pos;
  }
  if (begin == pos) {
    throw std::invalid_argument("Seed должен быть неотрицательным числом");
  }

  const unsigned long long value = std::stoull(text.substr(begin, pos - begin));
  while (pos < text.size() && IsSpace(text[pos])) {
    ++pos;
  }
  if (pos != text.size() ||
      value > static_cast<unsigned long long>(std::numeric_limits<unsigned int>::max())) {
    throw std::invalid_argument("Seed должен быть неотрицательным числом");
  }

  return static_cast<unsigned int>(value);
}

std::vector<std::string> ParseVariableNames(const std::string& text) {
  std::vector<std::string> variables;
  std::string current;

  const auto flush = [&] {
    if (!current.empty()) {
      variables.emplace_back(std::move(current));
      current.clear();
    }
  };

  for (const char c : text) {
    if (c == ',' || c == ';' || IsSpace(c)) {
      flush();
    } else {
      current.push_back(c);
    }
  }
  flush();

  return variables;
}

void FillTableValues(const TruthTable& truth_table,
                     std::vector<std::vector<std::string>>& table_values) {
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
}

Element RenderTable(const std::vector<std::vector<std::string>>& table_values) {
  auto table = Table(table_values);
  if (!table_values.empty()) {
    table.SelectAll().Border(LIGHT);
    table.SelectAll().Separator(LIGHT);
  }
  return table.Render() | flex_grow | size(WIDTH, GREATER_THAN, 70) |
         size(HEIGHT, GREATER_THAN, 8);
}

void TryParsing(BooleanFormula& formula,
                bool& success,
                const std::string& input,
                std::string& output,
                std::vector<std::vector<std::string>>& table_values) {
  try {
    formula.Parse(input);
    FillTableValues(formula.BuildTruthTable(), table_values);
    output = "Формула пропарсена";
    success = true;
  } catch (const std::exception& error) {
    output = error.what();
    table_values.clear();
    success = false;
  }
}

void TryGenerating(BooleanFormula& formula,
                   bool& success,
                   const std::string& depth_text,
                   const std::string& variable_names_text,
                   const std::string& seed_text,
                   std::string& generated_formula,
                   std::string& output,
                   std::vector<std::vector<std::string>>& table_values) {
  try {
    const int max_depth = ParsePositiveInteger(depth_text, "Глубина");
    const std::vector<std::string> variable_names =
        ParseVariableNames(variable_names_text);
    const unsigned int seed = ParseSeed(seed_text);

    formula = BooleanFormula::GenerateRandom(max_depth, variable_names, seed);
    generated_formula = formula.ToString();
    FillTableValues(formula.BuildTruthTable(), table_values);
    output = "Формула сгенерирована";
    success = true;
  } catch (const std::exception& error) {
    output = error.what();
    generated_formula.clear();
    table_values.clear();
    success = false;
  }
}

}  // namespace

int main() {
  BooleanFormula parsed_formula;
  bool parsed = false;
  std::string parse_button_label = "Пропарсить";
  std::string input_formula;
  std::string parse_output =
      "Используйте &, ||, |, ^, !, скобки и имена переменных";
  std::vector<std::vector<std::string>> parse_table_values;

  BooleanFormula generated_formula_model;
  bool generated = false;
  std::string generate_button_label = "Сгенерировать";
  std::string depth_text = "3";
  std::string variable_names_text = "a b c";
  std::string seed_text = "1";
  std::string generated_formula;
  std::string generation_output =
      "Введите глубину, имена переменных через пробел/запятую и seed";
  std::vector<std::vector<std::string>> generation_table_values;

  auto parse_button = Button(&parse_button_label, [&] {
    TryParsing(parsed_formula, parsed, input_formula, parse_output,
               parse_table_values);
  });
  auto input_field = Input(&input_formula, "Enter formula");
  auto parse_layout = Container::Horizontal({input_field, parse_button});
  auto parse_page = Renderer(parse_layout, [&] {
    const auto status = parsed ? "Пропарсено" : "Формула";
    return vbox({
               hbox({filler(), text(status), filler()}),
               separator(),
               RenderTable(parse_table_values),
               separator(),
               text(parse_output) | flex,
               separator(),
               hbox({input_field->Render() | flex, parse_button->Render()}),
           }) |
           flex_grow;
  });

  auto depth_input = Input(&depth_text, "max depth");
  auto variables_input = Input(&variable_names_text, "a b c");
  auto seed_input = Input(&seed_text, "seed");
  auto generate_button = Button(&generate_button_label, [&] {
    TryGenerating(generated_formula_model, generated, depth_text,
                  variable_names_text, seed_text, generated_formula,
                  generation_output, generation_table_values);
  });
  auto generate_layout = Container::Vertical(
      {depth_input, variables_input, seed_input, generate_button});
  auto generate_page = Renderer(generate_layout, [&] {
    const auto status = generated ? "Сгенерировано" : "Генерация формулы";
    return vbox({
               hbox({filler(), text(status), filler()}),
               separator(),
               RenderTable(generation_table_values),
               separator(),
               text(generation_output) | flex,
               text(generated_formula.empty()
                        ? "Формула: <пока не сгенерирована>"
                        : "Формула: " + generated_formula) |
                   flex,
               separator(),
               hbox({text("Глубина: "), depth_input->Render() | flex}),
               hbox({text("Переменные: "), variables_input->Render() | flex}),
               hbox({text("Seed: "), seed_input->Render() | flex}),
               generate_button->Render(),
           }) |
           flex_grow;
  });

  std::vector<std::string> page_names = {"Разбор", "Генерация"};
  int selected_page = 0;
  auto page_toggle = Toggle(&page_names, &selected_page);
  auto pages = Container::Tab({parse_page, generate_page}, &selected_page);
  auto layout = Container::Vertical({page_toggle, pages});

  auto component = Renderer(layout, [&] {
    return vbox({
               page_toggle->Render(),
               separator(),
               pages->Render() | flex_grow,
           }) |
           flex_grow | border | size(WIDTH, GREATER_THAN, 90) |
           size(HEIGHT, GREATER_THAN, 30);
  });

  ScreenInteractive::FitComponent().Loop(component);
}
