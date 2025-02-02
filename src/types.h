#ifndef _TYPES_HEADER_
#define _TYPES_HEADER_

#include <iostream>
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/DerivedTypes.h"

#include "config.h"

enum types {
    double_type = -1,
//    int_type = -2,
    bool_type = -2,
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
//    bool_type = -15,
    other_type  = -15, // includes struct, enum, etc
    never_type = -16,
//    argv_type = -1000,
    empty_type = -1000,
};

class Cpoint_Type;

bool is_type(std::string type);
int get_type(std::string type);
std::string create_pretty_name_for_type(Cpoint_Type type);


class Cpoint_Type {
public:
    int type;
    bool is_ptr;
    int nb_ptr;
    bool is_array;
    int nb_element;
    bool is_struct;
    std::string struct_name;
    bool is_union;
    std::string union_name;
    bool is_enum;
    std::string enum_name;
    bool is_template_type;
    bool is_object_template;
    bool is_empty;
    /*std::string*/ Cpoint_Type* object_template_type_passed;
    bool is_function;
    std::vector<Cpoint_Type> args; // the first is the return type, the other are arguments
    Cpoint_Type* return_type;
    bool is_vector_type;
    Cpoint_Type* vector_element_type;
    int vector_size;
    Cpoint_Type(int type, bool is_ptr = false, int nb_ptr = 0, bool is_array = false, int nb_element = 0, bool is_struct = false, const std::string& struct_name = "", bool is_union = false, const std::string& union_name = "", bool is_enum = false, const std::string& enum_name = "", bool is_template_type = false, bool is_object_template = false, /*const std::string&*/ Cpoint_Type* object_template_type_passed = nullptr, bool is_function = false, std::vector<Cpoint_Type> args = {}, Cpoint_Type* return_type = nullptr, bool is_vector_type = false, Cpoint_Type* vector_element_type = nullptr, int vector_size = 0) 
                : type(type), is_ptr(is_ptr), nb_ptr(nb_ptr), is_array(is_array), nb_element(nb_element), is_struct(is_struct), struct_name(struct_name), is_union(is_union), union_name(union_name), is_enum(is_enum), enum_name(enum_name), is_template_type(is_template_type), is_object_template(is_object_template), object_template_type_passed(object_template_type_passed), is_function(is_function), args(args), return_type(return_type), is_vector_type(is_vector_type), vector_element_type(vector_element_type), vector_size(vector_size) {
                    is_empty = false;
                    if (is_ptr && nb_ptr == 0){
                        nb_ptr = 1;
                    }
                }
    Cpoint_Type() {
        *this = Cpoint_Type(empty_type);
        this->is_empty = true;
    }
    friend std::ostream& operator<<(std::ostream& os, const Cpoint_Type& type){
        os << "{ type : " << type.type << " is_ptr : " << type.is_ptr << " nb_ptr : " << type.nb_ptr  << " is_struct : " << type.is_struct << " is_array : " << type.is_array << " nb_element : " << type.nb_element << " is_template_type : " << type.is_template_type << " is_function : " << type.is_function << " is_vector : " << type.is_vector_type << " }"; 
        return os;
    }
    /*friend bool operator==(const Cpoint_Type& lhs, const Cpoint_Type& rhs){
        // bool is_object_template_type_passed_same = false;
        // if (lhs.object_template_type_passed != nullptr && rhs.object_template_type_passed != nullptr){
        //     is_object_template_type_passed_same = *lhs.object_template_type_passed == *rhs.object_template_type_passed;
        // }
        // bool is_return_type_same = false;
        // if (lhs.return_type != nullptr && rhs.return_type != nullptr){
        //     is_return_type_same = *lhs.return_type == *rhs.return_type;
        // }
        // bool is_vector_element_type_same = false;
        // if (lhs.vector_element_type != nullptr && rhs.vector_element_type != nullptr){
        //     is_vector_element_type_same = *lhs.vector_element_type == *rhs.vector_element_type;
        // }

        bool is_object_template_type_passed_same = true;
        bool is_return_type_same = true;
        bool is_vector_element_type_same = true;

        return lhs.type == rhs.type && lhs.is_ptr == rhs.is_ptr && lhs.nb_ptr == rhs.nb_ptr && lhs.is_array == rhs.is_array && lhs.nb_element == rhs.nb_element && lhs.is_struct == rhs.is_struct && lhs.struct_name == rhs.struct_name && lhs.is_union == rhs.is_union && lhs.union_name == rhs.union_name && lhs.is_enum == rhs.is_enum && lhs.enum_name == rhs.enum_name && lhs.is_template_type == rhs.is_template_type && lhs.is_object_template == rhs.is_object_template && lhs.is_empty == rhs.is_empty && is_object_template_type_passed_same && lhs.is_function == rhs.is_function && std::equal(lhs.args.begin(), lhs.args.end(), rhs.args.begin(), rhs.args.end()) && is_return_type_same && lhs.is_vector_type == rhs.is_vector_type && is_vector_element_type_same && lhs.vector_size == rhs.vector_size;
    }
    friend bool operator!=(const Cpoint_Type& lhs, const Cpoint_Type& rhs){
        return !(lhs == rhs);
    }*/
   std::string to_string(){
        return create_pretty_name_for_type(*this);
   }
    char* c_stringify(){
        std::string str = this->to_string();
        return strdup(str.c_str());
    }
    int get_number_of_bits();
    std::string to_printf_format();
    bool is_just_struct();
    std::string create_mangled_name();
    bool is_decimal_number_type();
    bool is_signed();
    bool is_unsigned();
    bool is_number_type();
    bool is_integer_type();
    // if is a template, remove it
    Cpoint_Type get_real_type();
    Cpoint_Type deref_type();
    Cpoint_Type get_ptr(){
        Cpoint_Type new_type = *this;
        new_type.is_ptr = true;
        new_type.nb_ptr++;
        return new_type;
    }
};

namespace LLVM {
    class Context;
}
llvm::Type* get_type_llvm(std::unique_ptr<LLVM::Context>& llvm_context, Cpoint_Type cpoint_type);
llvm::Type* get_type_llvm(Cpoint_Type cpoint_type); // TODO : move this to a member function
llvm::Value* get_default_value(Cpoint_Type type);
llvm::Constant* get_default_constant(Cpoint_Type type);
llvm::Constant* get_default_constant(llvm::LLVMContext& context, Cpoint_Type type);
Cpoint_Type get_cpoint_type_from_llvm(llvm::Type* llvm_type);
bool is_llvm_type_number(llvm::Type* llvm_type);
bool convert_to_type(Cpoint_Type typeFrom, llvm::Type* typeTo, llvm::Value* &val);
bool convert_to_type(Cpoint_Type typeFrom, Cpoint_Type typeTo, llvm::Value* &val);
bool convert_to_type(std::unique_ptr<LLVM::Context>& llvm_context, Cpoint_Type typeFrom, Cpoint_Type typeTo_cpoint, llvm::Value* &val);


bool operator==(const Cpoint_Type& lhs, const Cpoint_Type& rhs);
bool operator!=(const Cpoint_Type& lhs, const Cpoint_Type& rhs);

#if ENABLE_CIR
class FileCIR;

namespace CIR {
    class InstructionRef;
}

bool cir_convert_to_type(std::unique_ptr<FileCIR>& fileCIR, Cpoint_Type typeFrom, Cpoint_Type typeTo, CIR::InstructionRef& val);
#endif

llvm::Constant* from_val_to_constant_infer(llvm::Value* val);
llvm::Constant* from_val_to_constant(llvm::LLVMContext& context, llvm::Value* val, Cpoint_Type type);
llvm::Constant* from_val_to_constant(llvm::Value* val, Cpoint_Type type);
int from_val_to_int(llvm::Value* val);
//std::string from_cpoint_type_to_printf_format(Cpoint_Type type);
int find_struct_type_size(Cpoint_Type cpoint_type);
//bool is_just_struct(Cpoint_Type type);
int struct_get_number_type(Cpoint_Type cpoint_type, int type);
bool is_struct_all_type(Cpoint_Type cpoint_type, int type);
llvm::VectorType* vector_type_from_struct(Cpoint_Type cpoint_type);
int get_type_size(Cpoint_Type type);
Cpoint_Type get_vector_type(Cpoint_Type vector_element_type, int vector_size);
bool is_just_type(Cpoint_Type type, int type_type);

#endif