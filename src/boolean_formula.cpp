#include "boolean_formula.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

bool IsSpace(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool IsNameStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool IsNamePart(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool IsValidVariableName(std::string_view name) {
  if (name.empty() || !IsNameStart(name.front())) {
    return false;
  }
  for (const char c : name.substr(1)) {
    if (!IsNamePart(c)) {
      return false;
    }
  }
  return true;
}

std::string At(size_t pos) {
  return " at position " + std::to_string(pos);
}

std::string OperationText(BinaryOperationType type) {
  switch (type) {
    case AND:
      return "&";
    case OR:
      return "||";
    case XOR:
      return "^";
    case NAND:
      return "|";
    case PIERCE:
      return "\xC3\x97";
  }

  throw std::runtime_error("Unknown binary operation");
}

bool IsBinaryNode(const BooleanTreeNode* node) {
  return dynamic_cast<const BooleanBinaryOperation*>(node) != nullptr;
}

std::unique_ptr<BooleanTreeNode> MakeRandomVariable(
    const std::vector<std::string>& variable_names,
    std::mt19937& rng) {
  return std::make_unique<BooleanValue>(
      variable_names[rng() % variable_names.size()]);
}

BinaryOperationType RandomBinaryOperation(std::mt19937& rng) {
  switch (rng() % 5) {
    case 0:
      return AND;
    case 1:
      return OR;
    case 2:
      return XOR;
    case 3:
      return NAND;
    case 4:
      return PIERCE;
  }

  return AND;
}

std::unique_ptr<BooleanTreeNode> GenerateRandomNode(
    int max_depth,
    const std::vector<std::string>& variable_names,
    std::mt19937& rng,
    bool force_non_leaf) {
  if (max_depth <= 1) {
    return MakeRandomVariable(variable_names, rng);
  }

  const int kind = force_non_leaf ? static_cast<int>(rng() % 2) + 1
                                  : static_cast<int>(rng() % 3);
  if (kind == 0) {
    return MakeRandomVariable(variable_names, rng);
  }

  if (kind == 1) {
    return std::make_unique<BooleanNegativeOperation>(
        GenerateRandomNode(max_depth - 1, variable_names, rng, false));
  }

  return std::make_unique<BooleanBinaryOperation>(
      RandomBinaryOperation(rng),
      GenerateRandomNode(max_depth - 1, variable_names, rng, false),
      GenerateRandomNode(max_depth - 1, variable_names, rng, false));
}

void ValidateRandomGenerationArguments(
    int max_depth,
    const std::vector<std::string>& variable_names) {
  if (max_depth < 1) {
    throw std::invalid_argument("max_depth must be at least 1");
  }
  if (variable_names.empty()) {
    throw std::invalid_argument("variable_names must not be empty");
  }

  std::set<std::string> seen;
  for (const auto& name : variable_names) {
    if (!IsValidVariableName(name)) {
      throw std::invalid_argument("invalid variable name: " + name);
    }
    if (!seen.insert(name).second) {
      throw std::invalid_argument("duplicate variable name: " + name);
    }
  }
}

class Parser {
 public:
  explicit Parser(std::string_view text, size_t offset = 0)
      : text_(text), offset_(offset) {}

  std::unique_ptr<BooleanTreeNode> Parse() {
    SkipSpaces();
    if (Done()) {
      throw std::runtime_error("Formula is empty");
    }

    auto expression = ParseExpression();
    SkipSpaces();
    if (!Done()) {
      throw std::runtime_error("Unexpected token" + At(offset_ + pos_));
    }
    return expression;
  }

  size_t position() const { return offset_ + pos_; }

 private:
  std::unique_ptr<BooleanTreeNode> ParseExpression() {
    auto left = ParseUnary();

    for (;;) {
      SkipSpaces();
      const auto operation = ReadOperation();
      if (!operation.has_value()) {
        return left;
      }
      left = std::make_unique<BooleanBinaryOperation>(
          *operation, std::move(left), ParseUnary());
    }
  }

  std::unique_ptr<BooleanTreeNode> ParseUnary() {
    SkipSpaces();
    if (Consume("!") || Consume("~") || Consume("\xC2\xAC")) {
      return std::make_unique<BooleanNegativeOperation>(ParseUnary());
    }
    return ParsePrimary();
  }

  std::unique_ptr<BooleanTreeNode> ParsePrimary() {
    SkipSpaces();
    if (Done()) {
      throw std::runtime_error("Expected expression" + At(offset_ + pos_));
    }

    const char opener = text_[pos_];
    if (opener == '(' || opener == '[' || opener == '{') {
      ++pos_;
      auto nested = ParseExpression();
      SkipSpaces();

      const char closer = opener == '(' ? ')' : opener == '[' ? ']' : '}';
      if (Done() || text_[pos_] != closer) {
        throw std::runtime_error("Unmatched bracket" + At(offset_ + pos_));
      }
      ++pos_;
      return nested;
    }

    if (opener == ')' || opener == ']' || opener == '}') {
      throw std::runtime_error("Unexpected closing bracket" + At(offset_ + pos_));
    }

    if (!IsNameStart(opener)) {
      throw std::runtime_error("Unexpected character" + At(offset_ + pos_));
    }

    const size_t begin = pos_++;
    while (!Done() && IsNamePart(text_[pos_])) {
      ++pos_;
    }
    return std::make_unique<BooleanValue>(
        std::string(text_.substr(begin, pos_ - begin)));
  }

  std::optional<BinaryOperationType> ReadOperation() {
    if (Consume("||")) return OR;
    if (Consume("&")) return AND;
    if (Consume("|")) return NAND;
    if (Consume("^") || Consume("\xC2\xA9") || Consume("\xE2\x8A\x95")) {
      return XOR;
    }
    if (Consume("\xC3\x97") || Consume("\xE2\x86\x93")) {
      return PIERCE;
    }
    return std::nullopt;
  }

  bool Consume(std::string_view token) {
    if (text_.substr(pos_, token.size()) != token) {
      return false;
    }
    pos_ += token.size();
    return true;
  }

  void SkipSpaces() {
    while (!Done() && IsSpace(text_[pos_])) {
      ++pos_;
    }
  }

  bool Done() const { return pos_ >= text_.size(); }

  std::string_view text_;
  size_t offset_;
  size_t pos_ = 0;
};

}  // namespace

bool& BooleanInput::operator[](const std::string& name) {
  return data[name];
}

BooleanFormula BooleanTreeNode::BuildAsRoot() const {
  BooleanFormula formula;
  formula.BuildFromRoot(Clone());
  return formula;
}

BooleanValue::BooleanValue(std::string name) : name_(std::move(name)) {}

bool BooleanValue::Process(const BooleanInput& input) const {
  const auto found = input.data.find(name_);
  if (found == input.data.end()) {
    throw std::runtime_error("Missing input variable: " + name_);
  }
  return found->second;
}

bool BooleanValue::Process(const bool* input) const {
  return input[order_];
}

void BooleanValue::CollectVars(std::set<std::string>& vars) const {
  vars.emplace(name_);
}

void BooleanValue::CalculateIndices(const std::set<std::string>& vars) {
  const auto found = vars.find(name_);
  if (found == vars.end()) {
    throw std::runtime_error("Variable has no table index: " + name_);
  }
  order_ = static_cast<int>(std::distance(vars.begin(), found));
}

std::unique_ptr<BooleanTreeNode> BooleanValue::Clone() const {
  auto clone = std::make_unique<BooleanValue>(name_);
  clone->order_ = order_;
  return clone;
}

int BooleanValue::Depth() const {
  return 1;
}

std::string BooleanValue::ToString() const {
  return name_;
}

void BooleanValue::CollectSubformulas(
    std::vector<const BooleanTreeNode*>& /*nodes*/) const {}

const std::string& BooleanValue::name() const {
  return name_;
}

BooleanNegativeOperation::BooleanNegativeOperation(
    std::unique_ptr<BooleanTreeNode> child)
    : child_(std::move(child)) {}

bool BooleanNegativeOperation::Process(const BooleanInput& input) const {
  return !child_->Process(input);
}

bool BooleanNegativeOperation::Process(const bool* input) const {
  return !child_->Process(input);
}

void BooleanNegativeOperation::CollectVars(std::set<std::string>& vars) const {
  child_->CollectVars(vars);
}

void BooleanNegativeOperation::CalculateIndices(
    const std::set<std::string>& vars) {
  child_->CalculateIndices(vars);
}

std::unique_ptr<BooleanTreeNode> BooleanNegativeOperation::Clone() const {
  return std::make_unique<BooleanNegativeOperation>(child_->Clone());
}

int BooleanNegativeOperation::Depth() const {
  return 1 + child_->Depth();
}

std::string BooleanNegativeOperation::ToString() const {
  const std::string child = child_->ToString();
  if (IsBinaryNode(child_.get())) {
    return "!(" + child + ")";
  }
  return "!" + child;
}

void BooleanNegativeOperation::CollectSubformulas(
    std::vector<const BooleanTreeNode*>& nodes) const {
  child_->CollectSubformulas(nodes);
  nodes.push_back(this);
}

BooleanBinaryOperation::BooleanBinaryOperation(
    BinaryOperationType type,
    std::unique_ptr<BooleanTreeNode> left,
    std::unique_ptr<BooleanTreeNode> right)
    : type_(type), left_(std::move(left)), right_(std::move(right)) {}

bool BooleanBinaryOperation::Process(const BooleanInput& input) const {
  const bool left = left_->Process(input);
  const bool right = right_->Process(input);

  switch (type_) {
    case AND:
      return left && right;
    case OR:
      return left || right;
    case XOR:
      return left != right;
    case NAND:
      return !(left && right);
    case PIERCE:
      return !(left || right);
  }

  throw std::runtime_error("Unknown binary operation");
}

bool BooleanBinaryOperation::Process(const bool* input) const {
  const bool left = left_->Process(input);
  const bool right = right_->Process(input);

  switch (type_) {
    case AND:
      return left && right;
    case OR:
      return left || right;
    case XOR:
      return left != right;
    case NAND:
      return !(left && right);
    case PIERCE:
      return !(left || right);
  }

  throw std::runtime_error("Unknown binary operation");
}

void BooleanBinaryOperation::CollectVars(std::set<std::string>& vars) const {
  left_->CollectVars(vars);
  right_->CollectVars(vars);
}

void BooleanBinaryOperation::CalculateIndices(const std::set<std::string>& vars) {
  left_->CalculateIndices(vars);
  right_->CalculateIndices(vars);
}

std::unique_ptr<BooleanTreeNode> BooleanBinaryOperation::Clone() const {
  return std::make_unique<BooleanBinaryOperation>(
      type_, left_->Clone(), right_->Clone());
}

int BooleanBinaryOperation::Depth() const {
  return 1 + std::max(left_->Depth(), right_->Depth());
}

std::string BooleanBinaryOperation::ToString() const {
  std::string left = left_->ToString();
  if (IsBinaryNode(left_.get())) {
    left = "(" + left + ")";
  }

  std::string right = right_->ToString();
  if (IsBinaryNode(right_.get())) {
    right = "(" + right + ")";
  }

  return left + OperationText(type_) + right;
}

void BooleanBinaryOperation::CollectSubformulas(
    std::vector<const BooleanTreeNode*>& nodes) const {
  left_->CollectSubformulas(nodes);
  right_->CollectSubformulas(nodes);
  nodes.push_back(this);
}

BooleanFormula::BooleanFormula() = default;
BooleanFormula::~BooleanFormula() = default;

BooleanFormula::BooleanFormula(BooleanFormula&& other) noexcept
    : vars(std::move(other.vars)), root_(std::move(other.root_)) {
  root = root_.get();
  other.root = nullptr;
  other.vars.clear();
}

BooleanFormula& BooleanFormula::operator=(BooleanFormula&& other) noexcept {
  if (this != &other) {
    root_ = std::move(other.root_);
    root = root_.get();
    vars = std::move(other.vars);
    other.root = nullptr;
    other.vars.clear();
  }
  return *this;
}

void BooleanFormula::BuildFromRoot(std::unique_ptr<BooleanTreeNode> new_root) {
  if (!new_root) {
    throw std::runtime_error("Formula root is null");
  }

  root_ = std::move(new_root);
  root = root_.get();

  vars.clear();
  root_->CollectVars(vars);
  root_->CalculateIndices(vars);
}

void BooleanFormula::Parse(std::string_view text) {
  Parser parser(text);
  BuildFromRoot(parser.Parse());
}

void BooleanFormula::BuildFromString(
    std::string text, int& pos, BooleanTreeNode*& res) {
  if (pos < 0 || static_cast<size_t>(pos) > text.size()) {
    throw std::runtime_error("Invalid parse position");
  }

  Parser parser(std::string_view(text).substr(static_cast<size_t>(pos)),
                static_cast<size_t>(pos));
  auto parsed = parser.Parse();
  pos = static_cast<int>(parser.position());
  BuildFromRoot(std::move(parsed));
  res = root;
}

bool BooleanFormula::ProcessRecurse(const BooleanInput& input) const {
  if (!root_) {
    throw std::runtime_error("Formula is not parsed");
  }
  return root_->Process(input);
}

bool BooleanFormula::ProcessRecurse(const bool* input) const {
  if (!root_) {
    throw std::runtime_error("Formula is not parsed");
  }
  return root_->Process(input);
}

void BooleanFormula::BuildTable(int& num_vars, bool**& output) const {
  if (!root_) {
    throw std::runtime_error("Formula is not parsed");
  }
  if (vars.size() >= std::numeric_limits<int>::digits) {
    throw std::runtime_error("Too many variables for a truth table");
  }

  num_vars = static_cast<int>(vars.size());
  const int rows = 1 << num_vars;
  const int columns = num_vars + 1;
  output = new bool*[rows];

  for (int row = 0; row < rows; ++row) {
    output[row] = new bool[columns]{};
    for (int col = 0; col < num_vars; ++col) {
      const int shift = num_vars - col - 1;
      output[row][col] = ((row >> shift) & 1) != 0;
    }
    output[row][num_vars] = ProcessRecurse(output[row]);
  }
}

TruthTable BooleanFormula::BuildTruthTable(bool include_subformulas) const {
  if (!root_) {
    throw std::runtime_error("Formula is not parsed");
  }
  if (vars.size() >= std::numeric_limits<int>::digits) {
    throw std::runtime_error("Too many variables for a truth table");
  }

  TruthTable table;
  std::vector<std::string> variable_names(vars.begin(), vars.end());
  for (const auto& var : variable_names) {
    table.headers.push_back(var);
  }

  std::vector<const BooleanTreeNode*> candidate_formula_columns;
  if (include_subformulas) {
    root_->CollectSubformulas(candidate_formula_columns);
  } else {
    candidate_formula_columns.push_back(root_.get());
  }

  std::set<std::string> seen_headers(table.headers.begin(), table.headers.end());
  std::vector<const BooleanTreeNode*> formula_columns;
  for (const auto* node : candidate_formula_columns) {
    const std::string header = node->ToString();
    if (seen_headers.insert(header).second) {
      table.headers.push_back(header);
      formula_columns.push_back(node);
    }
  }

  if (include_subformulas) {
    const std::string root_header = root_->ToString();
    if (seen_headers.insert(root_header).second) {
      table.headers.push_back(root_header);
      formula_columns.push_back(root_.get());
    }
  }

  const int num_vars = static_cast<int>(variable_names.size());
  const int rows = 1 << num_vars;
  table.rows.reserve(rows);
  for (int row = 0; row < rows; ++row) {
    BooleanInput input;
    std::vector<bool> row_values;
    row_values.reserve(table.headers.size());

    for (int col = 0; col < num_vars; ++col) {
      const int shift = num_vars - col - 1;
      const bool value = ((row >> shift) & 1) != 0;
      input[variable_names[col]] = value;
      row_values.push_back(value);
    }

    for (const auto* node : formula_columns) {
      row_values.push_back(node->Process(input));
    }

    table.rows.push_back(std::move(row_values));
  }

  return table;
}

void BooleanFormula::Render(std::string& output) const {
  const TruthTable table = BuildTruthTable();

  std::ostringstream rendered;
  for (const auto& header : table.headers) {
    rendered << header << '\t';
  }
  rendered << '\n';

  for (const auto& row : table.rows) {
    for (const bool value : row) {
      rendered << value << '\t';
    }
    rendered << '\n';
  }

  output = rendered.str();
}

bool BooleanFormula::IsEquivalentTo(const BooleanFormula& other) const {
  if (!root_ || !other.root_) {
    throw std::runtime_error("Formula is not parsed");
  }

  std::set<std::string> all_vars = vars;
  all_vars.insert(other.vars.begin(), other.vars.end());
  if (all_vars.size() >= std::numeric_limits<size_t>::digits) {
    throw std::runtime_error("Too many variables for equivalence comparison");
  }

  std::vector<std::string> variable_names(all_vars.begin(), all_vars.end());
  const size_t rows = static_cast<size_t>(1) << variable_names.size();
  for (size_t row = 0; row < rows; ++row) {
    BooleanInput input;
    for (size_t col = 0; col < variable_names.size(); ++col) {
      const size_t shift = variable_names.size() - col - 1;
      input[variable_names[col]] = ((row >> shift) & 1U) != 0;
    }

    if (root_->Process(input) != other.root_->Process(input)) {
      return false;
    }
  }

  return true;
}

int BooleanFormula::Depth() const {
  if (!root_) {
    throw std::runtime_error("Formula is not parsed");
  }
  return root_->Depth();
}

std::string BooleanFormula::ToString() const {
  if (!root_) {
    throw std::runtime_error("Formula is not parsed");
  }
  return root_->ToString();
}

BooleanFormula BooleanFormula::GenerateRandom(
    int max_depth,
    const std::vector<std::string>& variable_names,
    unsigned int seed) {
  ValidateRandomGenerationArguments(max_depth, variable_names);

  std::mt19937 rng(seed);
  BooleanFormula formula;
  formula.BuildFromRoot(GenerateRandomNode(max_depth, variable_names, rng,
                                           max_depth > 1));
  return formula;
}
