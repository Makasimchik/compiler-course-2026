#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include <queue>
#include <unordered_set>

namespace {

using FunctionSet = std::unordered_set<const clang::FunctionDecl *>;

class ThrowAnalyzer {
public:
  explicit ThrowAnalyzer(clang::ASTContext &ctx) : m_context(ctx) {}

  bool canThrow(const clang::Stmt *stmt, const FunctionSet &noexceptFns,
                const clang::FunctionDecl *currentFn = nullptr) {
    if (!stmt)
      return false;

    std::queue<const clang::Stmt *> worklist;
    worklist.push(stmt);

    while (!worklist.empty()) {
      const auto *current = worklist.front();
      worklist.pop();

      if (llvm::isa<clang::CXXThrowExpr>(current))
        return true;

      if (llvm::isa<clang::CXXTryStmt>(current))
        return true;

      if (const auto *call = llvm::dyn_cast<clang::CallExpr>(current)) {
        if (const auto *callee = call->getDirectCallee()) {
          if (callee->getCanonicalDecl() !=
              (currentFn ? currentFn->getCanonicalDecl() : nullptr)) {
            if (!isCalleeSafe(callee, noexceptFns))
              return true;
          }
        }
      }

      if (const auto *construct =
              llvm::dyn_cast<clang::CXXConstructExpr>(current)) {
        if (const auto *ctor = construct->getConstructor()) {
          if (!isCalleeSafe(ctor, noexceptFns))
            return true;
        }
        const clang::CXXRecordDecl *record =
            construct->getType()->getAsCXXRecordDecl();
        if (record) {
          if (const clang::CXXDestructorDecl *dtor = record->getDestructor()) {
            if (!isCalleeSafe(dtor, noexceptFns))
              return true;
          }
        }
      }

      if (llvm::isa<clang::CXXNewExpr>(current))
        return true;

      for (const auto *child : current->children()) {
        if (child)
          worklist.push(child);
      }
    }

    return false;
  }

private:
  bool isCalleeSafe(const clang::FunctionDecl *fn,
                    const FunctionSet &noexceptFns) const {
    if (!fn)
      return true;
    if (const auto *fpt = fn->getType()->getAs<clang::FunctionProtoType>()) {
      if (fpt->isNothrow())
        return true;
    }
    return noexceptFns.find(fn) != noexceptFns.end();
  }

  clang::ASTContext &m_context;
};

class FunctionCollector : public clang::RecursiveASTVisitor<FunctionCollector> {
public:
  explicit FunctionCollector(std::vector<clang::FunctionDecl *> &functions)
      : m_functions(functions) {}

  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (func && func->hasBody() && !func->isImplicit() && !func->isDeleted())
      m_functions.push_back(func);
    return true;
  }

private:
  std::vector<clang::FunctionDecl *> &m_functions;
};

class NoexceptAdder : public clang::ASTConsumer {
public:
  explicit NoexceptAdder(clang::ASTContext &ctx)
      : m_context(ctx), m_analyzer(ctx), m_collector(m_functions) {}

  void HandleTranslationUnit(clang::ASTContext &ctx) override {
    m_collector.TraverseDecl(ctx.getTranslationUnitDecl());

    FunctionSet noexceptFunctions;
    for (const auto *func : m_functions) {
      if (hasNoexceptSpec(func))
        noexceptFunctions.insert(func);
    }

    bool changed = true;
    while (changed) {
      changed = false;
      for (auto *func : m_functions) {
        if (noexceptFunctions.count(func))
          continue;
        if (!func->getBody())
          continue;
        if (!m_analyzer.canThrow(func->getBody(), noexceptFunctions, func)) {
          addNoexceptSpecifier(func);
          noexceptFunctions.insert(func);
          changed = true;
        }
      }
    }

    ctx.getTranslationUnitDecl()->dump(llvm::errs());
  }

private:
  bool hasNoexceptSpec(const clang::FunctionDecl *func) const {
    if (const auto *fpt = func->getType()->getAs<clang::FunctionProtoType>())
      return fpt->isNothrow();
    return false;
  }

  void addNoexceptSpecifier(clang::FunctionDecl *func) {
    const auto *fpt = func->getType()->getAs<clang::FunctionProtoType>();
    if (!fpt)
      return;
    clang::FunctionProtoType::ExtProtoInfo epi = fpt->getExtProtoInfo();
    epi.ExceptionSpec.Type = clang::EST_BasicNoexcept;
    clang::QualType newType = m_context.getFunctionType(
        fpt->getReturnType(), fpt->getParamTypes(), epi);
    func->setType(newType);
  }

  clang::ASTContext &m_context;
  ThrowAnalyzer m_analyzer;
  std::vector<clang::FunctionDecl *> m_functions;
  FunctionCollector m_collector;
};

class NoexceptPluginAction : public clang::PluginASTAction {
public:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &ci, llvm::StringRef) override {
    return std::make_unique<NoexceptAdder>(ci.getASTContext());
  }

  bool ParseArgs(const clang::CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }

  ActionType getActionType() override { return ReplaceAction; }
};

} // namespace

static clang::FrontendPluginRegistry::Add<NoexceptPluginAction>
    X("noexcept_analyzer", "Add noexcept specifier");
