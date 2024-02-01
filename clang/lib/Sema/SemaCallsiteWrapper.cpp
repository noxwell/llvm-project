//===--- SemaCallsiteWrapper.cpp - Callsite wrapper functions -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis functions specific to callsite
//  wrapper attribute.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclarationName.h"
#include "clang/Basic/LLVM.h"
#include "clang/Sema/DeclSpec.h"
#include "clang/Sema/Ownership.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/Template.h"
#include "clang/Sema/TemplateDeduction.h"

using namespace clang;

ExprResult Sema::BuildCallsiteWrapperDeclarationNameExpr(
    const CXXScopeSpec &SS, const DeclarationNameInfo &NameInfo,
    FunctionDecl *FD, bool AcceptInvalidDecl) {
  assert(FD->hasAttr<CallsiteWrapperAttr>());

  // Put instantiated callsite wrapper to the same context as the original
  // callsite wrapper template.
  DeclContext *DC = FD->getDeclContext();

  SourceLocation CalleeLoc = NameInfo.getLoc();
  DeclContext *CalleeContext = CurContext;
  if (const auto *FD = dyn_cast<FunctionDecl>(CurContext)) {
    if (auto *CWSI = FD->getCallsiteWrapperSpecializationInfo()) {
      CalleeLoc = CWSI->getPointOfInstantiation();
      CalleeContext = CWSI->getCalleeContext();
    }
  }
  InstantiatingTemplate Inst(*this,
                             /*PointOfInstantiation=*/CalleeLoc,
                             /*Entity=*/FD);
  MultiLevelTemplateArgumentList EmptyArgs;
  FunctionDecl *NewFD =
      cast_or_null<FunctionDecl>(SubstDecl(FD, DC, EmptyArgs));
  if (!NewFD) {
    // Recovery from invalid cases (e.g. FD is an invalid Decl).
    return CreateRecoveryExpr(NameInfo.getBeginLoc(), NameInfo.getEndLoc(), {});
  }
  DC->addDecl(NewFD);
  auto *CWSI = NewFD->getCallsiteWrapperSpecializationInfo();
  assert(CWSI && "Cloned function is not a callsite wrapper specialization");
  CWSI->setPointOfInstantiation(CalleeLoc);
  CWSI->setCalleeContext(CalleeContext);
  // Instantiate immediately, so that nested callsite wrapper calls are
  // attributed to CurrentCallsiteWrapperInstantiationContext.
  InstantiateFunctionDefinition(CalleeLoc, NewFD);
  return BuildDeclarationNameExpr(SS, NewFD->getNameInfo(), NewFD, NewFD,
                                  nullptr, AcceptInvalidDecl);
}
