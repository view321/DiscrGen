#include "boolean_formula.h"

#include <cctype>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>

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

std::string At(size_t pos) {
  return " at position " + std::to_string(pos);
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

BooleanFormula::BooleanFormula() = default;
BooleanFormula::~BooleanFormula() = default;

void BooleanFormula::Parse(std::string_view text) {
  Parser parser(text);
  root_ = parser.Parse();
  root = root_.get();

  vars.clear();
  root_->CollectVars(vars);
  root_->CalculateIndices(vars);
}

void BooleanFormula::BuildFromString(
    std::string text, int& pos, BooleanTreeNode*& res) {
  if (pos < 0 || static_cast<size_t>(pos) > text.size()) {
    throw std::runtime_error("Invalid parse position");
  }

  Parser parser(std::string_view(text).substr(static_cast<size_t>(pos)),
                static_cast<size_t>(pos));
  root_ = parser.Parse();
  root = root_.get();

  vars.clear();
  root_->CollectVars(vars);
  root_->CalculateIndices(vars);

  pos = static_cast<int>(parser.position());
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

void BooleanFormula::Render(std::string& output) const {
  int num_vars = 0;
  bool** table = nullptr;
  BuildTable(num_vars, table);

  std::ostringstream rendered;
  for (const auto& var : vars) {
    rendered << var << '\t';
  }
  rendered << "result\n";

  const int rows = 1 << num_vars;
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col <= num_vars; ++col) {
      rendered << table[row][col] << '\t';
    }
    rendered << '\n';
    delete[] table[row];
  }
  delete[] table;

  output = rendered.str();
}
