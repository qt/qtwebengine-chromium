///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SemaHLSLDiagnoseTU.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file implements the Translation Unit Diagnose for HLSL.             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilShaderModel.h"
#include "dxc/Support/Global.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Sema/SemaHLSL.h"
#include "llvm/Support/Debug.h"

using namespace clang;
using namespace hlsl;

//
// This is similar to clang/Analysis/CallGraph, but the following differences
// motivate this:
//
// - track traversed vs. observed nodes explicitly
// - fully visit all reachable functions
// - merge graph visiting with checking for recursion
// - track global variables and types used (NYI)
//
namespace {
struct CallNode {
  FunctionDecl *CallerFn;
  ::llvm::SmallPtrSet<FunctionDecl *, 4> CalleeFns;
};
typedef ::llvm::DenseMap<FunctionDecl *, CallNode> CallNodes;
typedef ::llvm::SmallPtrSet<Decl *, 8> FnCallStack;
typedef ::llvm::SmallPtrSet<FunctionDecl *, 128> FunctionSet;
typedef ::llvm::SmallVector<FunctionDecl *, 32> PendingFunctions;
typedef ::llvm::DenseMap<FunctionDecl *, FunctionDecl *> FunctionMap;

// Returns the definition of a function.
// This serves two purposes - ignore built-in functions, and pick
// a single Decl * to be used in maps and sets.
FunctionDecl *getFunctionWithBody(FunctionDecl *F) {
  if (!F)
    return nullptr;
  if (F->doesThisDeclarationHaveABody())
    return F;
  F = F->getFirstDecl();
  for (auto &&Candidate : F->redecls()) {
    if (Candidate->doesThisDeclarationHaveABody()) {
      return Candidate;
    }
  }
  return nullptr;
}

// AST visitor that maintains visited and pending collections, as well
// as recording nodes of caller/callees.
class FnReferenceVisitor : public RecursiveASTVisitor<FnReferenceVisitor> {
private:
  CallNodes &m_callNodes;
  FunctionSet &m_visitedFunctions;
  PendingFunctions &m_pendingFunctions;
  FunctionDecl *m_source;
  CallNodes::iterator m_sourceIt;

public:
  FnReferenceVisitor(FunctionSet &visitedFunctions,
                     PendingFunctions &pendingFunctions, CallNodes &callNodes)
      : m_callNodes(callNodes), m_visitedFunctions(visitedFunctions),
        m_pendingFunctions(pendingFunctions) {}

  void setSourceFn(FunctionDecl *F) {
    F = getFunctionWithBody(F);
    m_source = F;
    m_sourceIt = m_callNodes.find(F);
  }

  bool VisitDeclRefExpr(DeclRefExpr *ref) {
    ValueDecl *valueDecl = ref->getDecl();
    RecordFunctionDecl(dyn_cast_or_null<FunctionDecl>(valueDecl));
    return true;
  }

  bool VisitCXXMemberCallExpr(CXXMemberCallExpr *callExpr) {
    RecordFunctionDecl(callExpr->getMethodDecl());
    return true;
  }

  void RecordFunctionDecl(FunctionDecl *funcDecl) {
    funcDecl = getFunctionWithBody(funcDecl);
    if (funcDecl) {
      if (m_sourceIt == m_callNodes.end()) {
        auto result = m_callNodes.insert(
            std::make_pair(m_source, CallNode{m_source, {}}));
        DXASSERT(result.second == true,
                 "else setSourceFn didn't assign m_sourceIt");
        m_sourceIt = result.first;
      }
      m_sourceIt->second.CalleeFns.insert(funcDecl);
      if (!m_visitedFunctions.count(funcDecl)) {
        m_pendingFunctions.push_back(funcDecl);
      }
    }
  }
};

// A call graph that can check for reachability and recursion efficiently.
class CallGraphWithRecurseGuard {
private:
  CallNodes m_callNodes;
  FunctionSet m_visitedFunctions;
  FunctionMap m_functionsCheckedForRecursion;

  FunctionDecl *CheckRecursion(FnCallStack &CallStack, FunctionDecl *D) {
    auto it = m_functionsCheckedForRecursion.find(D);
    if (it != m_functionsCheckedForRecursion.end())
      return it->second;
    if (CallStack.insert(D).second == false)
      return D;
    auto node = m_callNodes.find(D);
    if (node != m_callNodes.end()) {
      for (FunctionDecl *Callee : node->second.CalleeFns) {
        FunctionDecl *pResult = CheckRecursion(CallStack, Callee);
        if (pResult) {
          m_functionsCheckedForRecursion[D] = pResult;
          return pResult;
        }
      }
    }
    CallStack.erase(D);
    m_functionsCheckedForRecursion[D] = nullptr;
    return nullptr;
  }

public:
  void BuildForEntry(FunctionDecl *EntryFnDecl) {
    DXASSERT_NOMSG(EntryFnDecl);
    EntryFnDecl = getFunctionWithBody(EntryFnDecl);
    PendingFunctions pendingFunctions;
    FnReferenceVisitor visitor(m_visitedFunctions, pendingFunctions,
                               m_callNodes);
    pendingFunctions.push_back(EntryFnDecl);
    while (!pendingFunctions.empty()) {
      FunctionDecl *pendingDecl = pendingFunctions.pop_back_val();
      if (m_visitedFunctions.insert(pendingDecl).second == true) {
        visitor.setSourceFn(pendingDecl);
        visitor.TraverseDecl(pendingDecl);
      }
    }
  }

  // return true if FD2 is reachable from FD1
  bool CheckReachability(FunctionDecl *FD1, FunctionDecl *FD2) {
    if (FD1 == FD2)
      return true;
    auto node = m_callNodes.find(FD1);
    if (node != m_callNodes.end()) {
      for (FunctionDecl *Callee : node->second.CalleeFns) {
        if (CheckReachability(Callee, FD2))
          return true;
      }
    }
    return false;
  }

  FunctionDecl *CheckRecursion(FunctionDecl *EntryFnDecl) {
    FnCallStack CallStack;
    EntryFnDecl = getFunctionWithBody(EntryFnDecl);
    return CheckRecursion(CallStack, EntryFnDecl);
  }

  const CallNodes &GetCallGraph() { return m_callNodes; }

  void dump() const {
    llvm::dbgs() << "Call Nodes:\n";
    for (auto &node : m_callNodes) {
      llvm::dbgs() << node.first->getName().str().c_str() << " ["
                   << (void *)node.first << "]:\n";
      for (auto callee : node.second.CalleeFns) {
        llvm::dbgs() << "    " << callee->getName().str().c_str() << " ["
                     << (void *)callee << "]\n";
      }
    }
  }
};

struct NameLookup {
  FunctionDecl *Found;
  FunctionDecl *Other;
};

NameLookup GetSingleFunctionDeclByName(clang::Sema *self, StringRef Name,
                                       bool checkPatch) {
  auto DN = DeclarationName(&self->getASTContext().Idents.get(Name));
  FunctionDecl *pFoundDecl = nullptr;
  for (auto idIter = self->IdResolver.begin(DN), idEnd = self->IdResolver.end();
       idIter != idEnd; ++idIter) {
    FunctionDecl *pFnDecl = dyn_cast<FunctionDecl>(*idIter);
    if (!pFnDecl)
      continue;
    if (checkPatch &&
        !self->getASTContext().IsPatchConstantFunctionDecl(pFnDecl))
      continue;
    if (pFoundDecl) {
      return NameLookup{pFoundDecl, pFnDecl};
    }
    pFoundDecl = pFnDecl;
  }
  return NameLookup{pFoundDecl, nullptr};
}

bool IsTargetProfileLib6x(Sema &S) {
  // Remaining functions are exported only if target is 'lib_6_x'.
  const hlsl::ShaderModel *SM =
      hlsl::ShaderModel::GetByName(S.getLangOpts().HLSLProfile.c_str());
  bool isLib6x =
      SM->IsLib() && SM->GetMinor() == hlsl::ShaderModel::kOfflineMinor;
  return isLib6x;
}

bool IsExported(Sema *self, clang::FunctionDecl *FD,
                bool isDefaultLinkageExternal) {
  // Entry points are exported.
  if (FD->hasAttr<HLSLShaderAttr>())
    return true;

  // Internal linkage functions include functions marked 'static'.
  if (FD->getLinkageAndVisibility().getLinkage() == InternalLinkage)
    return false;

  // Explicit 'export' functions are exported.
  if (FD->hasAttr<HLSLExportAttr>())
    return true;

  return isDefaultLinkageExternal;
}

bool getDefaultLinkageExternal(clang::Sema *self) {
  const LangOptions &opts = self->getLangOpts();
  bool isDefaultLinkageExternal =
      opts.DefaultLinkage == DXIL::DefaultLinkage::External;
  if (opts.DefaultLinkage == DXIL::DefaultLinkage::Default &&
      !opts.ExportShadersOnly && IsTargetProfileLib6x(*self))
    isDefaultLinkageExternal = true;
  return isDefaultLinkageExternal;
}

std::vector<FunctionDecl *> GetAllExportedFDecls(clang::Sema *self) {
  // Add to the end, process from the beginning, to ensure AllExportedFDecls
  // will contain functions in decl order.
  std::vector<FunctionDecl *> AllExportedFDecls;

  std::deque<DeclContext *> Worklist;
  Worklist.push_back(self->getASTContext().getTranslationUnitDecl());
  while (Worklist.size()) {
    DeclContext *DC = Worklist.front();
    Worklist.pop_front();
    if (auto *FD = dyn_cast<FunctionDecl>(DC)) {
      AllExportedFDecls.push_back(FD);
    } else {
      for (auto *D : DC->decls()) {
        if (auto *FD = dyn_cast<FunctionDecl>(D)) {
          if (FD->hasBody() &&
              IsExported(self, FD, getDefaultLinkageExternal(self)))
            Worklist.push_back(FD);
        } else if (auto *DC2 = dyn_cast<DeclContext>(D)) {
          Worklist.push_back(DC2);
        }
      }
    }
  }

  return AllExportedFDecls;
}

// in the non-library case, this function will be run only once,
// but in the library case, this function will be run for each
// viable top-level function declaration by
// ValidateNoRecursionInTranslationUnit.
//  (viable as in, is exported)
clang::FunctionDecl *ValidateNoRecursion(CallGraphWithRecurseGuard &callGraph,
                                         clang::FunctionDecl *FD) {
  // Validate that there is no recursion reachable by this function declaration
  // NOTE: the information gathered here could be used to bypass code generation
  // on functions that are unreachable (as an early form of dead code
  // elimination).
  if (FD) {
    callGraph.BuildForEntry(FD);
    return callGraph.CheckRecursion(FD);
  }
  return nullptr;
}

} // namespace

void hlsl::DiagnoseTranslationUnit(clang::Sema *self) {
  DXASSERT_NOMSG(self != nullptr);

  // Don't bother with global validation if compilation has already failed.
  if (self->getDiagnostics().hasErrorOccurred()) {
    return;
  }

  // Check RT shader if available for their payload use and match payload access
  // against availiable payload modifiers.
  // We have to do it late because we could have payload access in a called
  // function and have to check the callgraph if the root shader has the right
  // access rights to the payload structure.
  if (self->getLangOpts().IsHLSLLibrary) {
    if (self->getLangOpts().EnablePayloadAccessQualifiers) {
      ASTContext &ctx = self->getASTContext();
      TranslationUnitDecl *TU = ctx.getTranslationUnitDecl();
      DiagnoseRaytracingPayloadAccess(*self, TU);
    }
  }

  // TODO: make these error 'real' errors rather than on-the-fly things
  // Validate that the entry point is available.
  DiagnosticsEngine &Diags = self->getDiagnostics();
  FunctionDecl *pEntryPointDecl = nullptr;
  std::vector<FunctionDecl *> FDeclsToCheck;
  if (self->getLangOpts().IsHLSLLibrary) {
    FDeclsToCheck = GetAllExportedFDecls(self);
  } else {
    const std::string &EntryPointName = self->getLangOpts().HLSLEntryFunction;
    if (!EntryPointName.empty()) {
      NameLookup NL = GetSingleFunctionDeclByName(self, EntryPointName,
                                                  /*checkPatch*/ false);
      if (NL.Found && NL.Other) {
        // NOTE: currently we cannot hit this codepath when CodeGen is enabled,
        // because CodeGenModule::getMangledName will mangle the entry point
        // name into the bare string, and so ambiguous points will produce an
        // error earlier on.
        unsigned id =
            Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
                                  "ambiguous entry point function");
        Diags.Report(NL.Found->getSourceRange().getBegin(), id);
        Diags.Report(NL.Other->getLocation(), diag::note_previous_definition);
        return;
      }
      pEntryPointDecl = NL.Found;
      if (!pEntryPointDecl || !pEntryPointDecl->hasBody()) {
        unsigned id =
            Diags.getCustomDiagID(clang::DiagnosticsEngine::Level::Error,
                                  "missing entry point definition");
        Diags.Report(id);
        return;
      }
      FDeclsToCheck.push_back(NL.Found);
    }
  }

  CallGraphWithRecurseGuard callGraph;
  std::set<FunctionDecl *> DiagnosedDecls;
  // for each FDecl, check for recursion
  for (FunctionDecl *FDecl : FDeclsToCheck) {
    FunctionDecl *result = ValidateNoRecursion(callGraph, FDecl);

    if (result) {
      // don't emit duplicate diagnostics for the same recursive function
      // if A and B call recursive function C, only emit 1 diagnostic for C.
      if (DiagnosedDecls.find(result) == DiagnosedDecls.end()) {
        DiagnosedDecls.insert(result);
        self->Diag(result->getSourceRange().getBegin(),
                   diag::err_hlsl_no_recursion)
            << FDecl->getQualifiedNameAsString()
            << result->getQualifiedNameAsString();
        self->Diag(result->getSourceRange().getBegin(),
                   diag::note_hlsl_no_recursion);
      }
    }

    FunctionDecl *pPatchFnDecl = nullptr;
    if (const HLSLPatchConstantFuncAttr *attr =
            FDecl->getAttr<HLSLPatchConstantFuncAttr>()) {
      NameLookup NL = GetSingleFunctionDeclByName(self, attr->getFunctionName(),
                                                  /*checkPatch*/ true);
      if (!NL.Found || !NL.Found->hasBody()) {
        self->Diag(attr->getLocation(),
                   diag::err_hlsl_missing_patch_constant_function)
            << attr->getFunctionName();
      }
      pPatchFnDecl = NL.Found;
    }

    if (pPatchFnDecl) {
      FunctionDecl *patchResult = ValidateNoRecursion(callGraph, pPatchFnDecl);

      // In this case, recursion was detected in the patch-constant function
      if (patchResult) {
        if (DiagnosedDecls.find(patchResult) == DiagnosedDecls.end()) {
          DiagnosedDecls.insert(patchResult);
          self->Diag(patchResult->getSourceRange().getBegin(),
                     diag::err_hlsl_no_recursion)
              << pPatchFnDecl->getQualifiedNameAsString()
              << patchResult->getQualifiedNameAsString();
          self->Diag(patchResult->getSourceRange().getBegin(),
                     diag::note_hlsl_no_recursion);
        }
      }

      // The patch function decl and the entry function decl should be
      // disconnected with respect to the call graph.
      // Only check this if neither function decl is recursive
      if (!result && !patchResult) {
        CallGraphWithRecurseGuard CG;
        CG.BuildForEntry(pPatchFnDecl);
        if (CG.CheckReachability(pPatchFnDecl, FDecl)) {
          self->Diag(FDecl->getSourceRange().getBegin(),
                     diag::err_hlsl_patch_reachability_not_allowed)
              << 1 << FDecl->getName() << 0 << pPatchFnDecl->getName();
        }
        CG.BuildForEntry(FDecl);
        if (CG.CheckReachability(FDecl, pPatchFnDecl)) {
          self->Diag(FDecl->getSourceRange().getBegin(),
                     diag::err_hlsl_patch_reachability_not_allowed)
              << 0 << pPatchFnDecl->getName() << 1 << FDecl->getName();
        }
      }
    }
  }
}
