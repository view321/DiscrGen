#include "boolean_formula.h"

#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using Assignment = std::initializer_list<std::pair<std::string, bool>>;

BooleanInput Input(Assignment values) {
  BooleanInput input;
  for (const auto& [name, value] : values) {
    input[name] = value;
  }
  return input;
}

void ExpectValue(const std::string& expression,
                 Assignment input,
                 bool expected) {
  BooleanFormula formula;
  formula.Parse(expression);
  const bool actual = formula.ProcessRecurse(Input(input));
  if (actual != expected) {
    throw std::runtime_error(expression + " produced " +
                             std::to_string(actual) + ", expected " +
                             std::to_string(expected));
  }
}

void ExpectThrows(const std::string& expression) {
  try {
    BooleanFormula formula;
    formula.Parse(expression);
  } catch (const std::runtime_error&) {
    return;
  }
  throw std::runtime_error(expression + " should have failed");
}

void TestOperators() {
  ExpectValue("a&b", {{"a", true}, {"b", true}}, true);
  ExpectValue("a&b", {{"a", true}, {"b", false}}, false);
  ExpectValue("a||b", {{"a", false}, {"b", true}}, true);
  ExpectValue("a|b", {{"a", true}, {"b", true}}, false);
  ExpectValue("a|b", {{"a", true}, {"b", false}}, true);
  ExpectValue("a^b", {{"a", true}, {"b", false}}, true);
  ExpectValue("a^b", {{"a", true}, {"b", true}}, false);
  ExpectValue("a\xC3\x97" "b", {{"a", false}, {"b", false}}, true);
  ExpectValue("a\xC3\x97" "b", {{"a", false}, {"b", true}}, false);
}

void TestBracketsAndNegation() {
  ExpectValue("!(a&b)", {{"a", true}, {"b", true}}, false);
  ExpectValue("\xC2\xAC" "[a||b]", {{"a", false}, {"b", false}}, true);
  ExpectValue("{a&(b||c)}", {{"a", true}, {"b", false}, {"c", true}}, true);
}

void TestErrors() {
  ExpectThrows("");
  ExpectThrows("a&");
  ExpectThrows("&a");
  ExpectThrows("(a&b]");
  ExpectThrows("a b");
}

void TestTruthTable() {
  BooleanFormula formula;
  formula.Parse("a&b");

  int num_vars = 0;
  bool** table = nullptr;
  formula.BuildTable(num_vars, table);

  if (num_vars != 2 || table[0][2] || table[1][2] || table[2][2] ||
      !table[3][2]) {
    throw std::runtime_error("truth table for a&b is wrong");
  }

  for (int row = 0; row < (1 << num_vars); ++row) {
    delete[] table[row];
  }
  delete[] table;
}

}  // namespace

int main() {
  try {
    TestOperators();
    TestBracketsAndNegation();
    TestErrors();
    TestTruthTable();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
