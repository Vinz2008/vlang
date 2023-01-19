#include "types.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"

using namespace llvm;

extern std::unique_ptr<LLVMContext> TheContext;

Type* get_type_llvm(int t, bool is_ptr){
    switch (t){
        case double_type:
            if (is_ptr){
            return Type::getDoublePtrTy(*TheContext);
            } else {
            return Type::getDoubleTy(*TheContext);
            }
        case int_type:
            if (is_ptr){
            return Type::getInt32PtrTy(*TheContext);
            } else {
            return Type::getInt32Ty(*TheContext);
            }
        case float_type:
            if (is_ptr){
            return Type::getFloatPtrTy(*TheContext);
            } else {
            return Type::getFloatTy(*TheContext);
            }
        case i8_type:
            if (is_ptr){
	        return Type::getInt8PtrTy(*TheContext);
	        } else {
	        return Type::getInt8Ty(*TheContext);
	        }
        case void_type:
            return Type::getVoidTy(*TheContext);
        case argv_type:
            return Type::getInt8PtrTy(*TheContext)->getPointerTo();
    }
}

std::vector<std::string> types{
    "double",
    "int",
    "float",
    "i8",
    "void",
};

bool is_type(std::string type){
    for (int i = 0; i < types.size(); i++){
       if (type.compare(types.at(i)) == 0){
	    return true;
       }
    }
    return false;
}

int get_type(std::string type){
    for (int i = 0; i < types.size(); i++){
       if (type.compare(types.at(i)) == 0){
        return -(i + 1);
       }
    }
    return 1;
}
