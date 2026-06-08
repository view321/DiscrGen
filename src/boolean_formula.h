#pragma once

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct BooleanInput {
  std::unordered_map<std::string, bool> data;

  bool& operator[](const std::string& name);
};

struct TruthTable {
  std::vector<std::string> headers;
  std::vector<std::vector<bool>> rows;
};

class BooleanFormula;

class BooleanTreeNode {
 public:
  virtual ~BooleanTreeNode() = default;

  BooleanFormula BuildAsRoot() const;

  virtual bool Process(const BooleanInput& input) const = 0;
  virtual bool Process(const bool* input) const = 0;
  virtual void CollectVars(std::set<std::string>& vars) const = 0;
  virtual void CalculateIndices(const std::set<std::string>& vars) = 0;
  virtual std::unique_ptr<BooleanTreeNode> Clone() const = 0;
  virtual int Depth() const = 0;
  virtual std::string ToString() const = 0;
  virtual void CollectSubformulas(
      std::vector<const BooleanTreeNode*>& nodes) const = 0;
};

class BooleanValue final : public BooleanTreeNode {
 public:
  explicit BooleanValue(std::string name);

  bool Process(const BooleanInput& input) const override;
  bool Process(const bool* input) const override;
  void CollectVars(std::set<std::string>& vars) const override;
  void CalculateIndices(const std::set<std::string>& vars) override;
  std::unique_ptr<BooleanTreeNode> Clone() const override;
  int Depth() const override;
  std::string ToString() const override;
  void CollectSubformulas(
      std::vector<const BooleanTreeNode*>& nodes) const override;
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
  std::unique_ptr<BooleanTreeNode> Clone() const override;
  int Depth() const override;
  std::string ToString() const override;
  void CollectSubformulas(
      std::vector<const BooleanTreeNode*>& nodes) const override;

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
  std::unique_ptr<BooleanTreeNode> Clone() const override;
  int Depth() const override;
  std::string ToString() const override;
  void CollectSubformulas(
      std::vector<const BooleanTreeNode*>& nodes) const override;

 private:
  BinaryOperationType type_;
  std::unique_ptr<BooleanTreeNode> left_;
  std::unique_ptr<BooleanTreeNode> right_;
};

class BooleanFormula {
 public:
  BooleanFormula();
  ~BooleanFormula();

  BooleanFormula(const BooleanFormula&) = delete;
  BooleanFormula& operator=(const BooleanFormula&) = delete;
  BooleanFormula(BooleanFormula&& other) noexcept;
  BooleanFormula& operator=(BooleanFormula&& other) noexcept;

  void Parse(std::string_view text);
  void BuildFromString(std::string text, int& pos, BooleanTreeNode*& res);
  bool ProcessRecurse(const BooleanInput& input) const;
  bool ProcessRecurse(const bool* input) const;
  void BuildTable(int& num_vars, bool**& output) const;
  TruthTable BuildTruthTable(bool include_subformulas = true) const;
  void Render(std::string& output) const;
  bool IsEquivalentTo(const BooleanFormula& other) const;
  int Depth() const;
  std::string ToString() const;

  static BooleanFormula GenerateRandom(
      int max_depth,
      const std::vector<std::string>& variable_names,
      unsigned int seed);

  BooleanTreeNode* root = nullptr;
  std::set<std::string> vars;

 private:
  friend class BooleanTreeNode;

  void BuildFromRoot(std::unique_ptr<BooleanTreeNode> root);

  std::unique_ptr<BooleanTreeNode> root_;
};
