#pragma once
#include <iostream>
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

enum types {
    double_type = -1,
    int_type = -2,
    float_type = -3,
    i8_type = -4,
    void_type = -5,
    argv_type = -1000,
};

bool is_type(std::string type);
int get_type(std::string type);
class Cpoint_Type {
public:
    int type;
    bool is_ptr;
    bool is_array;
    int nb_element;
    bool is_struct;
    std::string struct_name;
    bool is_class;
    std::string class_name;
    int nb_ptr;
    Cpoint_Type(int type, bool is_ptr = false, bool is_array = false, int nb_element = 0, bool is_struct = false, const std::string& struct_name = "", int nb_ptr = 0) : type(type), is_ptr(is_ptr), is_array(is_array), nb_element(nb_element), is_struct(is_struct), struct_name(struct_name), nb_ptr(nb_ptr) {}
};

llvm::Type* get_type_llvm(Cpoint_Type cpoint_type);
llvm::Value* get_default_value(Cpoint_Type type);
llvm::Constant* get_default_constant(Cpoint_Type type);
Cpoint_Type* get_cpoint_type_from_llvm(llvm::Type* llvm_type);