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

#include "llvm/ADT/StringRef.h"

#include <string>

namespace clang {

class CallsiteWrapperScope {
  std::string NamePrefix;

public:
  bool Empty() { return NamePrefix.empty(); }

  llvm::StringRef GetNamePrefix() { return NamePrefix; }

  explicit CallsiteWrapperScope() = default;

  class Guard;

private:
  explicit CallsiteWrapperScope(std::string NamePrefix)
      : NamePrefix(std::move(NamePrefix)) {}
};

class CallsiteWrapperScope::Guard {
public:
  Guard(std::string NamePrefix, CallsiteWrapperScope &Current)
      : Current(Current), OldVal(Current) {
    Current = CallsiteWrapperScope(std::move(NamePrefix));
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
