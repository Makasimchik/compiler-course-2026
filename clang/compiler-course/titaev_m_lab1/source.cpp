#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"

using namespace clang;

namespace {

class DeprecatedFunctionVisitor final
    : public RecursiveASTVisitor<DeprecatedFunctionVisitor> {
public:
  explicit DeprecatedFunctionVisitor(ASTContext *Context) : Context(Context) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD->isThisDeclarationADefinition())
      return true;

    llvm::StringRef Name = FD->getName();

    if (!Name.contains("deprecated"))
      return true;

    DiagnosticsEngine &DE = Context->getDiagnostics();
    unsigned ID =
        DE.getCustomDiagID(DiagnosticsEngine::Warning,
                           "function '%0' contains 'deprecated' in its name");

    DE.Report(FD->getLocation(), ID) << Name;

    return true;
  }

private:
  ASTContext *Context;
};

class DeprecatedFunctionConsumer final : public ASTConsumer {
public:
  explicit DeprecatedFunctionConsumer(ASTContext *Context) : Visitor(Context) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  DeprecatedFunctionVisitor Visitor;
};

class DeprecatedFunctionAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 llvm::StringRef) override {
    return std::make_unique<DeprecatedFunctionConsumer>(&CI.getASTContext());
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<DeprecatedFunctionAction>
    X("deprecated_function_checker",
      "warn about functions containing 'deprecated' in their name");