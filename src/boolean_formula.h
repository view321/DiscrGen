#pragma once

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

struct BooleanInput {
  std::unordered_map<std::string, bool> data;

  bool& operator[](const std::string& name);
};

class BooleanTreeNode {
 public:
  virtual ~BooleanTreeNode() = default;

  virtual bool Process(const BooleanInput& input) const = 0;
  virtual bool Process(const bool* input) const = 0;
  virtual void CollectVars(std::set<std::string>& vars) const = 0;
  virtual void CalculateIndices(const std::set<std::string>& vars) = 0;
};

class BooleanValue final : public BooleanTreeNode {
 public:
  explicit BooleanValue(std::string name);

  bool Process(const BooleanInput& input) const override;
  bool Process(const bool* input) const override;
  void CollectVars(std::set<std::string>& vars) const override;
  void CalculateIndices(const std::set<std::string>& vars) override;
  const std::string& name() const;

 private:
  std::string name_;
  int order_ = 0;
};

class BooleanNegativeOperation final : public BooleanTreeNode {
 public:
  explicit BooleanNegativeOperation(std::unique_ptr<BooleanTreeNode> child);

  bool Process(const BooleanInput& input) const override;
  bool Process(const bool* input) const override;
  void CollectVars(std::set<std::string>& vars) const override;
  void CalculateIndices(const std::set<std::string>& vars) override;

 private:
  std::unique_ptr<BooleanTreeNode> child_;
};

enum BinaryOperationType {
  AND,
  OR,
  XOR,
  NAND,
  PIERCE,
};

class BooleanBinaryOperation final : public BooleanTreeNode {
 public:
  BooleanBinaryOperation(BinaryOperationType type,
                         std::unique_ptr<BooleanTreeNode> left,
                         std::unique_ptr<BooleanTreeNode> right);

  bool Process(const BooleanInput& input) const override;
  bool Process(const bool* input) const override;
  void CollectVars(std::set<std::string>& vars) const override;
  void CalculateIndices(const std::set<std::string>& vars) override;

 private:
  BinaryOperationType type_;
  std::unique_ptr<BooleanTreeNode> left_;
  std::unique_ptr<BooleanTreeNode> right_;
};

class BooleanFormula {
 public:
  BooleanFormula();
  ~BooleanFormula();

  void Parse(std::string_view text);
  void BuildFromString(std::string text, int& pos, BooleanTreeNode*& res);
  bool ProcessRecurse(const BooleanInput& input) const;
  bool ProcessRecurse(const bool* input) const;
  void BuildTable(int& num_vars, bool**& output) const;
  void Render(std::string& output) const;

  BooleanTreeNode* root = nullptr;
  std::set<std::string> vars;

 private:
  std::unique_ptr<BooleanTreeNode> root_;
};
