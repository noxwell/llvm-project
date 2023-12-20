//===---- tools/extra/ToolTemplate.cpp - Template for refactoring tool ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements an empty refactoring tool using the clang tooling.
//  The goal is to lower the "barrier to entry" for writing refactoring tools.
//
//  Usage:
//  tool-template <cmake-output-dir> <file1> <file2> ...
//
//  Where <cmake-output-dir> is a CMake build directory in which a file named
//  compile_commands.json exists (enable -DCMAKE_EXPORT_COMPILE_COMMANDS in
//  CMake to get this output).
//
//  <file1> ... specify the paths of files in the CMake source tree. This path
//  is looked up in the compile command database. If the path of a file is
//  absolute, it needs to point into CMake's source tree. If the path is
//  relative, the current working directory needs to be in the CMake source
//  tree and the file must be in a subdirectory of the current working
//  directory. "./" prefixes in the relative files will be automatically
//  removed, but the rest of a relative path must be a suffix of a path in
//  the compile command line database.
//
//  For example, to use tool-template on all files in a subtree of the
//  source tree, use:
//
//    /path/in/subtree $ find . -name '*.cpp'|
//        xargs tool-template /path/to/build
//
//===----------------------------------------------------------------------===//

#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Signals.h"

AST_MATCHER(clang::Decl, hasCallsiteWrappedByAttr) {
  if (auto *AA = Node.getAttr<clang::AnnotateAttr>()) {
    if (AA->getAnnotation() == "callsite_wrapped_by")
      return true;
  }
  return false;
}

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;
using namespace llvm;

namespace {

// FIXME: error-handling
/// Replaces one range of source code by another.
static void
addReplacement(SourceRange Old, StringRef Text, const ASTContext &Context,
               std::map<std::string, tooling::Replacements> &Replacements) {
  tooling::Replacement R(Context.getSourceManager(),
                         CharSourceRange::getTokenRange(Old), Text,
                         Context.getLangOpts());
  consumeError(Replacements[std::string(R.getFilePath())].add(R));
}

// FIXME: error-handling
/// Inserts source range into destination with optional argument delimiter before.
static void
addInsertion(SourceLocation Dest, StringRef Text, const ASTContext &Context,
             std::map<std::string, tooling::Replacements> &Replacements) {
  tooling::Replacement R(Context.getSourceManager(), Dest, 0, Text);
  consumeError(Replacements[std::string(R.getFilePath())].add(R));
}

static StringRef textFromSourceRange(SourceRange Source,
                                     const ASTContext &Context) {
  return Lexer::getSourceText(CharSourceRange::getTokenRange(Source),
                              Context.getSourceManager(),
                              Context.getLangOpts());
}

class CallsiteWrapperCallback : public MatchFinder::MatchCallback {
public:
  CallsiteWrapperCallback(
      std::map<std::string, tooling::Replacements> &Replacements)
      : Replacements(Replacements) {}

  // FIXME: error-handling instead of asserts
  void run(const MatchFinder::MatchResult &Result) override {
    auto *CE = Result.Nodes.getNodeAs<CallExpr>("callsite_wrapped_by");
    assert(CE);
    auto *Callee = CE->getCallee();
    assert(Callee);
    auto *CalleeDecl = CE->getCalleeDecl();
    assert(CalleeDecl);
    auto *AA = CalleeDecl->getAttr<clang::AnnotateAttr>();
    assert(AA);
    assert(AA->args_size() == 2);
    auto *CallsiteWrapper = *AA->args().begin();
    assert(CallsiteWrapper);
    auto *CallsiteTag = dyn_cast<ConstantExpr>(*(AA->args().begin() + 1));
    assert(CallsiteTag);
    auto *CallsiteTagICE = dyn_cast<ImplicitCastExpr>(CallsiteTag->getSubExpr());
    assert(CallsiteTagICE);
    auto *CallsiteTagDeclRef = dyn_cast<DeclRefExpr>(CallsiteTagICE->getSubExpr());
    assert(CallsiteTagDeclRef);
    auto *CallsiteTagDecl = dyn_cast<FunctionDecl>(CallsiteTagDeclRef->getDecl());
    assert(CallsiteTagDecl);
    auto *CallsiteTagStmt = dyn_cast<CompoundStmt>(CallsiteTagDecl->getBody());
    assert(CallsiteTagStmt);

    if (Callee->getSourceRange().isValid() && CE->getRParenLoc().isValid() && CallsiteTag->getSourceRange().isValid()) {
      std::string ReplacementText = "({";
      for (Stmt *S : CallsiteTagStmt->body()) {
        ReplacementText +=
            textFromSourceRange(S->getSourceRange(), *Result.Context);
      }
      ReplacementText += textFromSourceRange(CallsiteWrapper->getSourceRange(),
                                             *Result.Context);
      for (auto& C : ReplacementText) {
        if (C == '\n' || C == '\r') {
          C = ' ';
        }
      }
      addReplacement(Callee->getSourceRange(), ReplacementText, *Result.Context,
                     Replacements);
      std::string InsertionText = (CE->getNumArgs() == 0 ? "" : ", ");
      InsertionText += textFromSourceRange(Callee->getSourceRange(), *Result.Context);
      InsertionText += ", &";
      InsertionText += textFromSourceRange(CallsiteTag->getSourceRange(), *Result.Context);
      InsertionText += ");}";
      for (auto& C : InsertionText) {
        if (C == '\n' || C == '\r') {
          C = ' ';
        }
      }
      addInsertion(CE->getRParenLoc(), InsertionText, *Result.Context,
                   Replacements);
    }
  }

private:
  std::map<std::string, tooling::Replacements> &Replacements;
};
} // end anonymous namespace

// Set up the command line options
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::OptionCategory CallsiteWrapperCategory("callsite-wrapper options");

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  auto ExpectedParser =
      tooling::CommonOptionsParser::create(argc, argv, CallsiteWrapperCategory);

  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }

  tooling::CommonOptionsParser &OptionsParser = ExpectedParser.get();

  const auto& Files = OptionsParser.getSourcePathList();
  tooling::RefactoringTool Tool(OptionsParser.getCompilations(), Files);

  ast_matchers::MatchFinder Finder;
  CallsiteWrapperCallback CallsiteWrapper(Tool.getReplacements());

  Finder.addMatcher(callExpr(callee(functionDecl(hasCallsiteWrappedByAttr())))
                        .bind("callsite_wrapped_by"),
                    &CallsiteWrapper);

  auto Factory = newFrontendActionFactory(&Finder);
  int ExitCode = Tool.run(Factory.get());
  LangOptions DefaultLangOptions;
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts(new DiagnosticOptions());
  TextDiagnosticPrinter DiagnosticPrinter(errs(), &*DiagOpts);
  DiagnosticsEngine Diagnostics(
      IntrusiveRefCntPtr<DiagnosticIDs>(new DiagnosticIDs()), &*DiagOpts,
      &DiagnosticPrinter, false);

  auto &FileMgr = Tool.getFiles();
  SourceManager Sources(Diagnostics, FileMgr);
  Rewriter Rewrite(Sources, DefaultLangOptions);
  Tool.applyAllReplacements(Rewrite);

  for (const auto &File : Files) {
    auto Entry = llvm::cantFail(FileMgr.getFileRef(File));
    const auto ID = Sources.getOrCreateFileID(Entry, SrcMgr::C_User);
    Rewrite.getEditBuffer(ID).write(outs());
  }

  return ExitCode;
}
