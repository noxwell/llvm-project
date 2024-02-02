//===--- CallsiteWraperScope.h ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines types used to track the callsite wrapper scope.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_CALLSITEWRAPPERSCOPE_H
#define LLVM_CLANG_AST_CALLSITEWRAPPERSCOPE_H

namespace clang {

class Decl;
class Expr;

class CallsiteWrapperScope {
  const Decl *WrapperDecl = nullptr;
  const Expr *CallsiteExpr = nullptr;

public:
  bool Empty() { return WrapperDecl == nullptr; }

  const Decl *GetWrapperDecl() { return WrapperDecl; }
  const Expr *GetCallsiteExpr() { return CallsiteExpr; }

  explicit CallsiteWrapperScope() = default;

  class Guard;

private:
  explicit CallsiteWrapperScope(const Decl *WrapperDecl,
                                const Expr *CallsiteExpr)
      : WrapperDecl(WrapperDecl), CallsiteExpr(CallsiteExpr) {}
};

class CallsiteWrapperScope::Guard {
public:
  Guard(const Decl *WrapperDecl, const Expr *CallsiteExpr,
        CallsiteWrapperScope &Current)
      : Current(Current), OldVal(Current) {
    Current = CallsiteWrapperScope(WrapperDecl, CallsiteExpr);
  }

  ~Guard() { Current = OldVal; }

private:
  Guard(Guard const &) = delete;
  Guard &operator=(Guard const &) = delete;

  CallsiteWrapperScope &Current;
  CallsiteWrapperScope OldVal;
};

} // end namespace clang

#endif // LLVM_CLANG_AST_CALLSITEWRAPPERSCOPE_H
