#pragma once
#include <iostream>
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"

enum types {
    double_type = -1,
    int_type = -2,
    float_type = -3,
    void_type = -4,
    i8_type = -5,
    i16_type = -6,
    i32_type = -7,
    i64_type = -8,
    i128_type = -9,
    u8_type = -10,
    u16_type = -11,
    u32_type = -12,
    u64_type = -13,
    u128_type = -14,
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
    bool is_function;
    std::vector<Cpoint_Type> args; // the first is the return type, the other are arguments
    Cpoint_Type* return_type;
    //bool is_class;
    //std::string class_name;
    int nb_ptr;
    bool is_template_type;
    Cpoint_Type(int type, bool is_ptr = false, bool is_array = false, int nb_element = 0, bool is_struct = false, const std::string& struct_name = "", /*bool is_class = false, const std::string& class_name = "",*/ int nb_ptr = 0, bool is_template_type = false, bool is_function = false, std::vector<Cpoint_Type> args = {}, Cpoint_Type* return_type = nullptr) : type(type), is_ptr(is_ptr), is_array(is_array), nb_element(nb_element), is_struct(is_struct), struct_name(struct_name), /*is_class(is_class), class_name(class_name),*/ nb_ptr(nb_ptr), is_template_type(is_template_type), is_function(is_function), args(args), return_type(return_type) {}
};

llvm::Type* get_type_llvm(Cpoint_Type cpoint_type);
llvm::Value* get_default_value(Cpoint_Type type);
llvm::Constant* get_default_constant(Cpoint_Type type);
Cpoint_Type get_cpoint_type_from_llvm(llvm::Type* llvm_type);
bool is_llvm_type_number(llvm::Type* llvm_type);
llvm::Type* get_array_llvm_type(llvm::Type* type, int nb_element);
bool is_decimal_number_type(Cpoint_Type type);
bool is_signed(Cpoint_Type type);
void convert_to_type(Cpoint_Type typeFrom, llvm::Type* typeTo, llvm::Value* &val);
int get_type_number_of_bits(Cpoint_Type type);
std::string get_string_from_type(Cpoint_Type type);