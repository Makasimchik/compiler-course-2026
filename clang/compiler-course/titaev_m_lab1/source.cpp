#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/DenseMap.h"

using namespace clang;

namespace {

enum class ResKind { NewAlloc, MallocAlloc, FopenAlloc };

struct ResInfo {
  ResKind kind;
  SourceLocation where;
  const VarDecl *decl;
  bool isFreed = false;
  bool alreadyReported = false;
};

class ResourceLeakVisitor final
    : public RecursiveASTVisitor<ResourceLeakVisitor> {
public:
  explicit ResourceLeakVisitor(ASTContext *ctx) : ctx(ctx) {}

  bool VisitVarDecl(VarDecl *vd) {
    if (!vd->hasInit())
      return true;

    const Expr *init = vd->getInit();
    if (!init)
      return true;

    init = init->IgnoreParenImpCasts();
    init = init->IgnoreCasts();

    if (const auto *ne = dyn_cast<CXXNewExpr>(init)) {
      registerAlloc(vd, ResKind::NewAlloc, vd->getLocation());
    } else if (const auto *call = dyn_cast<CallExpr>(init)) {
      if (const FunctionDecl *fd = call->getDirectCallee()) {
        StringRef nm = fd->getName();
        if (nm == "malloc") {
          registerAlloc(vd, ResKind::MallocAlloc, vd->getLocation());
        } else if (nm == "fopen") {
          registerAlloc(vd, ResKind::FopenAlloc, vd->getLocation());
        }
      }
    }
    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *bo) {
    if (!bo->isAssignmentOp())
      return true;

    const Expr *lhs = bo->getLHS();
    const Expr *rhs = bo->getRHS();
    if (!lhs || !rhs)
      return true;

    lhs = lhs->IgnoreParenImpCasts();
    lhs = lhs->IgnoreCasts();
    rhs = rhs->IgnoreParenImpCasts();
    rhs = rhs->IgnoreCasts();

    const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(lhs);
    if (!ref)
      return true;

    const VarDecl *var = dyn_cast<VarDecl>(ref->getDecl());
    if (!var)
      return true;

    if (const auto *ne = dyn_cast<CXXNewExpr>(rhs)) {
      registerAlloc(var, ResKind::NewAlloc, var->getLocation());
    } else if (const auto *call = dyn_cast<CallExpr>(rhs)) {
      if (const FunctionDecl *fd = call->getDirectCallee()) {
        StringRef nm = fd->getName();
        if (nm == "malloc") {
          registerAlloc(var, ResKind::MallocAlloc, var->getLocation());
        } else if (nm == "fopen") {
          registerAlloc(var, ResKind::FopenAlloc, var->getLocation());
        }
      }
    }
    return true;
  }

  bool VisitCXXDeleteExpr(CXXDeleteExpr *del) {
    const Expr *arg = del->getArgument();
    if (!arg)
      return true;

    arg = arg->IgnoreParenImpCasts();
    arg = arg->IgnoreCasts();

    if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(arg)) {
      if (const VarDecl *var = dyn_cast<VarDecl>(ref->getDecl())) {
        markAsFreed(var);
      }
    }
    return true;
  }

  bool VisitCallExpr(CallExpr *call) {
    if (const FunctionDecl *fd = call->getDirectCallee()) {
      StringRef nm = fd->getName();
      if (nm == "free" || nm == "fclose") {
        if (call->getNumArgs() >= 1) {
          const Expr *arg = call->getArg(0);
          if (!arg)
            return true;

          arg = arg->IgnoreParenImpCasts();
          arg = arg->IgnoreCasts();

          if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(arg)) {
            if (const VarDecl *var = dyn_cast<VarDecl>(ref->getDecl())) {
              markAsFreed(var);
            }
          }
        }
      }
    }
    return true;
  }

  const FunctionDecl *getParentFunction(const Stmt *st) {
    if (!st)
      return nullptr;

    DynTypedNode node = DynTypedNode::create(*st);

    while (true) {
      auto parents = ctx->getParents(node);
      if (parents.empty())
        break;

      node = parents[0];

      if (const FunctionDecl *fd = node.get<FunctionDecl>())
        return fd;
    }
    return nullptr;
  }

  bool VisitReturnStmt(ReturnStmt *rs) {
    const Expr *ret = rs->getRetValue();

    if (ret) {
      ret = ret->IgnoreParenImpCasts();
      ret = ret->IgnoreCasts();

      if (const DeclRefExpr *ref = dyn_cast<DeclRefExpr>(ret)) {
        if (const VarDecl *var = dyn_cast<VarDecl>(ref->getDecl())) {
          auto it = allocations.find(var);
          if (it != allocations.end() && !it->second.isFreed &&
              !it->second.alreadyReported) {

            DiagnosticsEngine &DE = ctx->getDiagnostics();
            unsigned id = DE.getCustomDiagID(
                DiagnosticsEngine::Warning,
                "Ресурс для переменной '%0' может быть не освобожден (не "
                "гарантированное освобождение при return)!");
            DE.Report(rs->getBeginLoc(), id) << var->getName();

            it->second.alreadyReported = true;
          }
        }
      }
    } else {
      const FunctionDecl *fd = getParentFunction(rs);
      if (!fd)
        return true;

      for (auto &pair : allocations) {
        ResInfo &info = pair.second;

        if (info.isFreed || info.alreadyReported)
          continue;

        const DeclContext *dc = info.decl->getDeclContext();
        const DeclContext *cur = dc;

        bool sameFunc = false;
        while (cur) {
          if (cur == fd) {
            sameFunc = true;
            break;
          }
          cur = cur->getParent();
        }

        if (!sameFunc)
          continue;

        SourceManager &SM = ctx->getSourceManager();

        if (SM.isBeforeInTranslationUnit(info.decl->getLocation(),
                                         rs->getBeginLoc())) {

          DiagnosticsEngine &DE = ctx->getDiagnostics();
          unsigned id = DE.getCustomDiagID(
              DiagnosticsEngine::Warning,
              "Ресурс для переменной '%0' может быть не освобожден (не "
              "гарантированное освобождение при return)!");
          DE.Report(rs->getBeginLoc(), id) << info.decl->getName();

          info.alreadyReported = true;
        }
      }
    }
    return true;
  }

  void finalizeDiagnostics() {
    DiagnosticsEngine &DE = ctx->getDiagnostics();

    for (const auto &pair : allocations) {
      const ResInfo &info = pair.second;

      if (!info.isFreed && !info.alreadyReported) {
        unsigned id = DE.getCustomDiagID(
            DiagnosticsEngine::Warning,
            "Память или ресурс для переменной '%0' не освобождены!");
        DE.Report(info.where, id) << info.decl->getName();
      }
    }
  }

private:
  ASTContext *ctx;
  llvm::DenseMap<const VarDecl *, ResInfo> allocations;

  void registerAlloc(const VarDecl *vd, ResKind k, SourceLocation loc) {
    ResInfo info;
    info.kind = k;
    info.where = loc;
    info.decl = vd;
    allocations[vd] = info;
  }

  void markAsFreed(const VarDecl *vd) {
    auto it = allocations.find(vd);
    if (it != allocations.end()) {
      it->second.isFreed = true;
    }
  }
};

class ResourceLeakConsumer final : public ASTConsumer {
public:
  explicit ResourceLeakConsumer(ASTContext *ctx) : visitor(ctx) {}

  void HandleTranslationUnit(ASTContext &ctx) override {
    visitor.TraverseDecl(ctx.getTranslationUnitDecl());
    visitor.finalizeDiagnostics();
  }

private:
  ResourceLeakVisitor visitor;
};

class ResourceLeakAction final : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &ci,
                                                 llvm::StringRef) override {
    return std::make_unique<ResourceLeakConsumer>(&ci.getASTContext());
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<ResourceLeakAction>
    Z("leak_checker", "TU-level leak checker");