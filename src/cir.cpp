//#include "config.h"
#include "cir.h"

#if ENABLE_CIR

#include "ast.h"

std::unique_ptr<CIR::Value> VariableExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> DeferExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> ReturnAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> ScopeExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> AsmExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}


std::unique_ptr<CIR::Value> AddrExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> VarExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> EmptyExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> StringExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> DerefExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> IfExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> TypeidExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> CharExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> SemicolonExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> SizeofExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> BoolExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> LoopExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> ForExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> StructMemberCallExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> MatchExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> ConstantVectorExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> EnumCreation::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> CallExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> GotoExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> WhileExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> BreakExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> ConstantStructExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> BinaryExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> ConstantArrayExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> LabelExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> NumberExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> UnaryExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> NullExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> ClosureAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> CastExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

std::unique_ptr<CIR::Value> CommentExprAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){
    return std::make_unique<CIR::Value>();
}

void ModAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){

}

void MembersDeclarAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){

}

void EnumDeclarAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){

}

void UnionDeclarAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){

}

void GlobalVariableAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){

}

void FunctionAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){

}

void StructDeclarAST::cir_gen(std::unique_ptr<FileCIR>& fileCIR){

}

std::unique_ptr<FileCIR> FileAST::cir_gen(){
    auto fileCIR = std::make_unique<FileCIR>(std::vector<std::unique_ptr<CIR::BasicBlock>>());
    for (int i = 0; i < global_vars.size(); i++){
    global_vars.at(i)->cir_gen(fileCIR);
  }
  /*for (int i = 0; i < function_protos.size(); i++){
    function_protos.at(i)->cir_gen();
  }*/
  for (int i = 0; i < structs.size(); i++){
    structs.at(i)->cir_gen(fileCIR);
  }
  for (int i = 0; i < enums.size(); i++){
    enums.at(i)->cir_gen(fileCIR);
  }
  for (int i = 0; i < unions.size(); i++){
    unions.at(i)->cir_gen(fileCIR);
  }
  for (int i = 0; i < members.size(); i++){
    members.at(i)->cir_gen(fileCIR);
  }
  for (int i = 0; i < functions.size(); i++){
    functions.at(i)->cir_gen(fileCIR);
  }
  for (int i = 0; i < mods.size(); i++){
    mods.at(i)->cir_gen(fileCIR);
  }
  return fileCIR;
}

#endif