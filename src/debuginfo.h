#include "llvm/IR/DIBuilder.h"
#include "ast.h"
#include "errors.h"

using namespace llvm;

#pragma once
struct DebugInfo {
  DICompileUnit *TheCU;
  DIType *DblTy;
  std::vector<DIScope *> LexicalBlocks;
  DIType *getDoubleTy();
  void emitLocation(Compiler_context context, bool pop_the_scope);
  void emitLocation(ExprAST* AST);
};


DISubroutineType *DebugInfoCreateFunctionType(Cpoint_Type type, std::vector<std::pair<std::string, Cpoint_Type>> Args);

void debugInfoCreateParameterVariable(DISubprogram *SP, DIFile *Unit, AllocaInst *Alloca, Argument& Arg, unsigned& ArgIdx, unsigned LineNo);