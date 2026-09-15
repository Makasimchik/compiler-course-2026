#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ParentMapContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace clang;

namespace {

struct MutationState {
  bool VariableChanged = false;
  bool ObjectChanged = false;
};

class ConstifyVisitor : public RecursiveASTVisitor<ConstifyVisitor> {
public:
  ConstifyVisitor(ASTContext &Context, Rewriter &Rewrite)
      : Context(Context), Rewrite(Rewrite) {}

  bool VisitVarDecl(VarDecl *Decl) {
    if (isCandidate(Decl) && Seen.insert(Decl).second)
      Candidates.push_back(Decl);
    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *Operator) {
    if (Operator->isAssignmentOp())
      markMutation(Operator->getLHS(), true, false);
    return true;
  }

  bool VisitUnaryOperator(UnaryOperator *Operator) {
    if (Operator->isIncrementDecrementOp())
      markMutation(Operator->getSubExpr(), true, false);
    return true;
  }

  bool VisitCallExpr(CallExpr *Call) {
    const FunctionDecl *Callee = Call->getDirectCallee();
    if (!Callee)
      return true;

    const unsigned Count = std::min(Call->getNumArgs(), Callee->getNumParams());
    for (unsigned I = 0; I < Count; ++I)
      markCallArgument(Call->getArg(I), Callee->getParamDecl(I)->getType());

    return true;
  }

  bool VisitCXXMemberCallExpr(CXXMemberCallExpr *Call) {
    const CXXMethodDecl *Method = Call->getMethodDecl();
    if (!Method || Method->isConst())
      return true;

    markMutation(Call->getImplicitObjectArgument(), false, true);
    return true;
  }

  void applyRewrites() {
    for (const VarDecl *Decl : Candidates) {
      const std::optional<std::string> Replacement = buildReplacementType(Decl);
      if (!Replacement)
        continue;

      replaceType(Decl, *Replacement);
    }
  }

private:
  ASTContext &Context;
  Rewriter &Rewrite;
  llvm::DenseMap<const VarDecl *, MutationState> Mutations;
  llvm::DenseSet<const VarDecl *> Seen;
  std::vector<const VarDecl *> Candidates;

  Expr *ignoreWrappers(Expr *Expression) const {
    return Expression ? Expression->IgnoreParenImpCasts() : nullptr;
  }

  bool isInMainFile(SourceLocation Location) const {
    if (Location.isInvalid() || Location.isMacroID())
      return false;

    const SourceManager &Sources = Context.getSourceManager();
    return Sources.isWrittenInMainFile(Sources.getSpellingLoc(Location));
  }

  const FunctionDecl *getEnclosingFunction(const Decl *DeclNode) const {
    const DeclContext *Current = DeclNode->getDeclContext();
    while (Current) {
      if (const auto *Function = dyn_cast<FunctionDecl>(Current))
        return Function;
      Current = Current->getParent();
    }
    return nullptr;
  }

  bool hasSupportedType(const VarDecl *Decl) const {
    const QualType Type = Decl->getType();

    if (Type->isPointerType())
      return !Type->getPointeeType()->isFunctionType();

    if (!Type->isLValueReferenceType())
      return false;

    const QualType Referred = Type.getNonReferenceType();
    return !Referred->isFunctionType() && !Referred->isPointerType();
  }

  bool isCandidate(const VarDecl *Decl) const {
    if (!Decl || Decl->getType().isNull() || !Decl->getIdentifier())
      return false;

    if (!hasSupportedType(Decl))
      return false;

    if (!hasSingleDeclarator(Decl))
      return false;

    if (!isInMainFile(Decl->getBeginLoc()) ||
        !isInMainFile(Decl->getLocation()))
      return false;

    if (const auto *Param = dyn_cast<ParmVarDecl>(Decl)) {
      const auto *Function = dyn_cast<FunctionDecl>(Param->getDeclContext());
      return Function && Function->isThisDeclarationADefinition();
    }

    if (!Decl->isLocalVarDecl() || Decl->isStaticLocal())
      return false;

    const FunctionDecl *Function = getEnclosingFunction(Decl);
    return Function && Function->hasBody();
  }

  bool canMakePointerConst(const VarDecl *Decl) const {
    return isa<ParmVarDecl>(Decl) || Decl->hasInit();
  }

  bool hasSingleDeclarator(const VarDecl *Decl) const {
    if (isa<ParmVarDecl>(Decl))
      return true;

    const auto Parents = Context.getParents(*Decl);
    for (const auto &Parent : Parents) {
      if (const auto *Statement = Parent.get<DeclStmt>())
        return Statement->isSingleDecl();
    }

    return true;
  }

  void rememberMutation(const VarDecl *Decl, bool VariableChanged,
                        bool ObjectChanged) {
    if (!Decl)
      return;

    MutationState &State = Mutations[Decl];
    if (Decl->getType()->isLValueReferenceType()) {
      State.ObjectChanged |= VariableChanged || ObjectChanged;
      return;
    }

    State.VariableChanged |= VariableChanged;
    State.ObjectChanged |= ObjectChanged;
  }

  void markMutation(Expr *Expression, bool VariableChanged,
                    bool ObjectChanged) {
    Expression = ignoreWrappers(Expression);
    if (!Expression)
      return;

    if (auto *Reference = dyn_cast<DeclRefExpr>(Expression)) {
      if (const auto *Decl = dyn_cast<VarDecl>(Reference->getDecl()))
        rememberMutation(Decl, VariableChanged, ObjectChanged);
      return;
    }

    if (auto *Operator = dyn_cast<UnaryOperator>(Expression)) {
      if (Operator->getOpcode() == UO_Deref)
        markMutation(Operator->getSubExpr(), false, true);
      else
        markMutation(Operator->getSubExpr(), VariableChanged, ObjectChanged);
      return;
    }

    if (auto *Subscript = dyn_cast<ArraySubscriptExpr>(Expression)) {
      markMutation(Subscript->getBase(), false, true);
      return;
    }

    if (auto *Member = dyn_cast<MemberExpr>(Expression)) {
      markMutation(Member->getBase(), false, true);
      return;
    }
  }

  void markCallArgument(Expr *Argument, QualType ParameterType) {
    if (ParameterType->isPointerType()) {
      if (!ParameterType->getPointeeType().isConstQualified())
        markMutation(Argument, false, true);
      return;
    }

    if (!ParameterType->isLValueReferenceType())
      return;

    const QualType Referred = ParameterType.getNonReferenceType();
    if (Referred.isConstQualified())
      return;

    if (Referred->isPointerType())
      markMutation(Argument, true, true);
    else
      markMutation(Argument, true, false);
  }

  std::optional<std::string> buildReplacementType(const VarDecl *Decl) const {
    const MutationState State = Mutations.lookup(Decl);
    const QualType Type = Decl->getType();
    PrintingPolicy Policy(Context.getLangOpts());

    if (Type->isLValueReferenceType()) {
      const QualType Referred = Type.getNonReferenceType();
      if (State.ObjectChanged || Referred.isConstQualified())
        return std::nullopt;

      const QualType Replacement =
          Context.getLValueReferenceType(Context.getConstType(Referred));
      return Replacement.getAsString(Policy);
    }

    if (!Type->isPointerType())
      return std::nullopt;

    const bool NeedConstObject =
        !State.ObjectChanged && !Type->getPointeeType().isConstQualified();
    const bool NeedConstPointer = !State.VariableChanged &&
                                  !Type.isLocalConstQualified() &&
                                  canMakePointerConst(Decl);
    if (!NeedConstObject && !NeedConstPointer)
      return std::nullopt;

    QualType Pointee = Type->getPointeeType();
    if (NeedConstObject)
      Pointee = Context.getConstType(Pointee);

    QualType Replacement = Context.getPointerType(Pointee);
    if (NeedConstPointer)
      Replacement = Context.getConstType(Replacement);

    return Replacement.getAsString(Policy);
  }

  void replaceType(const VarDecl *Decl, llvm::StringRef Replacement) {
    TypeSourceInfo *TypeInfo = Decl->getTypeSourceInfo();
    if (!TypeInfo)
      return;

    const SourceRange Range = TypeInfo->getTypeLoc().getSourceRange();
    if (Range.isValid())
      Rewrite.ReplaceText(Range, Replacement);
  }
};

class ConstifyConsumer : public ASTConsumer {
public:
  void HandleTranslationUnit(ASTContext &Context) override {
    Rewriter Rewrite;
    Rewrite.setSourceMgr(Context.getSourceManager(), Context.getLangOpts());

    ConstifyVisitor Visitor(Context, Rewrite);
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    Visitor.applyRewrites();

    const FileID MainFile = Context.getSourceManager().getMainFileID();
    if (const llvm::RewriteBuffer *Buffer =
            Rewrite.getRewriteBufferFor(MainFile)) {
      Buffer->write(llvm::outs());
      return;
    }

    bool Invalid = false;
    const llvm::StringRef Original =
        Context.getSourceManager().getBufferData(MainFile, &Invalid);
    if (!Invalid)
      llvm::outs() << Original;
  }
};

class ConstifyAction : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &,
                                                 llvm::StringRef) override {
    return std::make_unique<ConstifyConsumer>();
  }

  bool ParseArgs(const CompilerInstance &,
                 const std::vector<std::string> &) override {
    return true;
  }
};

} // namespace

static FrontendPluginRegistry::Add<ConstifyAction>
    X("egashin_k_const_plugin",
      "adds const qualifiers to unchanged local pointers and references");
