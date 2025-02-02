#include "config.h"

#if DISABLE_OLD_LLVM_BACKEND == 0

#include "codegen.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include <iostream>
#include <unordered_map>
#include <stack>
#include <cstdarg>
#include <fstream>
//#include "ast.h"
//#include "lexer.h"
//#include "types.h"
//#include "log.h"
#include "debuginfo.h"
#include "operators.h"
#include "checking.h"
#include "templates.h"
#include "abi.h"
#include "members.h"
//#include "types.h"
//#include "mangling.h"
#include "reflection.h"
//#include "vars.h"
#include "macros.h"
#include "match.h"
#include "targets/targets.h"
#include "utils.h"

#include "tracy.h"

#include <typeinfo>
#include <cxxabi.h>

using namespace llvm;

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;
std::unordered_map<std::string, std::unique_ptr<NamedValue>> NamedValues;
std::unordered_map<std::string, std::unique_ptr<GlobalVariableValue>> GlobalVariables;
std::unordered_map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
std::unordered_map<std::string, std::unique_ptr<StructDeclaration>> StructDeclarations;
std::unordered_map<std::string, std::unique_ptr<UnionDeclaration>> UnionDeclarations;
std::unordered_map<std::string, std::unique_ptr<EnumDeclaration>> EnumDeclarations;

std::unordered_map<std::string, std::unique_ptr<TemplateProto>> TemplateProtos;

std::unordered_map<std::string, std::unique_ptr<StructDeclar>> TemplateStructDeclars;
std::unordered_map<std::string, std::unique_ptr<EnumDeclar>> TemplateEnumDeclars;

//std::vector<std::string> modulesNamesContext;

std::unordered_map<std::string, Function*> GeneratedFunctions;

std::stack<BasicBlock*> blocksForBreak; // TODO : put this in Compiler_Context

std::vector<std::unique_ptr<TemplateCall>> TemplatesToGenerate;


// TODO : differenciate between the ones with enums, structs and function calls 
std::pair<std::string, Cpoint_Type> TypeTemplateCallCodegen; // contains the type of template in function call
std::string TypeTemplateCallAst = ""; // TODO : replace this by a vector to have multiple templates in the future ?

std::vector<std::unique_ptr<ExternToGenerate>> externFunctionsToGenerate;

extern std::vector<std::unique_ptr<TestAST>> testASTNodes;

std::unordered_map<std::string, Value*> StringsGenerated;

std::deque<Scope> Scopes;

bool is_in_extern = false;

extern std::unordered_map<std::string, int> BinopPrecedence;
extern std::unique_ptr<DIBuilder> DBuilder;
extern struct DebugInfo CpointDebugInfo;

extern std::string filename;
extern std::string first_filename;

extern Source_location emptyLoc;

extern bool debug_info_mode;

extern bool is_in_struct_templates_codegen;
extern bool is_in_enum_templates_codegen;

extern TargetInfo targetInfos;

extern bool is_in_extern;

Value *LogErrorV(Source_location astLoc, const char *Str, ...);

void add_manually_extern(std::string fnName, Cpoint_Type cpoint_type, std::vector<std::pair<std::string, Cpoint_Type>> ArgNames, unsigned Kind, unsigned BinaryPrecedence, bool is_variable_number_args, bool has_template, std::string TemplateName);

Function *getFunction(std::string Name) {
  // First, see if the function has already been added to the current module.
  if (!Comp_context->jit_mode){
  if (auto *F = TheModule->getFunction(Name)){
    Log::Info() << "TheModule->getFunction " << Name << "\n";
    return F;
  }
  }

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end() && FI->second){
    Log::Info() << "FI->second->codegen() " << Name << "\n";
    return FI->second->codegen();
  }
  Log::Info() << "nullptr " << Name << "\n";
  // If no existing prototype exists, return null.
  return nullptr;
}

static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName, Type* type){
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                 TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(type, 0,
                           VarName);
}

AllocaInst *CreateEntryBlockAlloca(Function *TheFunction, StringRef VarName, Cpoint_Type type){
    return CreateEntryBlockAlloca(TheFunction, VarName, get_type_llvm(type));
}

static BasicBlock* get_basic_block(Function* TheFunction, std::string name){
  for (Function::iterator b = TheFunction->begin(), be = TheFunction->end(); b != be; ++b){
    BasicBlock* BB = &(*b);
    Log::Info() << "BasicBlock : " << (std::string)BB->getName() << "\n";
    if (BB->getName() == name){
      return BB;
    }
  }
  return nullptr;
}

// TODO : change name of these function to be with underscodes ?
Value* callLLVMIntrisic(std::string Callee, std::vector<Value*> ArgsV, std::vector<Type*> Tys){
  Callee = Callee.substr(5, Callee.size());
  Log::Info() << "llvm intrisic called " << Callee << "\n";
  llvm::Intrinsic::IndependentIntrinsics intrisicId = llvm::Intrinsic::not_intrinsic;
  if (Callee == "va_start"){
    intrisicId = Intrinsic::vastart;
  } else if (Callee == "va_end"){
    intrisicId = Intrinsic::vaend;
  } else if (Callee == "abs"){
    intrisicId = Intrinsic::abs;
  } else if (Callee == "cos"){
    intrisicId = Intrinsic::cos;
  } else if (Callee == "addressofreturnaddress"){
    intrisicId = Intrinsic::addressofreturnaddress;
  } else if (Callee == "bswap"){
    intrisicId = Intrinsic::bswap;
  } else if (Callee == "exp"){
    intrisicId = Intrinsic::exp;
  } else if (Callee == "fabs"){
    intrisicId = Intrinsic::fabs;
  } else if (Callee == "floor"){
    intrisicId = Intrinsic::floor;
  } else if (Callee == "log"){
    intrisicId = Intrinsic::log;
  } else if (Callee == "memcpy"){
    intrisicId = Intrinsic::memcpy;
  } else if (Callee == "memmove"){
    intrisicId = Intrinsic::memmove;
  } else if (Callee == "memset"){
    intrisicId = Intrinsic::memset;
  } else if (Callee == "trap"){
    intrisicId = Intrinsic::trap;
  } else if (Callee == "trunc"){
    intrisicId = Intrinsic::trunc;
  } else if (Callee == "read_register"){
    intrisicId = Intrinsic::read_register;
  } else if (Callee == "vp.add"){
    intrisicId = Intrinsic::vp_add;
  } else if (Callee == "bitreverse"){
    intrisicId = Intrinsic::bitreverse;
    /*Cpoint_Type arg_cpoint_type = Args.at(0)->get_type();
    Log::Info() << "type : " << arg_cpoint_type << "\n";
    if (arg_cpoint_type.is_decimal_number_type()){
        return LogErrorV(emptyLoc, "Can't use the bitreverse intrisic on a floating point type arg"); 
    }
    Tys = ArrayRef<Type*>(get_type_llvm(arg_cpoint_type));*/
  } else if (Callee == "ctpop"){
    intrisicId = Intrinsic::ctpop;
  }
  Function *CalleeF = Intrinsic::getDeclaration(TheModule.get(), intrisicId, Tys);
  // add additional args for intrisics
  if (Callee == "memcpy" || Callee == "memset" || Callee == "memmove") {
    ArgsV.push_back(BoolExprAST(false).codegen()); // the bool is if it is volatile
  }
  if (Callee == "abs"){
    ArgsV.push_back(BoolExprAST(false).codegen()); // the bool is if it is a poison value
  }
  std::string call_instruction_name = "calltmp";
  if (CalleeF->getReturnType()->isVoidTy()){
    call_instruction_name = "";
  }
  for (int i = 0; i < ArgsV.size(); i++){
    if (ArgsV.at(i)->getType() != CalleeF->getArg(i)->getType()){
        convert_to_type(get_cpoint_type_from_llvm(ArgsV.at(i)->getType()), get_cpoint_type_from_llvm(CalleeF->getArg(i)->getType()), ArgsV.at(i)); // the types passed should probably trivial to find the Cpoint_Type from the llvm one (number types or vector types) so it is okay to use get_cpoint_type_from_llvm
    }
  }
  return Builder->CreateCall(CalleeF, ArgsV, call_instruction_name);
}

int get_arg_pos_intrisic_infer_overload_type(std::string CalleeWithoutPrefix){
    if (CalleeWithoutPrefix == "memcpy" || CalleeWithoutPrefix == "memmove"){
        return 2;
    }
    return 0;
}

Value* callLLVMIntrisic(std::string Callee, std::vector<std::unique_ptr<ExprAST>>& Args, Cpoint_Type template_passed_type){
  std::vector<Type*> Tys;
  std::string CalleeWithoutPrefix = Callee.substr(5, Callee.size());
  Cpoint_Type overloading_type;
  if (CalleeWithoutPrefix == "bitreverse" || CalleeWithoutPrefix == "ctpop" || CalleeWithoutPrefix == "vp.add" || CalleeWithoutPrefix == "memcpy"){
    Cpoint_Type arg_cpoint_type = Args.at(get_arg_pos_intrisic_infer_overload_type(CalleeWithoutPrefix))->get_type();
    overloading_type = arg_cpoint_type;
    if (!template_passed_type.is_empty){
        overloading_type = template_passed_type;
    }
    Log::Info() << "type : " << arg_cpoint_type << "\n";
    if (CalleeWithoutPrefix == "bitreverse" || CalleeWithoutPrefix == "ctpop"){
        if (overloading_type.is_decimal_number_type()){
            return LogErrorV(emptyLoc, "Can't use the %s intrisic on a floating point type arg", Callee.c_str()); 
        }
    } else if (CalleeWithoutPrefix == "vp.add"){
        if (!overloading_type.is_decimal_number_type() && !overloading_type.is_signed() && !overloading_type.is_unsigned()){
            return LogErrorV(emptyLoc, "Can't use the %s intrisic on an arg that is not a number", Callee.c_str()); 
        }
    }
    Tys.push_back(get_type_llvm(overloading_type));
  }
  std::vector<Value *> ArgsV;
  for (int i = 0; i < Args.size(); i++) {
    Value* temp_val = Args[i]->codegen();
    if (!temp_val){
        return nullptr;
    }

    ArgsV.push_back(temp_val);
  }
  return callLLVMIntrisic(Callee, ArgsV, Tys);
}

static Value* getSizeOfStruct(Value *A){
    Type* llvm_type = A->getType();
    auto one = llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 1, true));
    Value* size = Builder->CreateGEP(llvm_type, Builder->CreateIntToPtr(ConstantInt::get(Builder->getInt64Ty(), 0),llvm_type->getPointerTo()), {one});
    size =  Builder->CreatePtrToInt(size, get_type_llvm(Cpoint_Type(i32_type)));
    return size;
    /*std::vector<Value*> ArgsV;
    ArgsV.push_back(A);
    ArgsV.push_back(ConstantInt::get(*TheContext, llvm::APInt(1, 0, true))); // return -1 if object size is unknown
    ArgsV.push_back(ConstantInt::get(*TheContext, llvm::APInt(1, 1, true))); // return unknown size if null is passed
    ArgsV.push_back(ConstantInt::get(*TheContext, llvm::APInt(1, 1, true))); // is the calculation made at runtime
    std::vector<Type*> OverloadedTypes;
//    OverloadedTypes.push_back(get_type_llvm(i32_type));
    OverloadedTypes.push_back(get_type_llvm(i64_type));
    Function *CalleeF = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::objectsize, OverloadedTypes);
    return Builder->CreateCall(CalleeF, ArgsV, "sizeof_calltmp");*/
}

Value *NumberExprAST::codegen() {
  if (trunc(Val) == Val){
    return ConstantInt::get(*TheContext, APInt(32, (int)Val, true));
  } else {
    return ConstantFP::get(get_type_llvm(Cpoint_Type(double_type)), APFloat(Val));
  }
}

Value* StringExprAST::codegen() {
  Value* string;
  if (StringsGenerated[str]){
    string = StringsGenerated[str];
  } else {
    string = Builder->CreateGlobalStringPtr(StringRef(str.c_str()));
    StringsGenerated[str] = string;
  }
  return string;
}

std::string nearest_var(std::string var){
    int minDistance = -1;
    std::string minDistanceName = "";
    for (auto& global : GlobalVariables){
        if (minDistance == -1){
            minDistance = stringDistance(global.first, var);
        } else {
            int tempGlobalDistance = stringDistance(global.first, var);
            if (tempGlobalDistance < minDistance){
                minDistance = tempGlobalDistance;
                minDistanceName = global.first;
            }
        }
    }

    for (auto& local : NamedValues){
        if (minDistance == -1){
            minDistance = stringDistance(local.first, var);
        } else {
            int tempGlobalDistance = stringDistance(local.first, var);
            if (tempGlobalDistance < minDistance){
                minDistance = tempGlobalDistance;
                minDistanceName = local.first;
            }
        }
    }

    if (minDistance == -1){
        return "";
    }

    return minDistanceName;
}

Value *VariableExprAST::codegen() {
  // Look this variable up in the function.
  if (FunctionProtos[Name] != nullptr){
    return GeneratedFunctions[Name];
  }
  if (!var_exists(Name)) {
    std::string nearest_var_name = nearest_var(Name); // TODO : implement this in CIR 
    return LogErrorV(this->loc, "Unknown variable name %s, maybe you meant %s", Name.c_str(), nearest_var_name.c_str()); // TODO : print the "maybe you meant" part only when the string is not empty ?
  }
  Value* ptr = get_var_allocation(Name);
  Cpoint_Type var_type = *get_variable_type(Name); 
  Log::Info() << "loading var " << Name << " of type " << type << "\n";
  return Builder->CreateLoad(get_type_llvm(var_type), ptr, (Name + ".load").c_str());
}

Value* ConstantArrayExprAST::codegen(){
  std::vector<Constant*> arrayMembersVal;
  for (int i = 0; i < ArrayMembers.size(); i++){
    Value* Vtemp = ArrayMembers.at(i)->codegen();
    Constant* constant;
    if (! (constant = dyn_cast<Constant>(Vtemp))){
      return LogErrorV(this->loc, "The %d value of constant array is not a constant", i);
    }
    arrayMembersVal.push_back(constant);
  }
  llvm::ArrayType* arrayType = ArrayType::get(arrayMembersVal.at(0)->getType(), arrayMembersVal.size());
  // verify if everyone of them are constants of the same type
  return llvm::ConstantArray::get(arrayType, arrayMembersVal);
}

Value* ConstantStructExprAST::codegen(){
    std::vector<Constant*> structMembersVal;
    for (int i = 0; i < StructMembers.size(); i++){
        Cpoint_Type structMemberType = StructMembers.at(i)->get_type();
        Value* Vtemp = StructMembers.at(i)->codegen();
        if (get_type_llvm(StructDeclarations[struct_name]->members.at(i).second) != Vtemp->getType()){
            convert_to_type(structMemberType, get_type_llvm(StructDeclarations[struct_name]->members.at(i).second), Vtemp);
        }
        Constant* constant;
        if (! (constant = dyn_cast<Constant>(Vtemp))){
            return LogErrorV(this->loc, "The %d value of constant array is not a constant", i);
        }
        // convert type
        /*if (get_type_llvm(StructDeclarations[struct_name]->members.at(i).second) != constant->getType()){
            convert_to_type(get_cpoint_type_from_llvm(constant->getType()), get_type_llvm(StructDeclarations[struct_name]->members.at(i).second), constant);
        }*/
        structMembersVal.push_back(constant);
    }
    auto structType = StructDeclarations[struct_name]->struct_type;
    return ConstantStruct::get(dyn_cast<StructType>(structType), structMembersVal);
}

Value* ConstantVectorExprAST::codegen(){
    std::vector<Constant*> VectorMembersVal;
    for (int i = 0; i < VectorMembers.size(); i++){
        Cpoint_Type structMemberType = VectorMembers.at(i)->get_type();
        Value* Vtemp = VectorMembers.at(i)->codegen();
        if (get_type_llvm(vector_element_type) != Vtemp->getType()){
            convert_to_type(structMemberType, get_type_llvm(vector_element_type), Vtemp);
        }
        Constant* constant;
        if (! (constant = dyn_cast<Constant>(Vtemp))){
            return LogErrorV(this->loc, "The %d value of constant array is not a constant", i);
        }
        VectorMembersVal.push_back(constant);
    }
    return ConstantVector::get(VectorMembersVal);
}

static Value* codegenBody(std::vector<std::unique_ptr<ExprAST>>& Body){
    //bool should_break = false;
    Value* ret = nullptr;
    for (int i = 0; i < Body.size(); i++){
        /*if (Body.at(i)->contains_expr(ExprType::Return) || Body.at(i)->contains_expr(ExprType::Unreachable)){
          should_break = true;
        }*/
        ret = Body.at(i)->codegen();
        if (!ret){
            return nullptr;
        }
        /*if (should_break){
          if (i < Body.size()-1){
            auto unreachable_code_warning = Log::Warning();
            unreachable_code_warning.head << "Unreachable code found\n";
            unreachable_code_warning.head.end();
            unreachable_code_warning.content << "test content";
            unreachable_code_warning.content.end();
            
          }
          //break;
        }*/
    }
    return ret;
}

static void createScope(){
    DIScope* debuginfos_scope = nullptr;
    if (debug_info_mode){
        Function* TheFunction = Builder->GetInsertBlock()->getParent();
        int lineNo = 0; // TODO
        debuginfos_scope = DBuilder->createLexicalBlock(TheFunction->getSubprogram(), CpointDebugInfo.TheCU->getFile(), lineNo, 0);
    }
    Scope new_scope = Scope(std::deque<std::unique_ptr<ExprAST>>(), debuginfos_scope);
    Scopes.push_back(std::move(new_scope));
}

static void endScope(){
    Scope back = std::move(Scopes.back());
    Log::Info() << "gen scope defers : " << back.to_string() << "\n";
    for (auto it = back.deferExprs.begin(); it != back.deferExprs.end(); ++it) {
        (*it)->codegen();
    }
    Scopes.pop_back();
}

Value* ScopeExprAST::codegen(){
    // TODO : add debuginfos for scopes ?
    createScope();
    Log::Info() << "Scope size : " << Body.size() << "\n";
    Value* ret = codegenBody(Body);
    endScope();
    return ret;
}

Type* StructDeclarAST::codegen(){
  Log::Info() << "CODEGEN STRUCT DECLAR" << "\n";
  StructType* structType = StructType::create(*TheContext);
  structType->setName(Name);
  std::vector<Type*> dataTypes;
  std::vector<std::pair<std::string,Cpoint_Type>> members_for_template;
  std::vector<std::string> functions_for_template;
  for (int i = 0; i < Vars.size(); i++){
    std::unique_ptr<VarExprAST> VarExpr = get_Expr_from_ExprAST<VarExprAST>(/*Vars.at(i)->clone()*/ std::move(Vars.at(i)));
    if (!VarExpr){
        Log::Info() << "VarExpr is nullptr" << "\n";
    }
    Log::Info() << "Var StructDeclar type codegen : " << VarExpr->cpoint_type << "\n";
    /*if (VarExpr->cpoint_type.struct_name == Name && VarExpr->cpoint_type.is_ptr){ // TODO : fix this ?
        VarExpr->cpoint_type.is_struct = false;
        VarExpr->cpoint_type.struct_name = "";
    }*/
    Type* var_type = get_type_llvm(VarExpr->cpoint_type);
    dataTypes.push_back(var_type);
    std::string VarName = VarExpr->VarNames.at(0).first;
    members_for_template.push_back(std::make_pair(VarName, VarExpr->cpoint_type)); // already done in ast.cpp
  }
  structType->setBody(dataTypes);
  Log::Info() << "adding struct declaration name " << Name << " to StructDeclarations" << "\n";
  // TODO for debuginfos
  DIType* structDebugInfosType = nullptr;
  if (!is_in_struct_templates_codegen){
    StructDeclarations[Name] = std::make_unique<StructDeclaration>(structType, structDebugInfosType, StructDeclarations[Name]->members, /*functions*/  StructDeclarations[Name]->functions);
  } else {
        std::string structName = Name.substr(0, Name.find("____"));
        StructDeclarations[Name] = StructDeclarations[structName]->clone();
        StructDeclarations[Name]->struct_type = structType;
  }
 if (is_in_struct_templates_codegen){
    StructDeclarations[Name] = std::make_unique<StructDeclaration>(structType, structDebugInfosType, std::move(members_for_template), StructDeclarations[Name]->functions);
  }
  if (debug_info_mode){
    Cpoint_Type struct_type = Cpoint_Type(other_type, false, 0, false, 0, true, Name);
    std::vector<std::pair<std::string, Cpoint_Type>> Members = members_for_template;
    structDebugInfosType = DebugInfoCreateStructType(struct_type, Members, 0); // TODO : get LineNo
    StructDeclarations[Name]->struct_debuginfos_type = structDebugInfosType;
  }
  for (int i = 0; i < Functions.size(); i++){
    std::unique_ptr<FunctionAST> FunctionExpr = Functions.at(i)->clone();
    std::string function_name;
    std::string structNameWithoutTemplate = Name;
    if (is_object_template(Name)){
        structNameWithoutTemplate = remove_struct_template(Name);
    }
    if (FunctionExpr->Proto->Name == structNameWithoutTemplate){
    // Constructor
    function_name = "Constructor__Default";
    } else {
    function_name = FunctionExpr->Proto->Name;
    }

    std::string mangled_name_function = struct_function_mangling(Name, function_name);
    Cpoint_Type self_pointer_type = Cpoint_Type(other_type, true, 0, false, 0, true, Name);
    FunctionExpr->Proto->Args.insert(FunctionExpr->Proto->Args.begin(), std::make_pair("self", self_pointer_type));
    FunctionExpr->Proto->Name = mangled_name_function;
    FunctionExpr->codegen();
    if (is_in_struct_templates_codegen){
        functions_for_template.push_back(function_name);
    }
  }
  for (int i = 0; i < ExternFunctions.size(); i++){
    std::unique_ptr<PrototypeAST> ProtoExpr = std::move(ExternFunctions.at(i));
    std::string function_name;
    if (ProtoExpr->Name == Name){
    // Constructor
    function_name = "Constructor__Default";
    } else {
    function_name = ProtoExpr->Name;
    }
    std::string mangled_name_function = struct_function_mangling(Name, function_name);
    Cpoint_Type self_pointer_type = Cpoint_Type(other_type, true, 1, false, 0, true, Name);
    ProtoExpr->Args.insert(ProtoExpr->Args.begin(), std::make_pair("self", self_pointer_type));
    ProtoExpr->Name = mangled_name_function;
    ProtoExpr->codegen();
    if (is_in_struct_templates_codegen){
        functions_for_template.push_back(function_name);
    }
  }
  // TODO : for now, enable it after just for templates, see later if there is another solution
  if (is_in_struct_templates_codegen){
    StructDeclarations[Name] = std::make_unique<StructDeclaration>(structType, structDebugInfosType, /*std::move(members_for_template)*/ StructDeclarations[Name]->members, std::move(functions_for_template));
  }
  Log::Info() << "appending to StructDeclarations struct " << Name << "\n";
  return structType;
}

Type* UnionDeclarAST::codegen(){
  StructType* unionType = StructType::create(*TheContext);
  unionType->setName(Name);
  // take the biggest type and just have it in the struct and bitcast for the other types
  std::vector<std::pair<std::string,Cpoint_Type>> members;
  std::vector<Type*> dataType;
  Cpoint_Type biggest_type = Cpoint_Type(double_type);
  int biggest_type_size = 0;
  for (int i = 0; i < Vars.size(); i++){
    if (Vars.at(i)->cpoint_type.get_number_of_bits() > biggest_type_size){
      biggest_type = Vars.at(i)->cpoint_type;
      biggest_type_size = Vars.at(i)->cpoint_type.get_number_of_bits();
    }
  }
  for (int i = 0; i < Vars.size(); i++){
    std::unique_ptr<VarExprAST> VarExpr = std::move(Vars.at(i));
    std::string VarName = VarExpr->VarNames.at(0).first;
    members.push_back(std::make_pair(VarName, VarExpr->cpoint_type));
  }
  dataType.push_back(get_type_llvm(biggest_type));
  unionType->setBody(dataType);
  UnionDeclarations[Name] = std::make_unique<UnionDeclaration>(unionType, members);
  return unionType;
}

Type* EnumDeclarAST::codegen(){
    Type* enumType = nullptr;
    if (!enum_member_contain_type){
        enumType = get_type_llvm(Cpoint_Type(i32_type));
        EnumDeclarations[Name] = std::make_unique<EnumDeclaration>(enumType, this->clone());
        return nullptr;
    }
    Log::Info() << "codegen EnumDeclarAST with type" << "\n";
    std::vector<Type*> dataType;
    StructType* enumStructType = StructType::create(*TheContext);
    enumStructType->setName(Name);
    dataType.push_back(get_type_llvm(Cpoint_Type(i32_type)));
    Log::Info() << "EnumDeclarAST TEST" << "\n";
    int biggest_type_size = 0;
    Cpoint_Type biggest_type = Cpoint_Type(double_type);
    for (int i = 0; i < EnumMembers.size(); i++){
        if (EnumMembers.at(i).Type){
        if (EnumMembers.at(i).Type->get_number_of_bits() > biggest_type_size){
            biggest_type = *(EnumMembers.at(i).Type);
            biggest_type_size = EnumMembers.at(i).Type->get_number_of_bits();
        }
        }
    }
    dataType.push_back(get_type_llvm(biggest_type));
    enumStructType->setBody(dataType);
    EnumDeclarations[Name] = std::make_unique<EnumDeclaration>(enumStructType, this->clone());
    return enumStructType;
}

Cpoint_Type MembersDeclarAST::get_self_type(){
    if (!is_type(members_for)){
        return Cpoint_Type(other_type, true, 0, false, 0, true, members_for);;
    } else {
        return Cpoint_Type(get_type(members_for));
    }
}

void MembersDeclarAST::codegen(){
    bool is_builtin_type = is_type(members_for);
    if (StructDeclarations[members_for] == nullptr && !is_builtin_type){
        LogErrorE("Members for a struct that doesn't exist");
        return;
    }
    Cpoint_Type self_type = get_self_type();
    Log::Info() << "self type : " << self_type << "\n";
    for (int i = 0; i < Functions.size(); i++){
        std::unique_ptr<FunctionAST> FunctionExpr = std::move(Functions.at(i));
        std::string function_name = FunctionExpr->Proto->getName();
        std::string mangled_name_function = "";
        if (!is_builtin_type){
            StructDeclarations[members_for]->functions.push_back(function_name);
            mangled_name_function = struct_function_mangling(members_for, function_name);
        } else {
            mangled_name_function = self_type.create_mangled_name() + "__" + function_name;;
        }
        FunctionExpr->Proto->Args.insert(FunctionExpr->Proto->Args.begin(), std::make_pair("self", self_type));
        FunctionExpr->Proto->Name = mangled_name_function;
        FunctionExpr->codegen();
    }
    is_in_extern = true;
    for (int i = 0; i < Externs.size(); i++){
        std::unique_ptr<PrototypeAST> Proto = std::move(Externs.at(i));
        std::string function_name = Proto->getName();
        std::string mangled_name_function = "";
        if (!is_builtin_type){
            StructDeclarations[members_for]->functions.push_back(function_name);
            mangled_name_function = struct_function_mangling(members_for, function_name);
        } else {
            mangled_name_function = self_type.create_mangled_name() + "__" + function_name;;
        }
        Proto->Args.insert(Proto->Args.begin(), std::make_pair("self", self_type));
        Proto->Name = mangled_name_function;
        Proto->codegen();
    }
    is_in_extern = false;
}


// TODO : add the creation of a struct here (struct {tag, value} in pseudo code). Maybe even replace it for the current implementation in RedeclarationExprAST
Value* EnumCreation::codegen(){
    if (!EnumDeclarations[EnumVarName]){
        return LogErrorV(this->loc, "Enum %s doesn't exist", EnumVarName.c_str());
    }
    if (EnumDeclarations[EnumVarName]->EnumDeclar->enum_member_contain_type){
        return LogErrorV(this->loc, "Enum containing types are not supported to be created not in default values of var for now");
    }
    int index = -1;
    bool is_custom_value = false;
    // TODO : move the finding of the index in a different function
    for (int i = 0; i < EnumDeclarations[EnumVarName]->EnumDeclar->EnumMembers.size(); i++){
        if (EnumDeclarations[EnumVarName]->EnumDeclar->EnumMembers.at(i).Name == EnumMemberName){
            if (EnumDeclarations[EnumVarName]->EnumDeclar->EnumMembers.at(i).contains_custom_index){
                is_custom_value = true;
                index = EnumDeclarations[EnumVarName]->EnumDeclar->EnumMembers.at(i).Index;
            } else {
                index = i;
            }
        }
    }
    if (!is_custom_value && index == -1){
        return LogErrorV(this->loc, "Couldn't find member %s in enum %s", EnumMemberName.c_str(), EnumVarName.c_str());
    }
    return ConstantInt::get(*TheContext, llvm::APInt(64, index, true));
}

Value* DeferExprAST::codegen(){
    // modify the last scope so pop it then repush it
    Scope back = std::move(Scopes.back());
    Scopes.pop_back();
    back.deferExprs.push_back(std::move(Expr));
    Scopes.push_back(std::move(back));
    return Constant::getNullValue(get_type_llvm(Cpoint_Type(void_type)));
}

void assignUnionMember(Value* union_ptr, Value* val, Cpoint_Type member_type){
    if (val->getType() != get_type_llvm(member_type)){
        convert_to_type(get_cpoint_type_from_llvm(val->getType()), member_type, val);
    }
    Builder->CreateStore(val, union_ptr);
}

Value* getArrayOrVectorMemberGEP(std::unique_ptr<ExprAST> array, std::unique_ptr<ExprAST> index, Cpoint_Type& member_type, bool& should_load);

Value* equalOperator(std::unique_ptr<ExprAST> lvalue, std::unique_ptr<ExprAST> rvalue){
    Log::Info() << "EQUAL OP " << lvalue->to_string() << " = " << rvalue->to_string() << "\n";
    Value* ValDeclared = rvalue->codegen();
    if (dynamic_cast<BinaryExprAST*>(lvalue.get())){
        Log::Info() << "Equal op bin lvalue" << "\n";
        std::unique_ptr<BinaryExprAST> BinExpr = get_Expr_from_ExprAST<BinaryExprAST>(std::move(lvalue));
        Log::Info() << "op : " << BinExpr->Op << "\n";
        if (BinExpr->Op.at(0) == '['){
            // TODO : make work using other things than variables
            Cpoint_Type member_type;
            bool should_load;
            Value* ptr = getArrayOrVectorMemberGEP(std::move(BinExpr->LHS), std::move(BinExpr->RHS), member_type, should_load);
            Log::Info() << "Create GEP" << "\n";
            if (ValDeclared->getType() != get_type_llvm(member_type)){
            convert_to_type(get_cpoint_type_from_llvm(ValDeclared->getType()), member_type, ValDeclared);
            }
            Builder->CreateStore(ValDeclared, ptr);
            return Constant::getNullValue(Type::getDoubleTy(*TheContext));
        } else if (BinExpr->Op.at(0) == '.'){
            // TODO : just use getStructMemberGEP
            Cpoint_Type member_type;
            Value* ptr = getStructMemberGEP(std::move(BinExpr->LHS), std::move(BinExpr->RHS), member_type);
                
            if (ValDeclared->getType() != get_type_llvm(member_type)){
                convert_to_type(get_cpoint_type_from_llvm(ValDeclared->getType()), member_type, ValDeclared);
            }
            Builder->CreateStore(ValDeclared, ptr);
            return Constant::getNullValue(Type::getDoubleTy(*TheContext));
        } else {
            return LogErrorV(emptyLoc, "Using as a rvalue a bin expr that is not an array member or a array member operator");
        }
    } else if (dynamic_cast<VariableExprAST*>(lvalue.get())){
        Log::Info() << "equal operator lvalue variable" << "\n";
        std::unique_ptr<VariableExprAST> VarExpr = get_Expr_from_ExprAST<VariableExprAST>(std::move(lvalue));
        bool is_global = false;
        if (GlobalVariables[VarExpr->Name] != nullptr){
            is_global = true;
        }
        Cpoint_Type cpoint_type = *get_variable_type(VarExpr->Name);
        if (ValDeclared->getType() != get_type_llvm(cpoint_type)){
            convert_to_type(get_cpoint_type_from_llvm(ValDeclared->getType()), cpoint_type, ValDeclared);
        }
        if (is_global){
            Builder->CreateStore(ValDeclared, GlobalVariables[VarExpr->Name]->globalVar);
        } else {
        AllocaInst *Alloca = NamedValues[VarExpr->Name]->alloca_inst;
        if (ValDeclared->getType() == get_type_llvm(Cpoint_Type(void_type))){
            return LogErrorV(emptyLoc, "Assigning to a variable a void value");
        }
        Builder->CreateStore(ValDeclared, Alloca);
        }
        // TODO : maybe replace all the null values returned to the lvalue
        return Constant::getNullValue(Type::getDoubleTy(*TheContext));
    } else if (dynamic_cast<UnaryExprAST*>(lvalue.get()) || dynamic_cast<DerefExprAST*>(lvalue.get())){
        Value* lval_llvm = nullptr;
        if (dynamic_cast<UnaryExprAST*>(lvalue.get())){
            std::unique_ptr<UnaryExprAST> UnaryExpr = get_Expr_from_ExprAST<UnaryExprAST>(std::move(lvalue));
            if (UnaryExpr->Opcode != '*'){
                return LogErrorV(emptyLoc, "The equal operator is not implemented for other Unary Operators as rvalue than deref");
            }
            lval_llvm = AddrExprAST(std::move(UnaryExpr->Operand)).codegen();
        } else if (dynamic_cast<DerefExprAST*>(lvalue.get())) {
            lval_llvm = lvalue->codegen();
        }
        Value* ptr = Builder->CreateLoad(lval_llvm->getType(), lval_llvm);
        Builder->CreateStore(ValDeclared, ptr);
        return Constant::getNullValue(Type::getDoubleTy(*TheContext));
    }
    //Log::Info() << "Expr type : " << typeid(*lvalue.get()).name() << "\n";
    return LogErrorV(emptyLoc, "Trying to use the equal operator with an expression which it is not implemented for");
}

std::pair<Cpoint_Type, int>* get_member_type_and_pos_object(Cpoint_Type objectType, std::string MemberName){
    Cpoint_Type member_type;
    int pos = -1;
    if (objectType.is_union){
        std::vector<std::pair<std::string, Cpoint_Type>> members = UnionDeclarations[objectType.union_name]->members;
        for (int i = 0; i < members.size(); i++){
        Log::Info() << "members.at(i).first : " << members.at(i).first << " MemberName : " << MemberName << "\n";
        if (members.at(i).first == MemberName){
            pos = i;
            break;
        }
        }
        if (pos == -1){
            LogErrorE("Unknown member %s in union member", MemberName.c_str());
            return nullptr;    
        }
        member_type = members.at(pos).second;
    } else {
        std::vector<std::pair<std::string, Cpoint_Type>> members;
        if (objectType.object_template_type_passed){
            Log::Info() << "is template" << "\n";
            members = StructDeclarations[get_object_template_name(objectType.struct_name, *objectType.object_template_type_passed)]->members;
        } else {
            if (!StructDeclarations[objectType.struct_name]){
                LogErrorE("struct not found \"%s\"", objectType.struct_name.c_str());
                return nullptr;
            }
            members = StructDeclarations[objectType.struct_name]->members;
        }
        Log::Info() << "members.size() : " << members.size() << "\n";
        for (int i = 0; i < members.size(); i++){
        Log::Info() << "members.at(i).first : " << members.at(i).first << " MemberName : " << MemberName << "\n";
            if (members.at(i).first == MemberName){
                pos = i;
                break;
            }
        }
        Log::Info() << "Pos for GEP struct member " << pos << "\n";
        if (pos == -1){
            LogErrorE("Unknown member %s in struct member", MemberName.c_str());
            return nullptr;
        }
        member_type = members.at(pos).second;
    }
    return new std::pair<Cpoint_Type, int>(member_type, pos);
}

// only doing GEP, not loading
// member_type is for returning
// get struct, or union
// TODO : maybe rename it to getObjectMember (it returns only the ptr without load so maybe calling it getObjectMemberPtr)
// TODO : return a pair like get_member_type_and_pos_object
Value* getStructMemberGEP(std::unique_ptr<ExprAST> struct_expr, std::unique_ptr<ExprAST> member, Cpoint_Type& member_type){
    // TODO : refactor all of this so it is not just recopied code one after the other
    auto zero = llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 0, true));
    if (dynamic_cast<VariableExprAST*>(struct_expr.get())){
        Value* Alloca = nullptr;
        std::unique_ptr<VariableExprAST> structVarExpr = get_Expr_from_ExprAST<VariableExprAST>(std::move(struct_expr));
        std::string StructName = structVarExpr->Name;
        auto varExprMember = get_Expr_from_ExprAST<VariableExprAST>(std::move(member));
        if (!varExprMember){
            return LogErrorV(emptyLoc, "expected an identifier for a struct member");
        }
        std::string MemberName = varExprMember->Name;
        if (!var_exists(StructName)){
            return LogErrorV(emptyLoc, "Can't find struct %s that is used for a member", StructName.c_str());
        }
        Alloca = get_var_allocation(StructName);
        Cpoint_Type structType = *get_variable_type(StructName);
        Log::Info() << "struct type : " <<  structType << "\n";
        if (!structType.is_struct && !structType.is_union){ // TODO : verify if is is really  struct (it didn't work for example with the self of structs function members)
            return LogErrorV(emptyLoc, "Using a member of variable even though it is not a struct or an union");
        }
        Log::Info() << "StructName : " << StructName << "\n";
        //Log::Info() << "StructName len : " << StructName.length() << "\n";
        if (NamedValues[StructName] == nullptr){
            Log::Info() << "NamedValues[StructName] nullptr" << "\n";
        }
        //Log::Info() << "struct_declaration_name : " << NamedValues[StructName]->struct_declaration_name << "\n"; // USE FOR NOW STRUCT NAME FROM CPOINT_TYPE
        Log::Info() << "struct_declaration_name : " << structType.struct_name << "\n";
        if (structType.object_template_type_passed){
            Log::Info() << "object_template_type_passed : " << *structType.object_template_type_passed << "\n";
        }
        //Log::Info() << "struct_declaration_name length : " << structType.struct_name.length() << "\n";
        std::pair<Cpoint_Type, int>* temp_pair = get_member_type_and_pos_object(structType, MemberName);
        if (!temp_pair){
            return nullptr;
        }
        member_type = temp_pair->first;
        Log::Info() << "index : " << temp_pair->second << "\n";
        int pos = temp_pair->second;
        if (structType.is_union){
            return Alloca;
        } else {
        auto index = llvm::ConstantInt::get(*TheContext, llvm::APInt(32, pos, true));
        if (structType.is_ptr){
            structType.is_ptr = false;
            if (structType.nb_ptr > 0){
                structType.nb_ptr--;
            }
            Alloca = Builder->CreateLoad(get_type_llvm(Cpoint_Type(void_type, true)), Alloca);
        }
        Log::Info() << "cpoint_type struct : " << structType << "\n";
        auto structTypeLLVM = get_type_llvm(structType);
        Value* ptr = Builder->CreateGEP(structTypeLLVM, Alloca, { zero, index}, "", true);
        return ptr;
        }
    } else if (dynamic_cast<BinaryExprAST*>(struct_expr.get())){
        std::unique_ptr<BinaryExprAST> structBinExpr = get_Expr_from_ExprAST<BinaryExprAST>(std::move(struct_expr));
        // TODO : support array op here
        if (structBinExpr->Op != "."){
            return LogErrorV(emptyLoc, "Trying to use the struct member operator with an operator expression which it is not implemented for");
        }
        Cpoint_Type objectType = structBinExpr->get_type();
        //Log::Info() << "objectType : " << objectType << "\n";
        auto varExprMember = get_Expr_from_ExprAST<VariableExprAST>(std::move(member));
        if (!varExprMember){
            return LogErrorV(emptyLoc, "expected an identifier for a struct member");
        }
        std::string MemberName = varExprMember->Name;
        std::pair<Cpoint_Type, int>* temp_pair = get_member_type_and_pos_object(objectType, MemberName);
        if (!temp_pair){
            return nullptr;
        }
        member_type = temp_pair->first;
        Log::Info() << "member_type : " << member_type << "\n";
        int pos = temp_pair->second;
        Log::Info() << "index : " << pos << "\n";
        Cpoint_Type member_type_binop_struct;
        Value* lStruct = getStructMemberGEP(std::move(structBinExpr->LHS), std::move(structBinExpr->RHS), member_type_binop_struct);
        if (!lStruct){
            return nullptr;
        }
        auto index = llvm::ConstantInt::get(*TheContext, llvm::APInt(32, pos, true));
        if (member_type_binop_struct.is_ptr){
            member_type_binop_struct.is_ptr = false;
            member_type_binop_struct.nb_ptr--;
            lStruct = Builder->CreateLoad(get_type_llvm(Cpoint_Type(void_type, true)), lStruct);
        }
        Log::Info() << "member_type_binop_struct : " << member_type_binop_struct << "\n";
        Value* ptr = Builder->CreateGEP(get_type_llvm(member_type_binop_struct), lStruct, { zero, index}, "", true);
        return ptr;
    }
    return LogErrorV(emptyLoc, "Trying to use the struct member operator with an expression which it is not implemented for");
}


// TODO : add a function for struct member calls
Value* getStructMember(std::unique_ptr<ExprAST> struct_expr, std::unique_ptr<ExprAST> member){
    Cpoint_Type member_type;
    Value* ptr = getStructMemberGEP(std::move(struct_expr), std::move(member), member_type);
    if (!ptr){
        return nullptr;
    }
    std::string StructName = ""; // TODO : get the StructName, maybe with the GetStructMemberGEP
    Value* value = Builder->CreateLoad(get_type_llvm(member_type), ptr, StructName);
    return value;
}

Value* StructMemberCallExprAST::codegen(){
    if (dynamic_cast<VariableExprAST*>(StructMember->LHS.get())){
        std::unique_ptr<VariableExprAST> structNameExpr = get_Expr_from_ExprAST<VariableExprAST>(StructMember->LHS->clone());
        std::unique_ptr<VariableExprAST> structMemberExpr = get_Expr_from_ExprAST<VariableExprAST>(StructMember->RHS->clone());
        std::string StructName = structNameExpr->Name;
        std::string MemberName = structMemberExpr->Name;
        if (StructName == "reflection"){
            return refletionInstruction(MemberName, std::move(Args));
        }
    }
    Cpoint_Type lhs_type = StructMember->LHS->get_type();
    if (!lhs_type.is_struct && !lhs_type.is_ptr){
        // not struct member call
        // will handle the special name mangling in this function
        std::unique_ptr<VariableExprAST> structMemberExpr = get_Expr_from_ExprAST<VariableExprAST>(std::move(StructMember->RHS));
        std::string functionCall = structMemberExpr->Name;
        return not_struct_member_call(std::move(StructMember->LHS), functionCall, std::move(Args));
    }
    // is struct

    std::string struct_type_name = lhs_type.struct_name;

    if (lhs_type.is_object_template){
        struct_type_name = get_object_template_name(struct_type_name, *lhs_type.object_template_type_passed);
        Log::Info() << "StructName call struct function after mangling : " << struct_type_name << "\n";
    }

    auto functions =  StructDeclarations[struct_type_name]->functions;
    std::unique_ptr<VariableExprAST> structMemberExpr = get_Expr_from_ExprAST<VariableExprAST>(std::move(StructMember->RHS));
    std::string MemberName = structMemberExpr->Name;
    bool found_function = false;
    for (int i = 0; i < functions.size(); i++){
        Log::Info() << "functions.at(i) : " << functions.at(i) << "\n";
        if (functions.at(i) == MemberName){
            found_function = true;
            Log::Info() << "Is function Call" << "\n";
        }
    }
    if (!found_function){
        return LogErrorV(this->loc, "Unknown struct function member called : %s\n", MemberName.c_str());
    }
    std::vector<llvm::Value*> CallArgs;
    Value* struct_ptr = nullptr;
    if (lhs_type.is_ptr){
        struct_ptr = StructMember->LHS->codegen();
    } else {
        struct_ptr = AddrExprAST(std::move(StructMember->LHS)).codegen();
    }
    CallArgs.push_back(struct_ptr);
    for (int i = 0; i < Args.size(); i++){
        CallArgs.push_back(Args.at(i)->codegen());
    }
    Function *F = getFunction(struct_function_mangling(struct_type_name, MemberName));
    if (!F){
        Log::Info() << "struct_function_mangling(StructName, MemberName) : " << struct_function_mangling(lhs_type.struct_name, MemberName) << "\n";
        return LogErrorV(this->loc, "The function member %s called doesn't exist mangled in the scope", MemberName.c_str());
    }
    std::string call_name = "calltmp_struct";
    if (F->getReturnType()->isVoidTy()){
        call_name = "";
    }
    return Builder->CreateCall(F, CallArgs, call_name);
}

// TODO : refactor this code to be more efficient (for example a lot of the code from variableExprAST should be a function that is called after getting the struct member)
// should load : if the value returned is a ptr (so we should load it)
Value* getArrayOrVectorMemberGEP(std::unique_ptr<ExprAST> array, std::unique_ptr<ExprAST> index, Cpoint_Type& member_type, bool& should_load){
    should_load = true;
    Value* IndexV = index->codegen();
    if (!IndexV){
        return LogErrorV(emptyLoc, "error in array index");
    }
    
    auto zero = llvm::ConstantInt::get(*TheContext, llvm::APInt(64, 0, true));
    if (dynamic_cast<VariableExprAST*>(array.get())){
        std::unique_ptr<VariableExprAST> VariableExpr = get_Expr_from_ExprAST<VariableExprAST>(std::move(array));
        std::string ArrayName = VariableExpr->Name;
        if (!get_variable_type(ArrayName)){
            return LogErrorV(emptyLoc, "Tried to get a member of an array that doesn't exist : %s", ArrayName.c_str());
        }
        Cpoint_Type cpoint_type = *get_variable_type(ArrayName);
        if (!cpoint_type.is_array && !cpoint_type.is_vector_type && !cpoint_type.is_ptr){
            Log::Info() << "indexing expr type : " << cpoint_type << "\n";
            return LogErrorV(emptyLoc, "Trying to index on an expr that is neither an array, a vector or a pointer");
        }
        Cpoint_Type cpoint_type_not_modified = cpoint_type;
        Value* firstIndex = IndexV;
        bool is_constant = false;
        if (dyn_cast<Constant>(IndexV) && (cpoint_type.nb_element > 0 || cpoint_type.vector_size > 0)){
            is_constant = true;
            Constant* indexConst = dyn_cast<Constant>(IndexV);
            if (bound_checking_constant_index_array_member(indexConst, cpoint_type, emptyLoc) == -1){
                return nullptr;
            }
        }

        if (IndexV->getType() != get_type_llvm(Cpoint_Type(i64_type))){
            convert_to_type(get_cpoint_type_from_llvm(IndexV->getType()), Cpoint_Type(i64_type), IndexV);
        }
  
        if (!is_llvm_type_number(IndexV->getType())){
            return LogErrorV(emptyLoc, "index for array is not a number\n");
        }
        if (cpoint_type.is_vector_type){
            should_load = false;
            return Builder->CreateExtractElement(VariableExpr->codegen(), IndexV);
        }
  
        Value* allocated_value = get_var_allocation(ArrayName);
        if (!allocated_value){
            return LogErrorV(emptyLoc, "Trying to get the array member from an unknown variable");
        }

        std::vector<Value*> indexes = { zero, IndexV};
        Log::Info() << "Cpoint_type for array member before : " << cpoint_type << "\n";
        // TODO : WHY SOME TYPES HAVE NB_PTR > 0 BUT IS_PTR FALSE ? IT IS NOT SUPPOSED TO BE POSSIBLE, BUT WE STILL NEED THIS CHECK FOR NOW. FIX IT!!
        if (cpoint_type.nb_ptr > 0 && !cpoint_type.is_ptr){
            cpoint_type.is_ptr = true;
        }
        if (cpoint_type.is_ptr && !cpoint_type.is_array){
            Log::Info() << "array for array member is ptr" << "\n";
            cpoint_type = cpoint_type.deref_type();
            /*if (cpoint_type.nb_ptr > 1){
            cpoint_type.nb_ptr--;
            } else {
            cpoint_type.is_ptr = false;
            cpoint_type.nb_ptr = 0;
            cpoint_type.nb_element = 0;
            }*/
            Log::Info() << "Cpoint_type for array member which is ptr : " << cpoint_type << "\n";
            IndexV = Builder->CreateSExt(IndexV, Type::getInt64Ty(*TheContext)); // try to use a i64 for index
            indexes = {IndexV};
        }

        Log::Info() << "Cpoint_type for array member : " << cpoint_type << "\n";
        if (!is_constant && cpoint_type.nb_element > 0 && !Comp_context->is_release_mode && Comp_context->std_mode && index){
            if (bound_checking_dynamic_index_array_member(firstIndex, cpoint_type) == -1){
                return nullptr;
            }
        }
        member_type = cpoint_type;
        /*member_type.is_array = false;
        member_type.nb_element = 0;*/
        member_type = member_type.deref_type();
  
        Type* type_llvm = get_type_llvm(cpoint_type);
        Value* array_or_ptr = allocated_value;
        // To make argv[0] work
        if (cpoint_type_not_modified.is_ptr /*&& cpoint_type_not_modified.nb_ptr > 1*/){
        array_or_ptr = Builder->CreateLoad(get_type_llvm(Cpoint_Type(i32_type, true, 1)), allocated_value, "load_gep_ptr");
        }
        Value* ptr = Builder->CreateGEP(type_llvm, array_or_ptr, indexes, "", true);
        return ptr;
    } else if (dynamic_cast<BinaryExprAST*>(array.get())){
        if (IndexV->getType() != get_type_llvm(Cpoint_Type(i64_type))){
            convert_to_type(get_cpoint_type_from_llvm(IndexV->getType()), Cpoint_Type(i64_type), IndexV);
        }
  
        if (!is_llvm_type_number(IndexV->getType())){
            return LogErrorV(emptyLoc, "index for array is not a number\n");
        }
        std::unique_ptr<BinaryExprAST> BinOp = get_Expr_from_ExprAST<BinaryExprAST>(std::move(array));
        // TODO : uncomment this, will need to modify the structMember operator to be able to return only the gep ptr for any expressions it support
        Value* ptr = nullptr;
        Cpoint_Type binop_member_type;
        if (BinOp->Op == "."){
            ptr = getStructMemberGEP(std::move(BinOp->LHS), std::move(BinOp->RHS), binop_member_type);
        } else if (BinOp->Op == "["){
            bool should_load;
            ptr = getArrayOrVectorMemberGEP(std::move(BinOp->LHS), std::move(BinOp->RHS), binop_member_type, should_load);
        } else {
            return LogErrorV(emptyLoc, "Indexing an array is only implemented for struct member binary operators expression\n");
        }
        std::vector<Value*> indexes = { zero, IndexV};
        if (binop_member_type.is_ptr && !binop_member_type.is_array){
            indexes = { IndexV };
        }
        member_type = binop_member_type;
        /*member_type.is_array = false; // TODO : just deref the type ?
        member_type.nb_element = 0;*/
        member_type = member_type.deref_type();
        Type* type_llvm = get_type_llvm(binop_member_type);
        if (binop_member_type.is_ptr){
            type_llvm = get_type_llvm(member_type);
        }
        Value* member_ptr = Builder->CreateGEP(type_llvm, ptr, indexes, "", true);
        // TODO : finish the load Name with the IndexV in string form
        return member_ptr;

    }
    return LogErrorV(emptyLoc, "Trying to use the array member operator with an expression which it is not implemented for\n");
}


Value* getArrayMember(std::unique_ptr<ExprAST> array, std::unique_ptr<ExprAST> index){
    Cpoint_Type member_type;
    bool should_load_val = true;
    Value* ptr_or_value = getArrayOrVectorMemberGEP(std::move(array), std::move(index), member_type, should_load_val);
    std::string ArrayName = ""; // TODO
    Value* value = ptr_or_value;
    if (should_load_val){
        value = Builder->CreateLoad(get_type_llvm(member_type), ptr_or_value, ArrayName);
    }
    return value;
}

Value* AsmExprAST::codegen(){
    bool contains_out = false;
    bool contains_in = false;
    auto asm_type = Cpoint_Type(void_type);
    //std::vector<Value*> in_values_loaded;
    Value* out_var_allocation = nullptr;
    std::vector<Value*> AsmArgs;
    std::vector<Type*> AsmArgsTypes;
    for (int i = 0; i < InputOutputArgs.size(); i++){
        if (dynamic_cast<VariableExprAST*>(InputOutputArgs.at(i).ArgExpr.get())){
            auto expr = get_Expr_from_ExprAST<VariableExprAST>(std::move(InputOutputArgs.at(i).ArgExpr));
            if (!expr){
                return LogErrorV(this->loc, "Error in expression of asm macro");
            }
            if (InputOutputArgs.at(i).argType == ArgInlineAsm::ArgType::output){
                contains_out = true;
                asm_type = expr->type;
                out_var_allocation = get_var_allocation(expr->Name);
            } else if (InputOutputArgs.at(i).argType == ArgInlineAsm::ArgType::input){
                contains_in = true;
                /*in_values_loaded*/ AsmArgs.push_back(Builder->CreateLoad(get_type_llvm(expr->type), get_var_allocation(expr->Name)));
                AsmArgsTypes.push_back(get_type_llvm(expr->type));
            }
        } else {
            return LogErrorV(this->loc, "Unknown expression for \"in\" in asm macro");
        }
    }
    std::string assembly_code_str = assembly_code->str;
    std::string generated_assembly_code = "";
    std::string constraints = ""; 
    int arg_nb = 0;
    int arg_in_nb = 0;
    for (int i = 0; i < assembly_code_str.size(); i++){
        if (assembly_code_str.at(i) == '{' && assembly_code_str.at(i+1) && '}'){
            if (InputOutputArgs.size() > arg_nb){
                generated_assembly_code += "${" + std::to_string(arg_nb) + ":q}";
                if (InputOutputArgs.at(arg_nb).argType == ArgInlineAsm::ArgType::input){
                    constraints += "r,";
                    arg_in_nb++;
                } else if (InputOutputArgs.at(arg_nb).argType == ArgInlineAsm::ArgType::output){
                    constraints += "=&r,";
                }
                arg_nb++;
            } else {
                return LogErrorV(emptyLoc, "Too much format args in inline asm block");
            }
            i++;
        } else {
            generated_assembly_code += assembly_code_str.at(i);
        }
    }
    constraints += "~{dirflag},~{fpsr},~{flags},~{memory}"; // TODO : set constraints only when needed
    auto inlineAsm = InlineAsm::get(FunctionType::get(get_type_llvm(asm_type), AsmArgsTypes, false), (StringRef)generated_assembly_code, (StringRef)constraints, true, true, InlineAsm::AD_Intel); // use intel dialect
    if (contains_out){
        auto asmCalled = Builder->CreateCall(inlineAsm, AsmArgs); 
        return Builder->CreateStore(asmCalled, out_var_allocation);
    } else {
        return Builder->CreateCall(inlineAsm, AsmArgs);
    }
}

Value* SemicolonExprAST::codegen(){
    //return UndefValue::get(Type::getVoidTy(*TheContext));
    return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *BinaryExprAST::codegen() {
  if (Op.at(0) == '['){
    return getArrayMember(std::move(LHS), std::move(RHS));
  }
  if (Op.at(0) == '=' && Op.size() == 1){
    return equalOperator(std::move(LHS), std::move(RHS));
  }
  if (Op.at(0) == '.'){
    return getStructMember(std::move(LHS), std::move(RHS));
  }
  Cpoint_Type LHS_type = LHS->get_type().get_real_type();
  Cpoint_Type RHS_type = RHS->get_type();
  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;
  if (Op == "+"){
    if (L->getType()->isPointerTy()){
        convert_to_type(get_cpoint_type_from_llvm(L->getType()), Cpoint_Type(i64_type), L);
    }
    if (R->getType()->isPointerTy()){
        convert_to_type(get_cpoint_type_from_llvm(R->getType()), Cpoint_Type(i64_type), R);
    }
  }
  if (L->getType() == get_type_llvm(Cpoint_Type(i8_type)) || L->getType() == get_type_llvm(Cpoint_Type(i16_type))){
    convert_to_type(get_cpoint_type_from_llvm(L->getType()), Cpoint_Type(i32_type), L);
  } else if (R->getType() == get_type_llvm(Cpoint_Type(i8_type)) || R->getType() == get_type_llvm(Cpoint_Type(i16_type))){
    convert_to_type(get_cpoint_type_from_llvm(R->getType()), Cpoint_Type(i32_type), R);
  }
  if (L->getType() != R->getType()){
    (Log::Warning(this->loc) << "Types are not the same for the binary operation '" << Op << "' to the " << create_pretty_name_for_type(get_cpoint_type_from_llvm(L->getType())) << " and " << create_pretty_name_for_type(get_cpoint_type_from_llvm(R->getType())) << " types" << "\n").end();
    convert_to_type(get_cpoint_type_from_llvm(R->getType()) /*RHS_type*/, L->getType(), R);
  }
  // and operator only work with ints and bools returned from operators are for now doubles, TODO)
  if (Op == "&&"){
    if (get_cpoint_type_from_llvm(L->getType()).is_decimal_number_type()){
        convert_to_type(get_cpoint_type_from_llvm(L->getType()), Cpoint_Type(i32_type), L);
    }
    if (get_cpoint_type_from_llvm(R->getType()).is_decimal_number_type()){
        convert_to_type(get_cpoint_type_from_llvm(R->getType()), Cpoint_Type(i32_type), R);
    }
  }
  // TODO : make every operators compatibles with ints and other types. Possibly also refactor this in multiple function in maybe a dedicated operators.cpp file
  if (Op == "=="){
    return operators::LLVMCreateCmp(L, R, LHS_type);
  }
  if (Op == "||"){
    return operators::LLVMCreateLogicalOr(L, R);
  }
  if (Op == "&&"){
    return operators::LLVMCreateLogicalAnd(L, R);
  }
  if (Op == "!="){
    return operators::LLVMCreateNotEqualCmp(L, R, LHS_type);
  }
  if (Op == "<="){
    return operators::LLVMCreateSmallerOrEqualThan(L, R, LHS_type);
  }
  if (Op == ">="){
    return operators::LLVMCreateGreaterOrEqualThan(L, R, LHS_type);
  }
  if (Op == ">>"){
    if (LHS_type.is_unsigned()){
        return Builder->CreateLShr(L, R, "shiftrtmp");
    } else {
        return Builder->CreateAShr(L, R, "shiftrtmp");
    }
  }
  if (Op == "<<"){
    return Builder->CreateShl(L, R, "shiftltmp");
  }
  switch (Op.at(0)) {
  case '+':
    return operators::LLVMCreateAdd(L, R, LHS_type);
  case '-':
    return operators::LLVMCreateSub(L, R, LHS_type);
  case '*':
    return operators::LLVMCreateMul(L, R, LHS_type);
  case '%':
    return operators::LLVMCreateRem(L, R/*, RHS_type*/);
  case '<':
    return operators::LLVMCreateSmallerThan(L, R, LHS_type);
  case '/':
    return operators::LLVMCreateDiv(L, R, LHS_type);
  case '>':
    return operators::LLVMCreateGreaterThan(L, R, LHS_type);
  case '^':
    L = Builder->CreateXor(L, R, "xortmp");
    return L;
  case '&':
    return operators::LLVMCreateAnd(L, R);
  case '|':
    return Builder->CreateOr(L, R, "ortmp");
  default:
    // TODO : maybe reactivate this
    //return LogErrorV(this->loc, "invalid binary operator");
    break;
  }
  Function *F = getFunction(std::string("binary") + Op);
  assert(F && "binary operator not found!");
  Value *Ops[2] = { L, R };
  return Builder->CreateCall(F, Ops, "binop");
}

Cpoint_Type callexpr_get_arg_type_function(std::string Callee, int pos){
    if (FunctionProtos[Callee]){
        return FunctionProtos[Callee]->Args.at(pos).second;
    } else {
        return NamedValues[Callee]->type.args.at(pos);
    }
}

Value *CallExprAST::codegen() {
  // Look up the name in the global module table.
  Function* TheFunction = Builder->GetInsertBlock()->getParent();
  Log::Info() << "function called " << Callee << "\n";
  // TODO : instead of using an internal func prefix, use namespace (which will be the same logic, just with a different number of underscore)
  std::string internal_func_prefix = INTERNAL_FUNC_PREFIX;
  //std::string internal_func_prefix = CallExprAST::get_internal_func_prefix();
  bool is_internal = false;
  CpointDebugInfo.emitLocation(this);
  if (StructDeclarations[Callee] != nullptr){
    // TODO : maybe replace struct_test(1, 2) by struct_test {1, 2} ? so maybe remove this
    // function called is constructor of struct
    std::string constructor_name = struct_function_mangling(Callee, "Constructor__Default");
    if (FunctionProtos[constructor_name] != nullptr){
        Callee = constructor_name;
    } else {
        std::vector<Constant*> constantArgs;
        for (int i = 0; i < Args.size(); i++){
            auto Val = Args.at(i)->codegen();
            auto ConstantVal = dyn_cast<Constant>(Val);
            if (!ConstantVal){
                return LogErrorV(this->loc, "You can't initialize a struct without a constructor with non constant values");
            }
            constantArgs.push_back(ConstantVal);
        }
        return ConstantStruct::get(dyn_cast<StructType>(StructDeclarations[Callee]->struct_type), ArrayRef<Constant*>(constantArgs));
    }
  }
  if (Callee.rfind(internal_func_prefix, 0) == 0){
    is_internal = true;
    Callee = Callee.substr(internal_func_prefix.size(), Callee.size());
    Log::Info() << "internal function called " << Callee << "\n";
    if (Callee.rfind("llvm_", 0) == 0){
      return callLLVMIntrisic(Callee, Args, template_passed_type);
    }
    if (Callee == "get_filename"){
      std::string filename_without_temp = filename;
      int temp_pos;
      if ((temp_pos = filename.rfind(".temp")) != std::string::npos){
        filename_without_temp = filename.substr(0, temp_pos);
      }
      return StringExprAST(filename_without_temp).codegen();
    }
    if (Callee == "get_function_name"){
      return StringExprAST((std::string)TheFunction->getName()).codegen();
    }
    if (Callee == "dbg"){
        std::unique_ptr<ExprAST> varDbg = std::move(Args.at(0));
        return DbgMacroCodegen(std::move(varDbg));
    }
    if (Callee == "get_va_adress_systemv"){
        return GetVaAdressSystemV(std::move(Args.at(0)));
    }
    if (Callee == "print"){
        return PrintMacroCodegen(std::move(Args));
    }
    if (Callee == "unreachable"){
        return Builder->CreateUnreachable();
    }
    if (Callee == "assume"){
        return Builder->CreateAssumption(Args.at(0)->codegen());
    }
  }
  bool is_function_template = TemplateProtos[Callee] != nullptr;
  if (is_function_template){
    callTemplate(Callee, template_passed_type);
  }
  // TODO : get code like for local and global variables for local and global functions to simplify the code and minimize the number of ifs (ex : get_arg_type function with just a std::string Callee arg)
  Function *CalleeF = getFunction(Callee);
  Log::Info() << "is_function_template : " << is_function_template << "\n";
  Cpoint_Type function_return_type;
  Value* function_ptr_from_local_var = nullptr;
  bool is_variable_number_args = false;
  size_t args_size = 0;
  // including hidden struct args
  size_t real_args_size = 0;
  if (CalleeF){
    function_return_type = FunctionProtos[Callee]->cpoint_type;
    is_variable_number_args = FunctionProtos[Callee]->is_variable_number_args;
    real_args_size = CalleeF->arg_size();
    args_size = FunctionProtos[Callee]->Args.size();
  } else if (NamedValues[Callee] && NamedValues[Callee]->type.is_function){
    function_ptr_from_local_var = Builder->CreateLoad(get_type_llvm(NamedValues[Callee]->type), NamedValues[Callee]->alloca_inst, "load_func_ptr");
    is_variable_number_args = false; // TODO : add variable number of args for function pointers ?
    args_size = real_args_size = NamedValues[Callee]->type.args.size();
    function_return_type = *NamedValues[Callee]->type.return_type;
    Cpoint_Type first_arg_type = NamedValues[Callee]->type.args.at(0).type;
    if (Callee.find("__") != std::string::npos && first_arg_type.is_struct && first_arg_type.is_ptr){
        real_args_size--;
    }
  } else {
    return LogErrorV(emptyLoc, "Unknown function %s", Callee.c_str());
  }

  if (!CalleeF && !is_function_template && !function_ptr_from_local_var)
    return LogErrorV(this->loc, "Unknown function referenced %s", Callee.c_str());
  if (CalleeF){
  Log::Info() << "CalleeF->arg_size : " << CalleeF->arg_size() << "\n";
  }
  Log::Info() << "Args.size : " << Args.size() << "\n";
  bool has_sret = function_return_type.is_just_struct() && should_return_struct_with_ptr(function_return_type);
  bool has_byval = false;
  if (is_variable_number_args){
    Log::Info() << "Variable number of args" << "\n";
    if (Args.size() < real_args_size){
      return LogErrorV(this->loc, "Incorrect number of arguments passed for %s : %d args but %d expected", Callee.c_str(), Args.size(), /*CalleeF->arg_size()*/ args_size);
    }
  } else if (has_sret){
    if (real_args_size != Args.size()+1)
        return LogErrorV(this->loc, "Incorrect number of arguments passed for %s : %d args but %d expected", Callee.c_str(), Args.size(), CalleeF->arg_size());
  } else {
    // If argument mismatch error.
  if (real_args_size != Args.size())
    return LogErrorV(this->loc, "Incorrect number of arguments passed for %s : %d args but %d expected", Callee.c_str(), Args.size(), CalleeF->arg_size());
  }
  Log::Info() << "has_sret : " << has_sret << "\n";
  AllocaInst* SretArgAlloca = nullptr;
  if (has_sret){
    SretArgAlloca = CreateEntryBlockAlloca(TheFunction, function_return_type.struct_name + "_sret", function_return_type);
    int idx = 0;
    for (auto& Arg : CalleeF->args()){
        if (idx == 0){
            Log::Info() << "Adding sret attr in callexpr" << "\n";
            addArgSretAttribute(*TheContext, Arg, SretArgAlloca->getAllocatedType());
        } else {
            break;
        }
        idx++;
    }
  }
  std::vector<Value *> ArgsV;
  bool has_added_sret = false;
  if (has_sret){
    ArgsV.push_back(SretArgAlloca);
  }
  auto zero = llvm::ConstantInt::get(*TheContext, llvm::APInt(64, 0, true));
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    if (has_sret && !has_added_sret){
        i--;
        has_added_sret = true;
    } else {
    Value* temp_val;
    Cpoint_Type arg_type = Cpoint_Type(double_type);
    if (i < args_size){
        arg_type = callexpr_get_arg_type_function(Callee, i);
    }
    bool is_additional_arg = is_variable_number_args && i >= args_size;
    bool is_arg_passed_an_array = Args[i]->get_type().is_array;
    if (arg_type.is_just_struct() && should_pass_struct_byval(arg_type)){
        temp_val = AddrExprAST(Args[i]->clone()).codegen();
    } else if (arg_type.is_just_struct() && is_struct_all_type(arg_type, float_type) && struct_get_number_type(arg_type, float_type) <= 2) {
        Value* loaded_vector = Builder->CreateLoad(vector_type_from_struct(FunctionProtos[Callee]->Args.at(i).second), AddrExprAST(Args[i]->clone()).codegen());
        temp_val = loaded_vector;
    } else if ((arg_type.is_ptr || is_additional_arg) && is_arg_passed_an_array){ // passing an array to a function wanting a ptr or as additional arg of a variadic function
        temp_val = AddrExprAST(Args[i]->clone()).codegen();
        temp_val = Builder->CreateGEP(get_type_llvm(Args[i]->get_type()), temp_val, {zero, zero}, "arraydecay", true);
    } else {
        temp_val = Args[i]->codegen();
    }
    if (!temp_val){
      return nullptr;
    }
    if (i < args_size){
    if (temp_val->getType() != get_type_llvm(arg_type)){
      Log::Info() << "name of arg converting in call expr : " << ((FunctionProtos[Callee]) ? FunctionProtos[Callee]->Args.at(i).first : (std::string)"(couldn't be found because it is a function pointer)") << "\n"; // TODO : fix this ?
      //return LogErrorV(this->loc, "Arg %s type is wrong in the call of %s\n Expected type : %s, got type : %s\n", FunctionProtos[Callee]->Args.at(i).second, Callee, create_pretty_name_for_type(get_cpoint_type_from_llvm(temp_val->getType())), create_pretty_name_for_type(FunctionProtos[Callee]->Args.at(i).second));
      convert_to_type(get_cpoint_type_from_llvm(temp_val->getType()) , arg_type, temp_val);
    }
    }
    ArgsV.push_back(temp_val);
    if (!ArgsV.back())
      return nullptr;
    }
  }
  std::vector<int> pos_byval;
  std::vector<int> pos_signext;
  std::vector<int> pos_zeroext;
  for (int i = 0; i < args_size; i++){
    Cpoint_Type arg_type = callexpr_get_arg_type_function(Callee, i);
    if (arg_type.is_just_struct() && should_pass_struct_byval(arg_type)){
        has_byval = true;
        pos_byval.push_back(i);
    }
    // TODO : are i8s (chars) only signextended on x86 ? (look at clang output) 
    if (arg_type.type == i8_type && !arg_type.is_ptr && !arg_type.is_array){
        pos_signext.push_back(i);
    }
    if (arg_type.is_unsigned()){
        pos_zeroext.push_back(i);
    }
  }
  std::string NameCallTmp = "calltmp";
  if (function_return_type.type  == void_type || function_return_type.type == never_type || has_sret){
    NameCallTmp = "";
  }
  CallInst* call = nullptr;
  if (CalleeF){
    call = Builder->CreateCall(CalleeF, ArgsV, NameCallTmp);
  } else {
    Log::Info() << "function type : " << NamedValues[Callee]->type << "\n";
    std::vector<Type*> args_llvm_types;
    for (int i = 0; i < NamedValues[Callee]->type.args.size(); i++){
        args_llvm_types.push_back(get_type_llvm(NamedValues[Callee]->type.args.at(i)));
    }
    call = Builder->CreateCall(FunctionCallee(FunctionType::get(get_type_llvm(*NamedValues[Callee]->type.return_type), args_llvm_types, false), function_ptr_from_local_var), ArgsV, NameCallTmp);
  }
  if (has_sret){
    call->addParamAttr(0, Attribute::getWithStructRetType(*TheContext, SretArgAlloca->getAllocatedType()));
    call->addParamAttr(0, Attribute::getWithAlignment(*TheContext, Align(8)));
    return Builder->CreateLoad(SretArgAlloca->getAllocatedType(), SretArgAlloca);
  }
  if (has_byval){
    for (int i = 0; i < pos_byval.size(); i++){
        Cpoint_Type cpoint_byval_type = FunctionProtos[Callee]->Args.at(pos_byval.at(i)).second;
        call->addParamAttr(pos_byval.at(i), Attribute::getWithByValType(*TheContext, get_type_llvm(cpoint_byval_type)));
        call->addParamAttr(pos_byval.at(i), Attribute::getWithAlignment(*TheContext, Align(8)));
    }
  }
  if (!pos_signext.empty()){
  for (int i = 0; i < pos_signext.size(); i++){
    call->addParamAttr(pos_signext.at(i), Attribute::SExt);
  }
  }
  if (!pos_zeroext.empty()){
  for (int i = 0; i < pos_zeroext.size(); i++){
    call->addParamAttr(pos_zeroext.at(i), Attribute::ZExt);
  }
  }
  if (FunctionProtos[Callee] && FunctionProtos[Callee]->cpoint_type.type == never_type){
    return Builder->CreateUnreachable();
  }
  return call;
}

Value* AddrExprAST::codegen(){
  if (dynamic_cast<VariableExprAST*>(Expr.get())){
    std::unique_ptr<VariableExprAST> VariableExpr = get_Expr_from_ExprAST<VariableExprAST>(std::move(Expr));
    Log::Info() << "Addr Variable : " << VariableExpr->getName() << "\n";
    if (is_var_local(VariableExpr->getName())){
        AllocaInst *A = NamedValues[VariableExpr->getName()]->alloca_inst;
        if (!A){
            return LogErrorV(this->loc, "Addr Unknown variable name %s", VariableExpr->getName().c_str());
        }
        return A;
    } else {
        GlobalVariable* G = GlobalVariables[VariableExpr->getName()]->globalVar;
        if (!G)
            return LogErrorV(this->loc, "Addr Unknown variable name %s", VariableExpr->getName().c_str());
        return G;
    }
  } else if (dynamic_cast<BinaryExprAST*>(Expr.get())){
    std::unique_ptr<BinaryExprAST> binOpMember = get_Expr_from_ExprAST<BinaryExprAST>(std::move(Expr));
    if (binOpMember->Op == "."){
        Cpoint_Type temp;
        Value* ptr = getStructMemberGEP(std::move(binOpMember->LHS), std::move(binOpMember->RHS), temp);
        return ptr;
    } else if (binOpMember->Op == "["){
        Cpoint_Type temp;
        bool should_load_val;
        Value* ptr = getArrayOrVectorMemberGEP(std::move(binOpMember->LHS), std::move(binOpMember->RHS), temp, should_load_val);
        return ptr;
    } else {
        return LogErrorV(this->loc, "Unknown operator expression for addr");
    }
  }
  return LogErrorV(this->loc, "Trying to use addr with an expression which it is not implemented for");
}

Value* DerefExprAST::codegen(){
    if (dynamic_cast<VariableExprAST*>(Expr.get())){
        std::unique_ptr<VariableExprAST> VariableExpr = get_Expr_from_ExprAST<VariableExprAST>(std::move(Expr));
        if (!var_exists(VariableExpr->getName())){
            return LogErrorV(this->loc, "Unknown variable in deref : %s", VariableExpr->getName().c_str());
        }
        if (!get_variable_type(VariableExpr->getName())->is_ptr){
            return LogErrorV(this->loc, "Derefencing a variable (%s) that is not a pointer", VariableExpr->getName().c_str());
        }

        llvm::Value* VarAlloc = get_var_allocation(VariableExpr->getName());
        Cpoint_Type contained_type = *get_variable_type(VariableExpr->getName());
        contained_type.is_ptr = false;
        Log::Info() << "Deref Type : " << contained_type << "\n";
        return Builder->CreateLoad(get_type_llvm(contained_type), VarAlloc, VariableExpr->getName().c_str());
    }
    return LogErrorV(this->loc, "Trying to use deref with an expression which it is not implemented for");
}

Value* compile_time_sizeof(Cpoint_Type type, /*std::string Name, bool is_variable*/ std::unique_ptr<ExprAST> expr, Source_location loc){
    Cpoint_Type sizeof_type = type;
    /*if (is_variable){
        AllocaInst *A = NamedValues[Name]->alloca_inst;
        if (!A){
            return LogErrorV(loc, "Sizeof Unknown variable name %s", Name.c_str());
        }
        sizeof_type = NamedValues[Name]->type;
    }*/
   if (expr){
    sizeof_type = expr->get_type();
   }
    if (sizeof_type.is_just_struct() && should_pass_struct_byval(sizeof_type)){
        return LogErrorV(loc, "For now, compile time sizeof of structs is not implemented"); // TODO
        // for now can't find the size because of padding. Take just the size without padding ? find a way to calculate the padding ? Juste x2 the size to be safe ?
    }
    if (sizeof_type.is_array){
        return LogErrorV(loc, "For now, compile time sizeof of array is not implemented");
    }
    if (sizeof_type.is_ptr){
        int pointer_size = get_pointer_size();
        return ConstantInt::get(*TheContext, APInt(32, (uint64_t)pointer_size/8, false));
    }
    int size_type = type.get_number_of_bits();
    return ConstantInt::get(*TheContext, APInt(32, (uint64_t)size_type/8, false));
}

Value* SizeofExprAST::codegen(){
  if (Comp_context->compile_time_sizeof){
    return compile_time_sizeof(type, std::move(expr), this->loc);
  }
  Log::Info() << "codegen sizeof" << "\n";
  auto one = llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 1, true));
  if (is_type){
    Log::Info() << "codegen sizeof is type" << "\n";
    Cpoint_Type cpoint_type = type;
    /*if (cpoint_type.is_struct){
        return getSizeOfStruct(temp_alloca);
    }*/
    Type* llvm_type = get_type_llvm(cpoint_type);
    Value* size = Builder->CreateGEP(llvm_type, Builder->CreateIntToPtr(ConstantInt::get(Builder->getInt64Ty(), 0),llvm_type->getPointerTo()), {one});
    size =  Builder->CreatePtrToInt(size, get_type_llvm(Cpoint_Type(i32_type)));
    return size;
  } else {
    Log::Info() << "codegen sizeof is variable" << "\n";
    /*AllocaInst *A = NamedValues[Name]->alloca_inst;
    if (!A){
        return LogErrorV(this->loc, "Sizeof Unknown variable name %s", Name.c_str());
    }*/ 
    assert(expr != nullptr);
    Cpoint_Type cpoint_type = expr->get_type();
    //if (cpoint_type.is_just_struct()){
    //Value* size =  getSizeOfStruct(A);
    //size = Builder->CreateMul(size, ConstantInt::get(*TheContext, APInt(32, 8, false)), "mul_converting_in_bits");
    //return size;
    /*std::vector<Value*> ArgsV;
    ArgsV.push_back(A);
    ArgsV.push_back(ConstantInt::get(*TheContext, llvm::APInt(1, 0, true))); // return -1 if object size is unknown
    ArgsV.push_back(ConstantInt::get(*TheContext, llvm::APInt(1, 1, true))); // return unknown size if null is passed
    ArgsV.push_back(ConstantInt::get(*TheContext, llvm::APInt(1, 0, true))); // is the calculation made at runtime
    std::vector<Type*> OverloadedTypes;
    OverloadedTypes.push_back(get_type_llvm(i32_type));
    OverloadedTypes.push_back(get_type_llvm(i64_type));
    Function *CalleeF = Intrinsic::getDeclaration(TheModule.get(), Intrinsic::objectsize, OverloadedTypes);
    return Builder->CreateCall(CalleeF, ArgsV, "sizeof_calltmp");*/
    //}
    // TODO : utiliser getAllocationSize sur les AllocaInst
    Type* llvm_type = get_type_llvm(cpoint_type);
    Value* size = Builder->CreateGEP(llvm_type, Builder->CreateIntToPtr(ConstantInt::get(Builder->getInt64Ty(), 0),llvm_type->getPointerTo()), {one});
    size = Builder->CreatePtrToInt(size, get_type_llvm(Cpoint_Type(i32_type)));
    return size;
  } 
}

Value* TypeidExprAST::codegen(){
    return getTypeIdLLVM(val->get_type());
    /*Value* valueLLVM = val->codegen();
    return getTypeId(valueLLVM);*/
}

Value* CastExprAST::codegen(){
    Value* val = ValToCast->codegen();
    convert_to_type(get_cpoint_type_from_llvm(val->getType()), type, val);
    return val;
}

Value* NullExprAST::codegen(){
    return ConstantPointerNull::get(PointerType::get(*TheContext, 0));
}

Value* BoolExprAST::codegen(){
  if (val){
    return ConstantInt::get(*TheContext, APInt(1, 1, true));
  } else {
    return ConstantInt::get(*TheContext, APInt(1, 0, true));
  }
}

Value* CharExprAST::codegen(){
  Log::Info() << "Char Codegen : " << c << "\n";
  return ConstantInt::get(*TheContext, APInt(8, c, true));
}

void TestAST::codegen(){
  Log::Info() << "Codegen test " << this->description << "\n";
  
  testASTNodes.push_back(this->clone()); // TODO : move instead ?
}

/*void generateTests(){
  if (!Comp_context->test_mode){
    return;
  }
  std::vector<std::pair<std::string,Cpoint_Type>> Args;
  Args.push_back(std::make_pair("argc", Cpoint_Type(double_type, false)));
  Args.push_back(std::make_pair("argv",  Cpoint_Type(i8_type, true, 2)));
  auto Proto = std::make_unique<PrototypeAST>(emptyLoc, "main", Args, Cpoint_Type(double_type));
  std::vector<std::unique_ptr<ExprAST>> Body;
  // add a function to generate gc_init
  if (Comp_context->gc_mode){
    std::vector<std::unique_ptr<ExprAST>> Args_gc_init;
    auto E_gc_init = std::make_unique<CallExprAST>(emptyLoc, "gc_init", std::move(Args_gc_init), Cpoint_Type(double_type));
    Body.push_back(std::move(E_gc_init));
  }
  Log::Info() << "testASTNodes size : " << testASTNodes.size() << "\n";
  std::unique_ptr<ExprAST> printfCall;
  std::vector<std::unique_ptr<ExprAST>> ArgsPrintf;
  for (int i = 0; i < testASTNodes.size(); i++){
    // create print for test description
    ArgsPrintf.clear();
    std::string temp_description = testASTNodes.at(i)->description;
    std::unique_ptr<ExprAST> strExprAST;
    strExprAST = std::make_unique<StringExprAST>("Test %s running (in %s)\n");
    ArgsPrintf.push_back(std::move(strExprAST));
    strExprAST = std::make_unique<StringExprAST>(temp_description);
    ArgsPrintf.push_back(std::move(strExprAST));
    strExprAST = std::make_unique<StringExprAST>(first_filename);
    ArgsPrintf.push_back(std::move(strExprAST));
    printfCall = std::make_unique<CallExprAST>(emptyLoc, "printf", std::move(ArgsPrintf), Cpoint_Type(double_type));
    Body.push_back(std::move(printfCall));
    for (int j = 0; j < testASTNodes.at(i)->Body.size(); j++){
      Body.push_back(std::move(testASTNodes.at(i)->Body.at(j)));
    }
  }
  Body.push_back(std::make_unique<NumberExprAST>(0));
  
  auto funcAST = std::make_unique<FunctionAST>(std::mo&e(Proto), std::move(Body));
  funcAST->codegen();
}*/

/*void generateExterns(){
    std::set<std::string> alreadyGeneratedFunctions;
    for (int i = 0; i < externFunctionsToGenerate.size(); i++){
        if (alreadyGeneratedFunctions.find(externFunctionsToGenerate.at(i)->Name) == alreadyGeneratedFunctions.end()){
            Function *F = Function::Create(externFunctionsToGenerate.at(i)->functionType, Function::ExternalLinkage, externFunctionsToGenerate.at(i)->Name, TheModule.get());
            unsigned Idx = 0;
            for (auto &Arg : F->args()){
                Arg.setName(externFunctionsToGenerate.at(i)->Args[Idx++].first);
            }
            alreadyGeneratedFunctions.insert(externFunctionsToGenerate.at(i)->Name);
        }
    }
}*/

/*void generateClosures(){
    if (!closuresToGenerate.empty()){
        for (int i = 0; i < closuresToGenerate.size(); i++){
            closuresToGenerate.at(i)->codegen();
        }
    }
}*/

Function *PrototypeAST::codegen() {
  std::vector<Type *> type_args;
  for (int i = 0; i < Args.size(); i++){
    Cpoint_Type arg_type = Args.at(i).second;
    //if (arg_type.is_struct && !arg_type.is_ptr && !arg_type.is_array && should_pass_struct_byval(arg_type)){
    if (arg_type.is_just_struct() && is_struct_all_type(arg_type, float_type) && struct_get_number_type(arg_type, float_type) <= 2) {
        type_args.push_back(vector_type_from_struct(arg_type));
    } else {
    if (arg_type.is_just_struct() && should_pass_struct_byval(arg_type)){
        arg_type.is_ptr = true;
    }
    type_args.push_back(get_type_llvm(arg_type));
    }
  }
  FunctionType *FT;
  bool has_sret = false;
  if (Name == "main"){
    std::vector<Type*> args_type_main;
    args_type_main.push_back(get_type_llvm(Cpoint_Type(i32_type)));
    args_type_main.push_back(get_type_llvm(Cpoint_Type(void_type, true))->getPointerTo());
    FT = FunctionType::get(/*get_type_llvm(cpoint_type)*/ get_type_llvm(Cpoint_Type(i32_type)), args_type_main, false);
  } else {
  if (cpoint_type.is_just_struct()){
    // replace this by if (should_return_struct_with_ptr())
    if (should_return_struct_with_ptr(cpoint_type)){
        has_sret = true;
        Cpoint_Type sret_arg_type = cpoint_type;
        sret_arg_type.is_ptr = true;
        type_args.insert(type_args.begin(), get_type_llvm(sret_arg_type));
    }
  }
  Cpoint_Type return_type = (has_sret) ? Cpoint_Type(void_type) : cpoint_type;
  FT = FunctionType::get(get_type_llvm(return_type), type_args, is_variable_number_args);
  }

  if (is_in_extern){
    externFunctionsToGenerate.push_back(std::make_unique<ExternToGenerate>(Name, FT, Args));
    FunctionProtos[this->getName()] = this->clone();
    return nullptr;
  }
  GlobalValue::LinkageTypes linkageType = Function::ExternalLinkage;
  if (is_private_func){
    linkageType = Function::InternalLinkage;
  }
  Function *F = Function::Create(FT, linkageType, Name, TheModule.get());
  if (cpoint_type.type == never_type){
    F->addFnAttr(Attribute::NoReturn);
    F->addFnAttr(Attribute::NoUnwind);
  }
  // Set names for all arguments.
  unsigned Idx = 0;
  bool has_added_sret = false;
  for (auto &Arg : F->args()){
    //if (Args[Idx++].first != "..."){
    if (has_sret && Idx == 0 && !has_added_sret){
    addArgSretAttribute(*TheContext, Arg, get_type_llvm(cpoint_type));
    Arg.setName("sret_arg");
    Idx = 0;
    has_added_sret = true;
    } else if (Args.at(Idx).second.is_just_struct() && should_pass_struct_byval(Args.at(Idx).second)){
        Log::Info() << "should_pass_struct_byval arg " << Args.at(Idx).first << " : " << Args.at(Idx).second << "\n";
        Cpoint_Type arg_type = get_cpoint_type_from_llvm(get_type_llvm(Args.at(Idx).second));
        Cpoint_Type by_val_ptr_type = arg_type;
        by_val_ptr_type.is_ptr = true;
        by_val_ptr_type.nb_ptr++;
        Arg.mutateType(get_type_llvm(by_val_ptr_type));
        Arg.addAttr(Attribute::getWithByValType(*TheContext, get_type_llvm(arg_type)));
        Arg.addAttr(Attribute::getWithAlignment(*TheContext, Align(8)));
        Arg.setName(Args[Idx++].first);
    } else {
        Arg.setName(Args[Idx++].first);
    }
    //}
  }
  FunctionProtos[this->getName()] = this->clone();
  GeneratedFunctions[this->getName()] = F;
  return F;
}

Function *FunctionAST::codegen() {
  /*if (Proto->getName() == "main"){
  std::ofstream out_debug_ast;
  out_debug_ast.open("main_ast.temp");
  out_debug_ast << this->get_ast_string() << "\n";
  out_debug_ast.close();
  }*/
  codegenStructTemplates();
  codegenEnumTemplates();
  auto &P = *Proto;
  Log::Info() << "FunctionAST Codegen : " << Proto->getName() << "\n";
  FunctionProtos[Proto->getName()] = Proto->clone(); // TODO : move this assignement and all the others to FunctionProtos from the codegen step to the ast step 
  // First, check for an existing function from a previous 'extern' declaration.
  std::string name = P.getName();
  Log::Info() << "Name " << name << "\n";
  Function *TheFunction = getFunction(P.getName());
  /*if (!TheFunction)
    TheFunction = Proto->codegen();*/

  if (!TheFunction)
    return nullptr;
  if (P.isBinaryOp()){
    std::string op = "";
    op += P.getOperatorName();
    BinopPrecedence[op] = P.getBinaryPrecedence();
  }

  // Create a new basic block to start insertion into.
  BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);
  DIFile *Unit = nullptr;
  DIScope *FContext;
  DISubprogram *SP = nullptr;
  unsigned LineNo = 0;
  LineNo = P.getLine();
  Log::Info() << "LineNo after P.getLine : " << LineNo << "\n";
  if (debug_info_mode){
  // TODO : move this to debuginfo.cpp
  Unit = DBuilder->createFile(CpointDebugInfo.TheCU->getFilename(),
                                      CpointDebugInfo.TheCU->getDirectory());
  FContext = Unit;
  unsigned ScopeLine = 0;
  SP = DBuilder->createFunction(
    FContext, P.getName(), StringRef(), Unit, LineNo,
    DebugInfoCreateFunctionType(P.cpoint_type, P.Args),
    ScopeLine,
    DINode::FlagPrototyped,
    DISubprogram::SPFlagDefinition);
  TheFunction->setSubprogram(SP);
  CpointDebugInfo.LexicalBlocks.push_back(SP);
  CpointDebugInfo.emitLocation(nullptr);
  }
  bool contains_return_or_unreachable_or_never_call = false;
  for (int i = 0; i < Body.size(); i++){
    if (Body.at(i)->contains_expr(ExprType::Unreachable) || Body.at(i)->contains_expr(ExprType::NeverFunctionCall)){
        contains_return_or_unreachable_or_never_call = true;
    }
    if (i != Body.size()-1 && Body.at(i)->contains_expr(ExprType::Return)){
        contains_return_or_unreachable_or_never_call = true;
    }
  }
  bool is_return_never_type = false;
  if (P.cpoint_type.type == never_type){
    if (Body.back()->get_type().type != never_type){
        LogErrorE("Never type is not returned when the functions has a never return type");
        return nullptr;
    }
    is_return_never_type = true;
  }
  // Record the function arguments in the NamedValues map.
  NamedValues.clear();
  // clean the map for Template types
  unsigned ArgIdx = 0;
  if (P.getName() == "main"){
    int i = 0;
    for (auto &Arg : TheFunction->args()){
      Cpoint_Type type = Cpoint_Type(double_type, false);
      if (i == 1){
        //type = argv_type;
        type = Cpoint_Type(i8_type, true, 2);
      } else {
        //type = i32_type;
        type = Cpoint_Type(i32_type);
      }
      AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName(), /*Cpoint_Type(type, false)*/ type);
      if (debug_info_mode){
      debugInfoCreateParameterVariable(SP, Unit, Alloca, /*Cpoint_Type(type, false)*/ type, Arg, ArgIdx, LineNo);
      }
      /*DILocalVariable *D = DBuilder->createParameterVariable(
      SP, Arg.getName(), ++ArgIdx, Unit, LineNo, CpointDebugInfo.getDoubleTy(),
      true);

      DBuilder->insertDeclare(Alloca, D, DBuilder->createExpression(),
                          DILocation::get(SP->getContext(), LineNo, 0, SP),
                          Builder->GetInsertBlock());*/
      Builder->CreateStore(&Arg, Alloca);
      Log::Info() << "type args MAIN function : " << type << "\n";
      NamedValues[std::string(Arg.getName())] = std::make_unique<NamedValue>(Alloca, /*Cpoint_Type(type, false)*/ type);
      i++;
    }
  } else {
  bool has_sret = false;
  if (P.cpoint_type.is_just_struct() && should_return_struct_with_ptr(P.cpoint_type)){
    has_sret = true;
  }

  int i = 0;
  unsigned ArgIdx = 0;
  for (auto &Arg : TheFunction->args()){
    Cpoint_Type cpoint_type_arg;
    bool has_by_val = false;
    if (has_sret){
        cpoint_type_arg = get_cpoint_type_from_llvm(Arg.getType());
        i--;
    } else if (P.Args.at(i).second.is_just_struct()  && should_pass_struct_byval(P.Args.at(i).second)){
        has_by_val = true;
        cpoint_type_arg = P.Args.at(i).second;
    } else {
        cpoint_type_arg = P.Args.at(i).second;
    }
    Log::Info() << "cpoint_type_arg : " << cpoint_type_arg << "\n";
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName(), cpoint_type_arg);
    if (debug_info_mode){
        debugInfoCreateParameterVariable(SP, Unit, Alloca, cpoint_type_arg, Arg, ArgIdx, LineNo);
    }
    if (has_by_val){
        // TODO ?
        //Builder->CreateMemCpy(Alloca, Alloca->getAlign(), &Arg, Arg.getParamAlign(), /*SizeofExprAST(get_cpoint_type_from_llvm(Alloca->getAllocatedType()), false, "").codegen()*/ find_struct_type_size(cpoint_type_arg)/8 * 2 /*IS A HACK : TODO find how to make it work, advice : look at clang generated ir*/);
        Builder->CreateStore(&Arg, Alloca);
    } else {
        Builder->CreateStore(&Arg, Alloca);
    }
    Log::Info() << "added arg " << (std::string)Arg.getName() << " to NamedValues" << "\n";
    NamedValues[std::string(Arg.getName())] = std::make_unique<NamedValue>(Alloca, cpoint_type_arg);
    i++;
  }
  }
  //Type* ret_val_type = nullptr;
  bool is_last_expr_return = false;
  std::unique_ptr<ExprAST> last_expr = nullptr;
  if (!Body.empty() && dynamic_cast<ReturnAST*>(Body.back().get())){
    is_last_expr_return = true;
    last_expr = Body.back()->clone();
    //ret_val_type = get_type_llvm(dynamic_cast<ReturnAST*>(Body.back().get())->returned_expr->get_type());
  }

  if (debug_info_mode){
  for (int i = 0; i < Body.size(); i++){
  CpointDebugInfo.emitLocation(Body.at(i).get());
  }
  }
  //codegenStructTemplates();

  createScope();
  //Value *RetVal = nullptr;
  //std::cout << "BODY SIZE : " << Body.size() << std::endl;
  bool is_last_expr_a_goto = !Body.empty() && dynamic_cast<GotoExprAST*>(Body.back().get());
  bool is_last_expr_an_infinite_loop = false;
  if (!Body.empty() && dynamic_cast<LoopExprAST*>(Body.back().get())){
    is_last_expr_an_infinite_loop = dynamic_cast<LoopExprAST*>(Body.back().get())->is_infinite_loop;
  }
  
  Value *RetVal = codegenBody(Body);
  /*for (int i = 0; i < Body.size(); i++){
    RetVal = Body.at(i)->codegen();
  }*/

  endScope();
  Type* ret_val_type;
  // if the last expr is not a return
  if (is_last_expr_return){
    assert(last_expr != nullptr);
    assert(dynamic_cast<ReturnAST*>(last_expr.get()) != nullptr);
    std::cout << "returned_expr : " << dynamic_cast<ReturnAST*>(last_expr.get())->returned_expr->to_string() << std::endl;
    ret_val_type = get_type_llvm(dynamic_cast<ReturnAST*>(last_expr.get())->returned_expr->get_type());
  } else {
    if (!RetVal){
        ret_val_type = get_type_llvm(Cpoint_Type(void_type));
    } else {
        ret_val_type = RetVal->getType();
    }
  }

  //if (RetVal) {
    Log::Info() << "before sret P.cpoint_type : " << P.cpoint_type << "\n";
    if (RetVal){
    Log::Info() << "before sret RetVal->getType()->isStructTy() : " << /*RetVal->getType()*/ ret_val_type->isStructTy() << "\n";
    }
    if  (P.cpoint_type.is_just_struct() && should_return_struct_with_ptr(P.cpoint_type) && /*RetVal && RetVal->getType()*/ ret_val_type->isStructTy()){
        Log::Info() << "sret storing in return" << "\n";
        /*auto intrisicId = Intrinsic::memcpy;
        Function* memcpyF = Intrinsic::getDeclaration(TheModule.get(), intrisicId);
        std::vector<Value*> ArgsV;
        ArgsV.push_back(VariableExprAST(emptyLoc, "sret_arg", *get_variable_type("sret_arg")).codegen());
        ArgsV.push_back(RetVal);
        ArgsV.push_back(SizeofExprAST(P.cpoint_type, false, "").codegen());
        ArgsV.push_back(BoolExprAST(false).codegen());
        Builder->CreateCall(memcpyF, ArgsV, "sret_memcpy");*/
        //Builder->CreateStore(RetVal, get_var_allocation("sret_arg"), false);
        //AllocaInst* sret_arg_allocation = dyn_cast<AllocaInst>(get_var_allocation("sret_arg"));
        std::string sret_arg_name = "sret_arg";
        Value* sret_ptr = get_var_allocation(sret_arg_name);
        //Value* sret_ptr = VariableExprAST(emptyLoc, sret_arg_name, *get_variable_type(sret_arg_name)).codegen();
        AllocaInst* temp_var = CreateEntryBlockAlloca(TheFunction, "temp_retvar_sret", RetVal->getType());
        Builder->CreateStore(RetVal, temp_var);
        /*std::vector<Value*> Args;
        Args.push_back(getSizeOfStruct(temp_var));
        Builder->CreateCall(getFunction("printi"), Args);*/
        //Builder->CreateCall(getFunction("printi"), std::vector<Value*>({getSizeOfStruct(sret_ptr)})); // for debugging purpose
        Builder->CreateMemCpy(sret_ptr, MaybeAlign(0), /*RetVal*/ temp_var, MaybeAlign(0), getSizeOfStruct(sret_ptr));
        Builder->CreateRetVoid();
        //goto after_ret;
    } else {
    /*if (TheFunction->getReturnType() == get_type_llvm(Cpoint_Type(void_type)) || !RetVal){
        // void is represented by nullptr
        RetVal = nullptr;
        goto before_ret;
    }*/

   if (TheFunction->getReturnType() == get_type_llvm(Cpoint_Type(void_type))){
        // void is represented by nullptr
        RetVal = nullptr;
    }

    /*if (!RetVal){
        goto before_ret;
    }*/

    if (RetVal){
    // Finish off the function.
    if (/*RetVal->getType()*/ ret_val_type == get_type_llvm(Cpoint_Type(void_type)) && /*TheFunction->getReturnType() != get_type_llvm(Cpoint_Type(void_type))*/ Proto->cpoint_type.type != void_type){
        if (P.getName() == "main"){
            RetVal = ConstantInt::get(*TheContext, APInt(32, 0, true));
            // TODO : maybe do an error if main function doesn't return an int and verify it after converting instead of createing the return 0
        } else {
            (Log::Warning(emptyLoc) << "Missing return value in function (the return value is void)" << "\n").end();
        }
    }
    if (P.getName() == "main" && ret_val_type != get_type_llvm(Proto->cpoint_type)){
        RetVal = ConstantInt::get(*TheContext, APInt(32, 0, true));
        ret_val_type = get_type_llvm(Cpoint_Type(i32_type));
    }

    if (!is_last_expr_return && /*RetVal->getType()*/ ret_val_type != get_type_llvm(Cpoint_Type(void_type)) && /*RetVal->getType()*/ ret_val_type != TheFunction->getReturnType()){
      convert_to_type(get_cpoint_type_from_llvm(RetVal->getType()), TheFunction->getReturnType(), RetVal);
    }
    if (/*RetVal->getType()*/ ret_val_type != TheFunction->getReturnType()){
        //return LogErrorF(emptyLoc, "Return type is wrong in the %s function", P.getName().c_str());
        auto wrong_return_type_warning = Log::Warning(emptyLoc);
        wrong_return_type_warning.head << "Return type is wrong in the " << P.getName() << " function" << "\n";
        wrong_return_type_warning.content << "Expected type : " << create_pretty_name_for_type(get_cpoint_type_from_llvm(TheFunction->getReturnType())) << ", got type : " << create_pretty_name_for_type(get_cpoint_type_from_llvm(RetVal->getType())) << "\n";
        wrong_return_type_warning.end();
    }
    }
//before_ret:
    //std::cout << "is_last_expr_return : " << is_last_expr_return << std::endl; 
    if (!contains_return_or_unreachable_or_never_call && !is_return_never_type && !is_last_expr_return){
    // only verify if the last expr is a goto if the function is main (because it will create a ret by default)
    //std::cout << "ret_val_type : " << get_cpoint_type_from_llvm(ret_val_type) << std::endl;
    //std::cout << "is RetVal nullptr : " << (RetVal == nullptr) << std::endl;
    if ((!is_last_expr_a_goto && !is_last_expr_an_infinite_loop) || P.getName() != "main"){
        if (RetVal && !ret_val_type->isVoidTy()){
            Builder->CreateRet(RetVal);
        } else {
            Builder->CreateRetVoid();
        }
    }
    }

    }
//after_ret:
    /*std::error_code EC;
    auto dump_file = raw_fd_ostream(StringRef("dump_" + P.getName() + ".ll"), EC);
    TheFunction->print(dump_file);*/
    
    CpointDebugInfo.LexicalBlocks.pop_back();
    // Validate the generated code, checking for consistency.
    // TODO : maybe enable this only in somes cases
    std::string error_str;    
    raw_string_ostream string_ostream(error_str);
    if (llvm::verifyFunction(*TheFunction, &string_ostream)){
        std::error_code EC;
        auto dump_file = raw_fd_ostream(StringRef("dump_" + P.getName() + ".ll"), EC);
        TheFunction->print(dump_file);
        dump_file.close();
        /*std::error_code EC2;
        auto dump_module_file = raw_fd_ostream(StringRef("dump_module.ll"), EC2);
        TheModule->print(dump_module_file, nullptr);
        dump_module_file.close();*/
        LogErrorV(emptyLoc, "LLVM ERROR : %s\n", error_str.c_str());
        return nullptr;
    }
    return TheFunction;
  //}
  
  /*// Error reading body, remove function.
  TheFunction->eraseFromParent();
  if (P.isBinaryOp()){
    std::string op = "";
    op += P.getOperatorName();
    BinopPrecedence.erase(op);
  }
  if (debug_info_mode){
  CpointDebugInfo.LexicalBlocks.pop_back();
  }
  return LogErrorF(emptyLoc, "Return Value failed for the function");*/
}

void TypeDefAST::codegen(){
  return;
}

void ModAST::codegen(){
  for (int i = 0; i < function_protos.size(); i++){
    std::unique_ptr<PrototypeAST> function_proto = std::move(function_protos.at(i));
    std::string mangled_function_name = module_mangling(mod_name, function_proto->Name);
    function_proto->Name = mangled_function_name;
    function_proto->codegen();
    
    FunctionProtos[mangled_function_name] = function_proto->clone();
  }
  for (int i = 0; i < functions.size(); i++){
    std::unique_ptr<FunctionAST> function = std::move(functions.at(i));
    std::string mangled_function_name = module_mangling(mod_name, function->Proto->Name);
    function->Proto->Name = mangled_function_name;
    function->codegen();
    
    // functions.at(i)->Proto
  }
  for (int i = 0; i < structs.size(); i++){
    // TODO
    fprintf(stderr, "STRUCTS IN MOD NOT IMPLEMENTED YET\n");
    exit(1);
  }
  for (int i = 0; i < mods.size(); i++){
    std::unique_ptr<ModAST> mod = std::move(mods.at(i));
    mod->mod_name = module_mangling(mod_name, mod->mod_name);
    mod->codegen();
  }
  return;
}

Value* CommentExprAST::codegen(){
    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

GlobalVariable* GlobalVariableAST::codegen(){
  Constant* InitVal = nullptr;
  if (is_extern && Init){
    return LogErrorGLLVM("Init Value found even though the global variable is extern");
  }
  if (!is_extern) {
  InitVal = get_default_constant(cpoint_type);
  if (Init){
    InitVal = from_val_to_constant(Init->codegen(), cpoint_type);
    if (!InitVal){
        return LogErrorGLLVM("The constant initialization of the global variable couldn't be converted to a constant");
    }
    //InitVal = nullptr;
  }
  }
  GlobalValue::LinkageTypes linkage = GlobalValue::ExternalLinkage;
  if (is_array){
    auto indexVal = index->codegen();
    int indexD = from_val_to_int(indexVal);
    cpoint_type.is_array = true;
    cpoint_type.nb_element = indexD;
  }
  GlobalVariable* globalVar = new GlobalVariable(*TheModule, get_type_llvm(cpoint_type), is_const, linkage, InitVal, varName);
  if (section_name != ""){
    globalVar->setSection(section_name);
  }
  GlobalVariables[varName] = std::make_unique<GlobalVariableValue>(cpoint_type, globalVar);
  return globalVar;
}

//bool break_found = false; 

Value *IfExprAST::codegen() {
  Log::Info() << "IfExprAST codegen" << "\n";
  CpointDebugInfo.emitLocation(this);
  Cpoint_Type cond_type = Cond->get_type();
  Log::Info() << "cond_type : " << cond_type << "\n";
  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;
  // TODO : use the cond_type instead of 
  //if (CondV->getType() /*cond_type.type*/ != get_type_llvm(Cpoint_Type(bool_type))){
  if (cond_type.type != bool_type || cond_type.is_vector_type){
    // TODO : create default comparisons : if is pointer, compare to null, if number compare to 1, etc
    Log::Info() << "Not got bool i1 to if, got : " << cond_type << " (llvm : " << get_cpoint_type_from_llvm(CondV->getType()) << ")" << "\n";
    convert_to_type(/*get_cpoint_type_from_llvm(CondV->getType())*/ cond_type, Cpoint_Type(bool_type), CondV);
  }

  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  Type* phiType = Type::getDoubleTy(*TheContext);
  // TODO : replace the phitype from the function return type to the type of the ThenV
  phiType = TheFunction->getReturnType();

  // Create blocks for the then and else cases.  Insert the 'then' block at the
  // end of the function.
  bool has_one_branch_if = false;

  BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");
  Builder->CreateCondBr(CondV, ThenBB, ElseBB);
  // Emit then value.
  Builder->SetInsertPoint(ThenBB);
  bool has_then_return = /*Then->contains_return()*/ Then->contains_expr(ExprType::Return);
  bool has_then_break = Then->contains_expr(ExprType::Break);
  bool has_then_unreachable = Then->contains_expr(ExprType::Unreachable);
  bool has_then_never_function_call = Then->contains_expr(ExprType::NeverFunctionCall);
  Value *ThenV = Then->codegen();
  if (ThenV->getType() != Type::getVoidTy(*TheContext)){
  phiType = ThenV->getType();
  }
  /*if (ThenV->getType() != Type::getVoidTy(*TheContext) && ThenV->getType() != phiType){
    convert_to_type(get_cpoint_type_from_llvm(ThenV->getType()), phiType, ThenV);
  }*/
  if (!has_then_break /*!break_found*/ && !has_then_return && !has_then_unreachable && !has_then_never_function_call){
    Builder->CreateBr(MergeBB);
  } else {
    has_one_branch_if = true;
  }
  //break_found = false;
  // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
  ThenBB = Builder->GetInsertBlock();
  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);
  Value *ElseV = nullptr;
  bool has_else_return = false;
  bool has_else_break = false;
  bool has_else_unreachable = false;
  bool has_else_never_function_call = false;
  if (Else){
    has_else_return = /*Else->contains_return()*/ Else->contains_expr(ExprType::Return);
    has_else_break = Else->contains_expr(ExprType::Break);
    has_else_unreachable = Else->contains_expr(ExprType::Unreachable);
    has_else_never_function_call = Else->contains_expr(ExprType::NeverFunctionCall);
    ElseV = Else->codegen();
    if (!ElseV){
        return nullptr;
    }
  if (ElseV->getType() != Type::getVoidTy(*TheContext) && ElseV->getType() != phiType){
    if (!convert_to_type(get_cpoint_type_from_llvm(ElseV->getType()), phiType, ElseV)){
        return LogErrorV(this->loc, "Mismatch, expected : %s, got : %s", get_cpoint_type_from_llvm(phiType).c_stringify(), get_cpoint_type_from_llvm(ElseV->getType()).c_stringify());
    }
  }
  }
 
  if (!has_else_break && !has_else_return && !has_else_unreachable && !has_else_never_function_call){ // TODO : add has_else unreachable ?
    Builder->CreateBr(MergeBB);
  } else {
    has_one_branch_if = true;
  }
  // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
  ElseBB = Builder->GetInsertBlock();

  // Emit merge block.
  //TheFunction->getBasicBlockList().push_back(MergeBB);
  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);

  if (TheFunction->getReturnType() == get_type_llvm(Cpoint_Type(void_type))){
    //return nullptr;
    return Constant::getNullValue(get_type_llvm(Cpoint_Type((void_type))));
  }
  if (Else && !has_one_branch_if){
  PHINode* PN = Builder->CreatePHI(phiType, 2, "iftmp");
  if (ThenV->getType() == Type::getVoidTy(*TheContext) && phiType != Type::getVoidTy(*TheContext)){
    //ThenV = ConstantFP::get(*TheContext, APFloat(0.0));
    ThenV = get_default_constant(get_cpoint_type_from_llvm(phiType));
  }

  if (ElseV == nullptr || ElseV->getType() == Type::getVoidTy(*TheContext)){
    if (phiType != Type::getVoidTy(*TheContext)){
    //ElseV = ConstantFP::get(*TheContext, APFloat(0.0));
    
    // TODO : for now creates a constant if else is empty. It creates a problem in this case
    // var a = if [EXPRESSION] { 
    //   gc_malloc(sizeof int * 2)
    // }
    // Here we should not create a null pointer by default without the user knowing
    // we should make an error if the if side is not void and the else has no return value 
    //if (is_else_empty){
    ElseV = get_default_constant(get_cpoint_type_from_llvm(phiType));
    //}
    //return LogErrorV(this->loc, "Mismatch of th")
    }
  }
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
  }
  //return nullptr;
  return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}


// TODO : maybe make all type converts in ast ? (with a convert type/implicit convert type ast node like in the clang ast)
Value* ReturnAST::codegen(){
  Value* value_returned = (returned_expr) ? returned_expr->codegen() : nullptr;
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  if (TheFunction->getReturnType() != value_returned->getType() && TheFunction->getReturnType() != get_type_llvm(Cpoint_Type(void_type))){
    convert_to_type(get_cpoint_type_from_llvm(value_returned->getType()), TheFunction->getReturnType(), value_returned);
  }
  //return Builder->CreateRet(value_returned);
  return Builder->CreateRet(value_returned);
  //return value_returned; // even though the type of returnAST is a never type, LLVM will prefer that we just return the Value
  
  //return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value* GotoExprAST::codegen(){
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  BasicBlock* bb = get_basic_block(TheFunction, label_name);
  if (bb == nullptr){
    //(Log::Warning(this->loc) << "Basic block couldn't be found in goto" << "\n").end();
    return LogErrorV(this->loc, "Basic block couldn't be found in goto");
  }
  Builder->CreateBr(bb);
  return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value* LabelExprAST::codegen(){
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  BasicBlock* labelBB = BasicBlock::Create(*TheContext, label_name, TheFunction);
  Builder->CreateBr(labelBB);
  Builder->SetInsertPoint(labelBB);
  return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value* BreakExprAST::codegen(){
  if (blocksForBreak.empty()){
    return LogErrorV(this->loc, "Break statement not in a a loop");
  }
  Builder->CreateBr(blocksForBreak.top());
  //Builder->SetInsertPoint(blocksForBreak.top());
  //break_found = true;
  return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value* InfiniteLoopCodegen(std::vector<std::unique_ptr<ExprAST>>& Body, Function* TheFunction){
    BasicBlock* loopBB = BasicBlock::Create(*TheContext, "loop_infinite", TheFunction);
    Builder->CreateBr(loopBB);
    Builder->SetInsertPoint(loopBB);
    //BasicBlock* afterBB = BasicBlock::Create(*TheContext, "after_loop_infinite", TheFunction);
    //blocksForBreak.push(afterBB);
    createScope();
    for (int i = 0; i < Body.size(); i++){
      if (!Body.at(i)->codegen())
        return nullptr;
    }
    endScope();
    //blocksForBreak.pop();
    Builder->CreateBr(loopBB);

    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value* LoopExprAST::codegen(){
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  if (is_infinite_loop || Array == nullptr){
    return InfiniteLoopCodegen(Body, TheFunction);
  } else {
    auto double_cpoint_type = Cpoint_Type(double_type, false); // TODO : add a way to select type
    AllocaInst *PosArrayAlloca = CreateEntryBlockAlloca(TheFunction, "pos_loop_in", double_cpoint_type);
    BasicBlock* CmpLoop = BasicBlock::Create(*TheContext, "cmp_loop_in", TheFunction);
    BasicBlock* InLoop = BasicBlock::Create(*TheContext, "body_loop_in", TheFunction);
    BasicBlock* AfterLoop = BasicBlock::Create(*TheContext, "after_loop_in", TheFunction);;
    if (NamedValues[VarName] != nullptr){
      return LogErrorV(this->loc, "variable for loop already exists in the context");
    }
    Value *StartVal = ConstantFP::get(*TheContext, APFloat(0.0));
    Builder->CreateStore(StartVal, PosArrayAlloca);
 
    std::unique_ptr<VariableExprAST> ArrayVar = get_Expr_from_ExprAST<VariableExprAST>(std::move(Array));
    Cpoint_Type cpoint_type = /*NamedValues[ArrayVar->getName()]->type*/ *get_variable_type(ArrayVar->getName());
    Cpoint_Type tempValueArrayType = Cpoint_Type(cpoint_type);
    tempValueArrayType.is_array = false;
    tempValueArrayType.nb_element = 0;
    AllocaInst* TempValueArray = CreateEntryBlockAlloca(TheFunction, VarName, tempValueArrayType);
    NamedValues[VarName] = std::make_unique<NamedValue>(TempValueArray, tempValueArrayType, nullptr, ""); // change values NULL and "" to have structs and classes supported
    Value* SizeArrayVal = ConstantFP::get(*TheContext, APFloat((double)cpoint_type.nb_element));
    Builder->CreateBr(CmpLoop);
    Builder->SetInsertPoint(CmpLoop);
    Value* PosVal = Builder->CreateLoad(PosArrayAlloca->getAllocatedType(), PosArrayAlloca, "load_pos_loop_in");
    Value* isLoopFinishedVal = Builder->CreateFCmpOLT(PosVal, SizeArrayVal,"cmptmp_loop_in");  // if is less than
    Builder->CreateCondBr(isLoopFinishedVal, InLoop, AfterLoop);
    
    Builder->SetInsertPoint(InLoop);
    // initalize here the temp variable to have the value of the array
    Value* PosValInner = Builder->CreateLoad(PosArrayAlloca->getAllocatedType(), PosArrayAlloca, "load_pos_loop_in");
    auto zero = llvm::ConstantInt::get(*TheContext, llvm::APInt(64, 0, true));
    Value* index = Builder->CreateFPToUI(PosValInner, Type::getInt32Ty(*TheContext), "cast_gep_index");
    Value* ptr = Builder->CreateGEP(get_type_llvm(cpoint_type), NamedValues[ArrayVar->getName()]->alloca_inst, { zero, index}, "gep_loop_in", true);
    Value* value = Builder->CreateLoad(get_type_llvm(tempValueArrayType), ptr, VarName);
    Builder->CreateStore(value /*ptr*/, TempValueArray);
    blocksForBreak.push(AfterLoop);
    createScope();
    for (int i = 0; i < Body.size(); i++){
    if (!Body.at(i)->codegen()){
      return nullptr;
    }
    }
    endScope();
    blocksForBreak.pop();
    Value* StepVal = ConstantFP::get(*TheContext, APFloat(1.0));
    Value *CurrentPos = Builder->CreateLoad(PosArrayAlloca->getAllocatedType(), PosArrayAlloca, "current_pos_loop_in");
    Value* NextPos = Builder->CreateFAdd(CurrentPos, StepVal, "nextpos_loop_in");
    Builder->CreateStore(NextPos, PosArrayAlloca);
    Builder->CreateBr(CmpLoop);

    Builder->SetInsertPoint(AfterLoop);

    return Constant::getNullValue(Type::getDoubleTy(*TheContext));
  }
}

Value* WhileExprAST::codegen(){
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  BasicBlock* whileBB = BasicBlock::Create(*TheContext, "while", TheFunction);
  Builder->CreateBr(whileBB);
  Builder->SetInsertPoint(whileBB);
  BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop_while", TheFunction);
  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;
  BasicBlock *AfterBB =
      BasicBlock::Create(*TheContext, "afterloop_while", TheFunction);

  blocksForBreak.push(AfterBB);
  Builder->CreateCondBr(CondV, LoopBB, AfterBB);
  Builder->SetInsertPoint(LoopBB);
  createScope();
  Value* lastVal = Body->codegen();
  if (!lastVal){
    return nullptr;
  }
  endScope();
  blocksForBreak.pop();
  Builder->CreateBr(whileBB);
  Builder->SetInsertPoint(AfterBB);
  // TODO : make while return value work
  /*if (lastVal){
    return lastVal;
  }*/
  
  return Constant::getNullValue(Type::getDoubleTy(*TheContext));
}

Value *ForExprAST::codegen(){
  Function *TheFunction = Builder->GetInsertBlock()->getParent();
  
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName, VarType);
  CpointDebugInfo.emitLocation(this);
  Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;
  if (StartVal->getType() != Alloca->getAllocatedType()){
      convert_to_type(get_cpoint_type_from_llvm(StartVal->getType()), Alloca->getAllocatedType(), StartVal);
  }
  Builder->CreateStore(StartVal, Alloca);
  AllocaInst *OldVal;
  if (NamedValues[VarName] == nullptr){
    OldVal = nullptr;
  } else {
    OldVal = NamedValues[VarName]->alloca_inst;
  }
  NamedValues[VarName] = std::make_unique<NamedValue>(Alloca, VarType);

  BasicBlock* CondBB = BasicBlock::Create(*TheContext, "loop_for_cond", TheFunction);
  Builder->CreateBr(CondBB);
  Builder->SetInsertPoint(CondBB);
  
  Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;
  BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop_for", TheFunction);
  BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop");
  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(LoopBB);
  blocksForBreak.push(AfterBB);
  createScope();
  if (debug_info_mode){
    DIFile *Unit = DBuilder->createFile(CpointDebugInfo.TheCU->getFilename(), CpointDebugInfo.TheCU->getDirectory());
    debugInfoCreateLocalVariable(Scopes.back().debuginfos_scope, Unit, Alloca, VarType, this->getLine());
  }
  Value* lastVal = Body->codegen();
  if (!lastVal){
    return nullptr;
  }
  /*Value* lastVal = nullptr;
  for (int i = 0; i < Body.size(); i++){
    lastVal = Body.at(i)->codegen();
    if (!lastVal)
      return nullptr;
  }*/
  endScope();
  blocksForBreak.pop();
  Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen();
    if (!StepVal)
      return nullptr;
  } else {
    if (VarType.is_decimal_number_type()){
        StepVal = ConstantFP::get(*TheContext, APFloat(1.0));
    } else {
        StepVal = ConstantInt::get(*TheContext, APInt(VarType.get_number_of_bits(), (uint64_t)1));
    }
  }
  if (StepVal->getType() != get_type_llvm(VarType)){
    convert_to_type(get_cpoint_type_from_llvm(StepVal->getType()), get_type_llvm(VarType), StepVal);
  }

  Value *CurVar =
      Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
  //Value *NextVar = Builder->CreateAdd(CurVar, StepVal, "nextvar_for"); // will need to make it into a function to have int add and fadd used depending of type
  Value* NextVar;
  if (VarType.is_decimal_number_type()){
    NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar_for");
  } else {
    NextVar = Builder->CreateAdd(CurVar, StepVal, "nextvar_for");
  }
  Builder->CreateStore(NextVar, Alloca);

  Builder->CreateBr(CondBB);

  // Any new code will be inserted in AfterBB.
  TheFunction->insert(TheFunction->end(), AfterBB);
  Builder->SetInsertPoint(AfterBB);

  // Add a new entry to the PHI node for the backedge.
  //Variable->addIncoming(NextVar, LoopEndBB);

  // Restore the unshadowed variable.
  if (OldVal)
    NamedValues[VarName] = std::make_unique<NamedValue>(OldVal, VarType);
  else
    NamedValues.erase(VarName);

  // is is needed ?
  /*if (lastVal){
    return lastVal;
  }*/ 
  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *UnaryExprAST::codegen() {
  if (Opcode == '-'){
    auto LHS = std::make_unique<NumberExprAST>(0.0);
    auto RHS = std::move(Operand);
    return std::make_unique<BinaryExprAST>(this->loc, "-", std::move(LHS), std::move(RHS))->codegen();
  }
  if (Opcode == '&'){
    return std::make_unique<AddrExprAST>(std::move(Operand))->codegen();
  }
  if (Opcode == '*'){
    return std::make_unique<DerefExprAST>(std::move(Operand))->codegen();
  }
  Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;
  Function *F = getFunction(std::string("unary") + Opcode);
  if (!F){
    Log::Info() << "UnaryExprAST : " << Opcode << "\n";
    return LogErrorV(this->loc, "Unknown unary operator %c", Opcode);
  }

  return Builder->CreateCall(F, OperandV, "unop");
}

Value *VarExprAST::codegen() {
  Log::Info() << "VAR CODEGEN " << VarNames.at(0).first << "\n";
  //std::vector<AllocaInst *> OldBindings; 
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  // Register all variables and emit their initializer.
  for (int i = 0; i < VarNames.size(); i++){
  //for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    
    ExprAST *Init = VarNames[i].second.get();

    // Emit the initializer before adding the variable to scope, this prevents
    // the initializer from referencing the variable itself, and permits stuff
    // like this:
    //  var a = 1 in
    //    var a = a in ...   # refers to outer 'a'.
    Value *InitVal;
    bool no_explicit_init_val = false;
    if (Init) {
      Log::Info() << "Has init" << "\n";
      if (dynamic_cast<EnumCreation*>(Init)){
        Log::Info() << "init val EnumCreation" << "\n";
        AllocaInst* Alloca = CreateEntryBlockAlloca(TheFunction, VarName, cpoint_type);
        NamedValues[VarName] = std::make_unique<NamedValue>(Alloca, cpoint_type);
        Cpoint_Type cpoint_type = NamedValues[VarName]->type;
        Cpoint_Type tag_type = Cpoint_Type(i32_type);
        auto* enumCreation = dynamic_cast<EnumCreation*>(Init);
        
    
        auto zero = llvm::ConstantInt::get(*TheContext, llvm::APInt(64, 0, true));
        auto index_tag = llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 0, true));
        auto index_val = llvm::ConstantInt::get(*TheContext, llvm::APInt(32, 1, true));
        Value* ptr_tag = nullptr;
        Value* value_tag = nullptr;
        auto EnumMembers = std::move(EnumDeclarations[enumCreation->EnumVarName]->EnumDeclar->clone()->EnumMembers);
        std::optional<EnumMember> enumMember = std::nullopt;
        int pos_member = -1;
        
        for (int i = 0; i < EnumMembers.size(); i++){
            //Log::Info() << "EnumMembers.at(i)->contains_custom_index other : " << EnumMembers.at(i)->contains_custom_index << "\n";
            if (EnumMembers.at(i).Name == enumCreation->EnumMemberName){
                //Log::Info() << "EnumMembers.at(i)->contains_custom_index : " << EnumMembers.at(i)->contains_custom_index << "\n";
                if (EnumMembers.at(i).contains_custom_index){
                    /*auto indexCodegened = EnumMembers.at(i)->Index->codegen();
                    if (!dyn_cast<Constant*>(indexCodegened)){
                        return LogErrorV(this->loc, "In enum DeclarASt ")
                    }*/
                    Log::Info() << "EnumMembers.at(i)->Index : " << EnumMembers.at(i).Index << "\n";
                    pos_member = EnumMembers.at(i).Index;
                } else {
                    pos_member = i;
                }
                enumMember = EnumMembers.at(i).clone();
            }
        }
        if (!enumMember.has_value()){
            return LogErrorV(this->loc, "Couldn't find enum member %s", enumCreation->EnumMemberName.c_str());
        }
        if (!EnumDeclarations[enumCreation->EnumVarName]->EnumDeclar->enum_member_contain_type){
            Builder->CreateStore(llvm::ConstantInt::get(*TheContext, llvm::APInt(32, pos_member, true)), Alloca);
            NamedValues[VarName] = std::make_unique<NamedValue>(Alloca, cpoint_type);
            goto after_storing;
        }
        ptr_tag = Builder->CreateGEP(get_type_llvm(cpoint_type), Alloca, {zero, index_tag}, "get_struct", true);
        value_tag = Builder->CreateGEP(get_type_llvm(cpoint_type), Alloca, {zero, index_val}, "get_struct", true);
        Builder->CreateStore(llvm::ConstantInt::get(*TheContext, llvm::APInt(32, pos_member, true)), ptr_tag);
        //NamedValues[VarName] = std::make_unique<NamedValue>(Alloca, cpoint_type);
        if (enumCreation->value){
            //Value* val = enumCreation->value->clone()->codegen();
            Cpoint_Type val_type = enumCreation->value->get_type();
            Value* val = enumCreation->value->codegen();
            if (val->getType() != get_type_llvm(*enumMember->Type)){
                convert_to_type(val_type, get_type_llvm(*enumMember->Type), val);
            }
            Builder->CreateStore(val, value_tag);
            //NamedValues[VarName] = std::make_unique<NamedValue>(Alloca, cpoint_type);
        }
        goto after_storing; 
        // TODO : write code here to initalize the enum without codegen
      }
      if (infer_type){
        cpoint_type = Init->get_type();
      }
      InitVal = Init->codegen();
      if (!InitVal)
        return nullptr;
      // TODO : remove it because it is already done in the ast part
      /*if (infer_type){
        cpoint_type = Cpoint_Type(get_cpoint_type_from_llvm(InitVal->getType()));
      }*/
    } else { // If not specified, use 0.0.
      no_explicit_init_val = true;
      InitVal = get_default_value(cpoint_type);
      //InitVal = get_default_constant(*cpoint_type);
    }
    /*llvm::Value* indexVal = nullptr;
    double indexD = -1;
    if (index != nullptr){
    indexVal = index->codegen();
    // this was removed because it was done in the AST
    int indexD = from_val_to_int(indexVal);
    Log::Info() << "index for varexpr array : " << indexD << "\n";
    cpoint_type.is_array = true;
    cpoint_type.nb_element = indexD;
    }*/
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName, cpoint_type);
    /*if (!get_type_llvm(cpoint_type)->isPointerTy()){
    Log::Warning() << "cpoint_type in var " << VarNames[i].first << " is not ptr" << "\n";
    }*/
    if (!index && InitVal->getType() != get_type_llvm(cpoint_type)){
      convert_to_type(get_cpoint_type_from_llvm(InitVal->getType()), get_type_llvm(cpoint_type), InitVal);
    }
    if (InitVal->getType() == get_type_llvm(Cpoint_Type(void_type))){
       return LogErrorV(this->loc, "Assigning to a variable as default value a void value");
    }
    if (!cpoint_type.is_struct /*TODO : remove this ?*/ || !no_explicit_init_val){
        Builder->CreateStore(InitVal, Alloca);
    }
    if (debug_info_mode){
        DIFile *Unit = DBuilder->createFile(CpointDebugInfo.TheCU->getFilename(), CpointDebugInfo.TheCU->getDirectory());
        debugInfoCreateLocalVariable(/*TheFunction->getSubprogram()*/ Scopes.back().debuginfos_scope , Unit, Alloca, cpoint_type, this->getLine());
    }


    // Remember the old variable binding so that we can restore the binding when
    // we unrecurse.
    /*if (NamedValues[VarName] == nullptr){

      OldBindings.push_back(nullptr);
    } else {
    OldBindings.push_back(NamedValues[VarName]->alloca_inst);
    }*/

    Type* struct_type_temp = get_type_llvm(Cpoint_Type(double_type));
    std::string struct_declaration_name_temp = "";
    std::string struct_name = cpoint_type.struct_name;
    if (cpoint_type.is_struct){
      if (cpoint_type.is_object_template){
        struct_name = get_object_template_name(cpoint_type.struct_name, *cpoint_type.object_template_type_passed);
      }
      if (StructDeclarations[struct_name] == nullptr){
        return LogErrorV(this->loc, "Couldn't find struct declaration %s for created variable", cpoint_type.struct_name.c_str());
      }
      struct_type_temp = StructDeclarations[struct_name]->struct_type;
      struct_declaration_name_temp = struct_name;
    }
    Log::Info() << "VarName " << VarName << "\n";
    //Log::Info() << "struct_declaration_name_temp " << struct_declaration_name_temp << "\n";
    NamedValues[VarName] = std::make_unique<NamedValue>(Alloca, cpoint_type, struct_type_temp, struct_declaration_name_temp);
    //Log::Info() << "NamedValues[VarName]->struct_declaration_name : " <<  NamedValues[VarName]->struct_declaration_name << "\n";
    if (cpoint_type.is_just_struct()){
      //std::cout << struct_name << std::endl;
      if (Function* constructorF = getFunction(struct_name + "__Constructor__Default")){
      std::vector<Value *> ArgsV;
      
      auto A = NamedValues[VarName]->alloca_inst;
      //Value* thisClass = Builder->CreateLoad(PointerType::get(A->getAllocatedType(), A->getAddressSpace()), A, VarName.c_str());
      Value* thisClass = A;
      ArgsV.push_back(thisClass);
      std::string name_constructor_call = "auto_constructor_calltmp";
      if (constructorF->getReturnType()->isVoidTy()){
        name_constructor_call = "";
      }
      Builder->CreateCall(constructorF, ArgsV, name_constructor_call);
      }
    }
  }
after_storing:
  CpointDebugInfo.emitLocation(this);
  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}


// TODO : refactor NamedValues to be scopes in each other instead of a big map

static void register_mod_functions_recursive(std::unique_ptr<ModAST>& mod, std::optional<std::string> parent_mods_prefix){
    for (int i = 0; i < mod->functions.size(); i++){
        std::string function_name = mod->functions.at(i)->Proto->Name;
        function_name = module_mangling(mod->mod_name, function_name);
        if (parent_mods_prefix != std::nullopt){
            //function_name = parent_mods_prefix.value() + function_name;
            function_name = module_mangling(parent_mods_prefix.value(), function_name);
        }
        FunctionProtos[function_name] = mod->functions.at(i)->Proto->clone();
    }
    for (int i = 0; i < mod->function_protos.size(); i++){
        std::string function_name = mod->function_protos.at(i)->Name;
        function_name = module_mangling(mod->mod_name, function_name);
        if (parent_mods_prefix != std::nullopt){
            //function_name = parent_mods_prefix.value() + function_name;
            function_name = module_mangling(parent_mods_prefix.value(), function_name);
        }
        FunctionProtos[function_name] = mod->function_protos.at(i)->clone();
    }

    for (int i = 0; i < mod->mods.size(); i++){
        std::string mod_prefix = mod->mod_name;
        if (parent_mods_prefix.has_value()){
            mod_prefix = module_mangling(parent_mods_prefix.value(), mod->mod_name);
        }
        register_mod_functions_recursive(mod->mods.at(i), std::optional(mod_prefix));
    }
}

static void register_mod_functions(std::unique_ptr<ModAST>& mod){
    register_mod_functions_recursive(mod, std::nullopt);
}

static void register_function_protos(std::unique_ptr<PrototypeAST>& proto){
    FunctionProtos[proto->getName()] = proto->clone();
}

void FileAST::codegen(){
  ZoneScopedN("LLVM codegen");
  // TODO : need to register all function protos from : function definitions, struct members, mods, etc
  // TOOD : detect in function codegen of mod functions if they were already registered and not do it 
  
  for (int i = 0; i < function_protos.size(); i++){
    register_function_protos(function_protos.at(i));
  }

  for (int i = 0; i < mods.size(); i++){
    register_mod_functions(mods.at(i));
  }
  

  for (int i = 0; i < global_vars.size(); i++){
    global_vars.at(i)->codegen();
  }
  /*for (int i = 0; i < function_protos.size(); i++){
    function_protos.at(i)->codegen();
  }*/
  for (int i = 0; i < structs.size(); i++){
    structs.at(i)->codegen();
  }
  for (int i = 0; i < enums.size(); i++){
    enums.at(i)->codegen();
  }
  for (int i = 0; i < unions.size(); i++){
    unions.at(i)->codegen();
  }
  for (int i = 0; i < members.size(); i++){
    members.at(i)->codegen();
  }
  for (int i = 0; i < functions.size(); i++){
    functions.at(i)->codegen();
  }
  for (int i = 0; i < mods.size(); i++){
    mods.at(i)->codegen();
  }
}

void InitializeModule(std::string filename) {
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>(filename, *TheContext);

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

#endif