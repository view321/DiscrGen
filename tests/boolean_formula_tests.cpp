#include "boolean_formula.h"

#include <algorithm>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

void TestEquivalentFormulasWithExtraVariable() {
  BooleanFormula lhs;
  lhs.Parse("a");

  BooleanFormula rhs;
  rhs.Parse("(a&b)||(a&!b)");

  if (!lhs.IsEquivalentTo(rhs)) {
    throw std::runtime_error("a should be equivalent to (a&b)||(a&!b)");
  }
}

void TestNonEquivalentFormulas() {
  BooleanFormula lhs;
  lhs.Parse("a&b");

  BooleanFormula rhs;
  rhs.Parse("a||b");

  if (lhs.IsEquivalentTo(rhs)) {
    throw std::runtime_error("a&b should not be equivalent to a||b");
  }
}

void TestTreeNodeBuildAsRootCreatesIndependentFormula() {
  BooleanFormula original;
  original.Parse("a&(b||c)");

  auto* binary = dynamic_cast<BooleanBinaryOperation*>(original.root);
  if (binary == nullptr) {
    throw std::runtime_error("expected parsed root to be a binary operation");
  }

  BooleanFormula rebuilt = binary->BuildAsRoot();

  BooleanInput input = Input({{"a", true}, {"b", false}, {"c", true}});
  if (!rebuilt.ProcessRecurse(input)) {
    throw std::runtime_error("BuildAsRoot formula should evaluate like the node subtree");
  }
}

void TestTreeNodeCollectVarsReportsStandaloneNodeVariables() {
  BooleanBinaryOperation node(
      OR,
      std::make_unique<BooleanValue>("x"),
      std::make_unique<BooleanNegativeOperation>(
          std::make_unique<BooleanValue>("y")));

  std::set<std::string> vars;
  node.CollectVars(vars);

  const std::set<std::string> expected = {"x", "y"};
  if (vars != expected) {
    throw std::runtime_error("CollectVars should report all variables in a standalone tree node");
  }
}

void TestRandomFormulaGenerationIsDeterministicForSameSeed() {
  const std::vector<std::string> variables = {"p", "q", "r"};
  BooleanFormula first = BooleanFormula::GenerateRandom(3, variables, 12345);
  BooleanFormula second = BooleanFormula::GenerateRandom(3, variables, 12345);

  std::string first_rendered;
  std::string second_rendered;
  first.Render(first_rendered);
  second.Render(second_rendered);

  if (first_rendered != second_rendered) {
    throw std::runtime_error("same random generation seed should produce the same rendered truth table");
  }
}

void TestRandomFormulaUsesRequestedVariableNames() {
  const std::vector<std::string> variables = {"p", "q", "answer"};
  BooleanFormula formula = BooleanFormula::GenerateRandom(4, variables, 99);

  if (formula.vars.empty()) {
    throw std::runtime_error("generated formula should contain at least one variable");
  }

  for (const auto& var : formula.vars) {
    if (std::find(variables.begin(), variables.end(), var) == variables.end()) {
      throw std::runtime_error("generated formula used a variable outside the requested variable names");
    }
  }
}

void TestRandomFormulaRejectsInvalidArguments() {
  const std::vector<std::string> variables = {"a", "b"};

  bool rejected_depth = false;
  try {
    BooleanFormula::GenerateRandom(0, variables, 1);
  } catch (const std::invalid_argument&) {
    rejected_depth = true;
  }
  if (!rejected_depth) {
    throw std::runtime_error("GenerateRandom should reject max_depth less than 1");
  }

  bool rejected_empty_variables = false;
  try {
    BooleanFormula::GenerateRandom(2, {}, 1);
  } catch (const std::invalid_argument&) {
    rejected_empty_variables = true;
  }
  if (!rejected_empty_variables) {
    throw std::runtime_error("GenerateRandom should reject an empty variable list");
  }
}

void TestRandomFormulaAtDepthOneIsSingleVariable() {
  const std::vector<std::string> variables = {"p", "q", "r"};
  BooleanFormula formula = BooleanFormula::GenerateRandom(1, variables, 7);

  if (formula.Depth() != 1 || formula.vars.size() != 1) {
    throw std::runtime_error("depth-one random formula should be exactly one variable node");
  }

  BooleanFormula variable_formula;
  variable_formula.Parse(*formula.vars.begin());

  if (!formula.IsEquivalentTo(variable_formula)) {
    throw std::runtime_error("depth-one random formula should be exactly a single variable");
  }
}

void TestBuildTruthTableIncludesSubformulaColumns() {
  BooleanFormula formula;
  formula.Parse("a&!b");

  TruthTable table = formula.BuildTruthTable();

  const std::vector<std::string> expected_headers = {"a", "b", "!b", "a&!b"};
  if (table.headers != expected_headers) {
    throw std::runtime_error("truth table headers should include variables, subformulas, and final formula");
  }
}

void TestBuildTruthTableComputesSubformulaValues() {
  BooleanFormula formula;
  formula.Parse("a&!b");

  TruthTable table = formula.BuildTruthTable();

  const std::vector<std::vector<bool>> expected_rows = {
      {false, false, true, false},
      {false, true, false, false},
      {true, false, true, true},
      {true, true, false, false},
  };

  if (table.rows != expected_rows) {
    throw std::runtime_error("truth table should include correct values for every subformula column");
  }
}

void TestRenderDisplaysSubformulaValues() {
  BooleanFormula formula;
  formula.Parse("a&!b");

  std::string rendered;
  formula.Render(rendered);

  if (rendered.find("a\tb\t!b\ta&!b") == std::string::npos) {
    throw std::runtime_error("rendered table should include subformula headers");
  }

  if (rendered.find("1\t0\t1\t1") == std::string::npos) {
    throw std::runtime_error("rendered table should include evaluated subformula values");
  }
}

void TestRandomFormulaDoesNotExceedMaxDepth() {
  const std::vector<std::string> variables = {"p", "q", "r"};

  for (unsigned int seed = 0; seed < 20; ++seed) {
    BooleanFormula formula = BooleanFormula::GenerateRandom(3, variables, seed);

    if (formula.Depth() > 3) {
      throw std::runtime_error("generated formula depth should not exceed max_depth");
    }
  }
}

void TestValueNodeBuildAsRootCreatesFormula() {
  BooleanValue node("flag");

  BooleanFormula formula = node.BuildAsRoot();

  if (!formula.ProcessRecurse(Input({{"flag", true}}))) {
    throw std::runtime_error("value node BuildAsRoot should preserve true variable value");
  }

  if (formula.ProcessRecurse(Input({{"flag", false}}))) {
    throw std::runtime_error("value node BuildAsRoot should preserve false variable value");
  }
}

void TestNegativeNodeBuildAsRootCreatesFormula() {
  BooleanNegativeOperation node(std::make_unique<BooleanValue>("flag"));

  BooleanFormula formula = node.BuildAsRoot();

  if (formula.ProcessRecurse(Input({{"flag", true}}))) {
    throw std::runtime_error("negative node BuildAsRoot should negate true to false");
  }

  if (!formula.ProcessRecurse(Input({{"flag", false}}))) {
    throw std::runtime_error("negative node BuildAsRoot should negate false to true");
  }
}

void TestEquivalentFormulaRequiresParsedOperands() {
  BooleanFormula parsed;
  parsed.Parse("a");

  BooleanFormula empty;

  bool rejected_right = false;
  try {
    parsed.IsEquivalentTo(empty);
  } catch (const std::runtime_error&) {
    rejected_right = true;
  }
  if (!rejected_right) {
    throw std::runtime_error("IsEquivalentTo should reject an unparsed right operand");
  }

  bool rejected_left = false;
  try {
    empty.IsEquivalentTo(parsed);
  } catch (const std::runtime_error&) {
    rejected_left = true;
  }
  if (!rejected_left) {
    throw std::runtime_error("IsEquivalentTo should reject an unparsed left operand");
  }
}

void TestBuildAsRootInitializesVariablesForTruthTable() {
  BooleanBinaryOperation node(
      AND,
      std::make_unique<BooleanValue>("a"),
      std::make_unique<BooleanValue>("b"));

  BooleanFormula formula = node.BuildAsRoot();

  const std::set<std::string> expected_vars = {"a", "b"};
  if (formula.vars != expected_vars) {
    throw std::runtime_error("BuildAsRoot should initialize formula vars from the node subtree");
  }

  int num_vars = 0;
  bool** table = nullptr;
  formula.BuildTable(num_vars, table);

  if (num_vars != 2 || table[0][2] || table[1][2] || table[2][2] ||
      !table[3][2]) {
    throw std::runtime_error("BuildAsRoot should initialize node indices for truth table evaluation");
  }

  for (int row = 0; row < (1 << num_vars); ++row) {
    delete[] table[row];
  }
  delete[] table;
}

void TestRandomFormulaRejectsDuplicateVariableNames() {
  bool rejected = false;
  try {
    BooleanFormula::GenerateRandom(2, {"a", "a"}, 1);
  } catch (const std::invalid_argument&) {
    rejected = true;
  }

  if (!rejected) {
    throw std::runtime_error("GenerateRandom should reject duplicate variable names");
  }
}

void TestRandomFormulaRejectsInvalidVariableNames() {
  bool rejected_space = false;
  try {
    BooleanFormula::GenerateRandom(2, {"valid", "not valid"}, 1);
  } catch (const std::invalid_argument&) {
    rejected_space = true;
  }
  if (!rejected_space) {
    throw std::runtime_error("GenerateRandom should reject variable names containing spaces");
  }

  bool rejected_parse_invalid = false;
  try {
    BooleanFormula::GenerateRandom(2, {"1bad"}, 1);
  } catch (const std::invalid_argument&) {
    rejected_parse_invalid = true;
  }
  if (!rejected_parse_invalid) {
    throw std::runtime_error("GenerateRandom should reject variable names that cannot be parsed");
  }
}

void TestBuildTruthTableRejectsUnparsedFormula() {
  BooleanFormula formula;

  bool rejected = false;
  try {
    formula.BuildTruthTable();
  } catch (const std::runtime_error&) {
    rejected = true;
  }

  if (!rejected) {
    throw std::runtime_error("BuildTruthTable should reject an unparsed formula");
  }
}

void TestBuildAsRootWorksThroughBaseNodePointer() {
  BooleanFormula original;
  original.Parse("a||b");

  BooleanTreeNode* node = original.root;
  BooleanFormula rebuilt = node->BuildAsRoot();

  if (!rebuilt.ProcessRecurse(Input({{"a", false}, {"b", true}}))) {
    throw std::runtime_error("BuildAsRoot should be available through BooleanTreeNode");
  }
}

void TestTruthTableSubformulaHeadersUseCanonicalParentheses() {
  BooleanFormula formula;
  formula.Parse("a&(b||c)");

  TruthTable table = formula.BuildTruthTable();

  const std::vector<std::string> expected_headers = {
      "a", "b", "c", "b||c", "a&(b||c)"};
  if (table.headers != expected_headers) {
    throw std::runtime_error("nested binary subformula headers should preserve grouping with parentheses");
  }
}

BooleanFormula BuildFormulaFromTemporaryTreeOwner() {
  BooleanFormula original;
  original.Parse("a&b");
  return original.root->BuildAsRoot();
}

void TestBuildAsRootCreatesIndependentOwnedCopy() {
  BooleanFormula rebuilt = BuildFormulaFromTemporaryTreeOwner();

  if (!rebuilt.ProcessRecurse(Input({{"a", true}, {"b", true}}))) {
    throw std::runtime_error("BuildAsRoot result should remain valid after original tree is destroyed");
  }
}

void TestCollectVarsWorksThroughBaseNodePointer() {
  BooleanFormula formula;
  formula.Parse("alpha&!beta");

  BooleanTreeNode* node = formula.root;

  std::set<std::string> vars;
  node->CollectVars(vars);

  const std::set<std::string> expected = {"alpha", "beta"};
  if (vars != expected) {
    throw std::runtime_error("CollectVars should be available through BooleanTreeNode");
  }
}

void TestBuildTruthTableDoesNotDuplicateRepeatedSubformulaColumns() {
  BooleanFormula formula;
  formula.Parse("(a&b)||(a&b)");

  TruthTable table = formula.BuildTruthTable();

  const std::vector<std::string> expected_headers = {
      "a", "b", "a&b", "(a&b)||(a&b)"};
  if (table.headers != expected_headers) {
    throw std::runtime_error("truth table should include each repeated subformula only once");
  }
}

void TestBuildTruthTableCanExcludeSubformulaColumns() {
  BooleanFormula formula;
  formula.Parse("a&!b");

  TruthTable table = formula.BuildTruthTable(false);

  const std::vector<std::string> expected_headers = {"a", "b", "a&!b"};
  if (table.headers != expected_headers) {
    throw std::runtime_error("BuildTruthTable(false) should include only variables and final formula");
  }

  const std::vector<std::vector<bool>> expected_rows = {
      {false, false, false},
      {false, true, false},
      {true, false, true},
      {true, true, false},
  };
  if (table.rows != expected_rows) {
    throw std::runtime_error("BuildTruthTable(false) should omit intermediate subformula values");
  }
}

void TestFormulaToStringUsesCanonicalExpression() {
  BooleanFormula formula;
  formula.Parse("a&(b||!c)");

  if (formula.ToString() != "a&(b||!c)") {
    throw std::runtime_error("ToString should return a canonical parseable expression");
  }
}

void TestFormulaToStringRejectsUnparsedFormula() {
  BooleanFormula formula;

  bool rejected = false;
  try {
    formula.ToString();
  } catch (const std::runtime_error&) {
    rejected = true;
  }

  if (!rejected) {
    throw std::runtime_error("ToString should reject an unparsed formula");
  }
}

void TestDifferentSeedsCanGenerateDifferentFormulas() {
  const std::vector<std::string> variables = {"p", "q", "r"};

  BooleanFormula first = BooleanFormula::GenerateRandom(4, variables, 1);
  BooleanFormula second = BooleanFormula::GenerateRandom(4, variables, 2);

  if (first.ToString() == second.ToString()) {
    throw std::runtime_error("different seeds should be able to produce different random formulas");
  }
}

}  // namespace

int main() {
  try {
    TestOperators();
    TestBracketsAndNegation();
    TestErrors();
    TestTruthTable();
    TestEquivalentFormulasWithExtraVariable();
    TestNonEquivalentFormulas();
    TestTreeNodeBuildAsRootCreatesIndependentFormula();
    TestTreeNodeCollectVarsReportsStandaloneNodeVariables();
    TestRandomFormulaGenerationIsDeterministicForSameSeed();
    TestRandomFormulaUsesRequestedVariableNames();
    TestRandomFormulaRejectsInvalidArguments();
    TestRandomFormulaAtDepthOneIsSingleVariable();
    TestBuildTruthTableIncludesSubformulaColumns();
    TestBuildTruthTableComputesSubformulaValues();
    TestRenderDisplaysSubformulaValues();
    TestRandomFormulaDoesNotExceedMaxDepth();
    TestValueNodeBuildAsRootCreatesFormula();
    TestNegativeNodeBuildAsRootCreatesFormula();
    TestEquivalentFormulaRequiresParsedOperands();
    TestBuildAsRootInitializesVariablesForTruthTable();
    TestRandomFormulaRejectsDuplicateVariableNames();
    TestRandomFormulaRejectsInvalidVariableNames();
    TestBuildTruthTableRejectsUnparsedFormula();
    TestBuildAsRootWorksThroughBaseNodePointer();
    TestTruthTableSubformulaHeadersUseCanonicalParentheses();
    TestBuildAsRootCreatesIndependentOwnedCopy();
    TestCollectVarsWorksThroughBaseNodePointer();
    TestBuildTruthTableDoesNotDuplicateRepeatedSubformulaColumns();
    TestBuildTruthTableCanExcludeSubformulaColumns();
    TestFormulaToStringUsesCanonicalExpression();
    TestFormulaToStringRejectsUnparsedFormula();
    TestDifferentSeedsCanGenerateDifferentFormulas();
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
