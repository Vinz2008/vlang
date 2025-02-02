#ifndef _CIR_INSTRUCTIONS_
#define _CIR_INSTRUCTIONS_

#include "../config.h"

#if ENABLE_CIR

#include "../types.h"

#include "basic_block.h"

//#include "cir.h"

class FileCIR;

namespace CIR {
    // TODO : remove type fields that are duplicates for the type field in the Instruction class (ex : member_type)

    //class BasicBlockRef;
    class BasicBlock;
    class Instruction {
    public:
        std::string label;
        Cpoint_Type type; // TODO : replace with CIR_Type ?
        Instruction(std::string label = "", Cpoint_Type type = Cpoint_Type()) : label(label), type(type) {}
        Instruction(Cpoint_Type type) : label(""), type(type) {}
        virtual ~Instruction() = default;
        virtual std::string to_string() = 0;
    };
    class InstructionRef {
      bool empty_ref;
      int instruction_pos;
    public:
        InstructionRef() : empty_ref(true), instruction_pos(-1) {}
        InstructionRef(int instruction_pos) : empty_ref(false), instruction_pos(instruction_pos) {}
        int get_pos(){
            assert(!empty_ref);
            return instruction_pos;
        }
        bool is_empty(){
            return empty_ref;
        }
        Cpoint_Type get_type(std::unique_ptr<FileCIR>& file);
        std::string to_string(); /*{
            std::string instruction_ref_str = "%" + std::to_string(instruction_pos);
            CIR::Instruction* instr = fileCIR->get_instruction(*this);
            if (instr->label != ""){
                instruction_ref_str += "(" + instr->label + ")";
            }
            if (!instr->type.is_empty){
                instruction_ref_str += " : " + create_pretty_name_for_type(type);
            }
            if (instruction_pos == -1) {
                instruction_ref_str += " (invalid)";
            }
            return instruction_ref_str; 
        }*/
    };
    class CallInstruction : public CIR::Instruction {
    public:
        std::string Callee;
        std::vector<InstructionRef> Args;
        CallInstruction(Cpoint_Type type, std::string Callee, std::vector<InstructionRef> Args) : Instruction(type), Callee(Callee), Args(Args) {}
        std::string to_string() override;
    };
    class ReturnInstruction : public CIR::Instruction {
    public:
        InstructionRef ret_val;
        ReturnInstruction(InstructionRef ret_val) : Instruction(Cpoint_Type(never_type)), ret_val(ret_val) {}
        std::string to_string() override;
    };
    class LoadVarInstruction : public CIR::Instruction {
    public:
        InstructionRef var;
        Cpoint_Type load_type;
        LoadVarInstruction(InstructionRef var, Cpoint_Type load_type) : Instruction(load_type), var(var), load_type(load_type) {}
        std::string to_string() override;
    };
    class DerefInstruction : public CIR::Instruction {
    public:
        InstructionRef ptr;
        Cpoint_Type element_type;
        DerefInstruction(InstructionRef ptr, Cpoint_Type element_type) : Instruction(element_type), ptr(ptr), element_type(element_type) {}
        std::string to_string() override;
    };
    class StoreInPtr : public CIR::Instruction {
    public:
        InstructionRef ptr;
        InstructionRef val_to_store;
        StoreInPtr(InstructionRef ptr, InstructionRef val_to_store) : Instruction(Cpoint_Type(void_type)), ptr(ptr), val_to_store(val_to_store) {}
        std::string to_string() override;
    };

    class LoadGlobalInstruction : public CIR::Instruction {
    public:
        bool is_string;
        int global_pos; // TODO : replace with a GlobalRef type ?
        std::string var_name;
        LoadGlobalInstruction(Cpoint_Type type, bool is_string, int global_pos, std::string var_name) : Instruction(type), is_string(is_string), global_pos(global_pos), var_name(var_name) {}
        std::string to_string() override;
    };

    class StoreGlobalInstruction : public CIR::Instruction {
    public:
        std::string var_name;
        InstructionRef val;
        StoreGlobalInstruction(std::string var_name, InstructionRef val) : Instruction(Cpoint_Type(void_type)), var_name(var_name), val(val) {}
        std::string to_string() override;
    };


    class InitArgInstruction : public CIR::Instruction {
    public:
        std::string arg_name;
        Cpoint_Type arg_type;
        InitArgInstruction(std::string arg_name, Cpoint_Type arg_type) : Instruction(arg_type), arg_name(arg_name), arg_type(arg_type) {}
        std::string to_string() override;
    };
    class StoreVarInstruction : public CIR::Instruction {
    public:
        InstructionRef var;
        InstructionRef val;
        StoreVarInstruction(InstructionRef var, InstructionRef val) : Instruction(Cpoint_Type(void_type)) /*TODO ?*/, var(var), val(val) {}
        std::string to_string() override;
    };
    class VarInit : public CIR::Instruction {
    public:
        // TODO : add multiple vars support
        std::pair<std::string, InstructionRef> VarName;
        Cpoint_Type var_type;
        VarInit(std::pair<std::string, InstructionRef> VarName, Cpoint_Type var_type) : Instruction(var_type), VarName(VarName), var_type(var_type) {}
        std::string to_string() override;
    };
    class CastInstruction : public CIR::Instruction {
    public:
        InstructionRef val; // TODO : add to Instructions the types of instructions to have the from type of the cast
        Cpoint_Type from_type;
        Cpoint_Type cast_type;
        CastInstruction(InstructionRef val, Cpoint_Type from_type, Cpoint_Type cast_type) : Instruction(cast_type), val(val), from_type(from_type), cast_type(cast_type) {}
        std::string to_string() override;
    };
    
    class GotoInstruction : public CIR::Instruction {
    public:
        CIR::BasicBlock* goto_bb;
        GotoInstruction(CIR::BasicBlock* goto_bb) : Instruction(Cpoint_Type(never_type)), goto_bb(goto_bb) {}
        std::string to_string() override;
    };

    class GotoIfInstruction : public CIR::Instruction {
    public:
        CIR::InstructionRef cond_instruction;
        CIR::BasicBlock* goto_bb_if_true;
        CIR::BasicBlock* goto_bb_if_false;
        GotoIfInstruction(CIR::InstructionRef cond_instruction, CIR::BasicBlock* goto_bb_if_true, CIR::BasicBlock* goto_bb_if_false) : Instruction(Cpoint_Type(never_type)), cond_instruction(cond_instruction), goto_bb_if_true(goto_bb_if_true), goto_bb_if_false(goto_bb_if_false) {}
        std::string to_string() override;
    };

    class SizeofInstruction : public CIR::Instruction {
    public:
        bool is_type;
        Cpoint_Type type;
        InstructionRef expr;
        SizeofInstruction(bool is_type, Cpoint_Type type, InstructionRef expr) : Instruction(Cpoint_Type(i32_type)), is_type(is_type), type(type), expr(expr) {
            if (!expr.is_empty()){
                assert(type.is_empty);
            }
            if (!type.is_empty){
                assert(expr.is_empty());
            }
        }
        std::string to_string() override;
    };

    class ArgInlineAsm {
    public:
        InstructionRef ArgExpr;
        enum ArgType {
            output,
            input
        } argType;
        ArgInlineAsm(InstructionRef ArgExpr, enum ArgType argType) : ArgExpr(ArgExpr), argType(argType) {}
    };
    class InlineAsmInstruction : public CIR::Instruction {
    public:
        std::string asm_code;
        std::vector<CIR::ArgInlineAsm> InputOutputArgs;
        InlineAsmInstruction(std::string asm_code, std::vector<CIR::ArgInlineAsm> InputOutputArgs) : asm_code(asm_code), InputOutputArgs(InputOutputArgs) {}
        std::string to_string() override;
    };

    class AddInstruction : public CIR::Instruction {
    public:
        InstructionRef arg1;
        InstructionRef arg2;
        AddInstruction(Cpoint_Type type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class SubInstruction : public CIR::Instruction {
    public:
        InstructionRef arg1;
        InstructionRef arg2;
        SubInstruction(Cpoint_Type type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class MulInstruction : public CIR::Instruction {
    public:
        InstructionRef arg1;
        InstructionRef arg2;
        MulInstruction(Cpoint_Type type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class RemInstruction : public CIR::Instruction {
    public:
        InstructionRef arg1;
        InstructionRef arg2;
        RemInstruction(Cpoint_Type type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class DivInstruction : public CIR::Instruction {
    public:
        InstructionRef arg1;
        InstructionRef arg2;
        DivInstruction(Cpoint_Type type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class AndInstruction : public CIR::Instruction {
    public:
        InstructionRef arg1;
        InstructionRef arg2;
        AndInstruction(Cpoint_Type type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class OrInstruction : public CIR::Instruction {
    public:
        InstructionRef arg1;
        InstructionRef arg2;
        OrInstruction(Cpoint_Type type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class ShiftInstruction : public CIR::Instruction {
    public:
        enum shift_type_ty {
            SHIFT_LEFT,
            SHIFT_RIGHT,
        } shift_type;
        InstructionRef arg1;
        InstructionRef arg2;
        ShiftInstruction(Cpoint_Type type, enum shift_type_ty shift_type, InstructionRef arg1, InstructionRef arg2) : Instruction(type), shift_type(shift_type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    class PhiInstruction : public CIR::Instruction {
    public:
        Cpoint_Type phi_type;
        BasicBlock* bb1;
        InstructionRef arg1;
        BasicBlock* bb2;
        InstructionRef arg2;
        PhiInstruction(Cpoint_Type phi_type, BasicBlock* bb1, InstructionRef arg1, BasicBlock* bb2, InstructionRef arg2) : Instruction(phi_type), phi_type(phi_type), bb1(bb1), arg1(arg1), bb2(bb2), arg2(arg2) {}
        std::string to_string() override;
    };

    class CmpInstruction : public CIR::Instruction {
    public:
        enum cmp_type_ty {
            CMP_EQ,
            CMP_NOT_EQ,
            CMP_GREATER,
            CMP_GREATER_EQ,
            CMP_LOWER,
            CMP_LOWER_EQ,
        } cmp_type;
        InstructionRef arg1;
        InstructionRef arg2;
        CmpInstruction(enum cmp_type_ty cmp_type, InstructionRef arg1, InstructionRef arg2) : Instruction(Cpoint_Type(bool_type)), cmp_type(cmp_type), arg1(arg1), arg2(arg2) {}
        std::string to_string() override;
    };

    /*// instructions for array members and structs
    class AccessMemoryInstruction : public CIR::Instruction  {
    public:
        AccessMemoryInstruction(){}
        AccessMemoryInstruction(Cpoint_Type type) : CIR::Instruction("", type) {}
    };
    // TODO : maybe refactor this code to have GetArrayElement that load the ptr and GetArrayElementPtr that returns the ptr ?
    class getArrayElement : public CIR::AccessMemoryInstruction {
    public:
        bool is_array_access_mem;
        union array_ty {
            CIR::InstructionRef val;
            std::unique_ptr<AccessMemoryInstruction> accessMemInstruction;
            array_ty(CIR::InstructionRef val) : val(val) {}
            array_ty(std::unique_ptr<AccessMemoryInstruction> accessMemInstruction) : accessMemInstruction(std::move(accessMemInstruction)) {}
            ~array_ty() {}
        } array;
        CIR::InstructionRef index;
        Cpoint_Type array_type;
        Cpoint_Type element_type;
        getArrayElement(CIR::InstructionRef val, CIR::InstructionRef index, Cpoint_Type array_type, Cpoint_Type element_type) : AccessMemoryInstruction(element_type), is_array_access_mem(false), array(val), index(index), array_type(array_type), element_type(element_type) {}
        getArrayElement(std::unique_ptr<AccessMemoryInstruction> accessMemInstruction, CIR::InstructionRef index, Cpoint_Type array_type, Cpoint_Type element_type, bool is_ptr) : AccessMemoryInstruction(element_type), is_array_access_mem(true), array(std::move(accessMemInstruction)), index(index), array_type(array_type), element_type(element_type) {}
        std::string to_string() override;
    };

    class getStructMember : public CIR::AccessMemoryInstruction {
    public:
        bool is_struct_access_mem;
        union struct_ty {
            CIR::InstructionRef val;
            std::unique_ptr<AccessMemoryInstruction> accessMemInstruction;
            struct_ty(CIR::InstructionRef val) : val(val) {}
            struct_ty(std::unique_ptr<AccessMemoryInstruction> accessMemInstruction) : accessMemInstruction(std::move(accessMemInstruction)) {}
            ~struct_ty() {}
        } structVal;
        std::string member;
        Cpoint_Type struct_type;
        Cpoint_Type element_type;
        getStructMember(CIR::InstructionRef val, std::string member, Cpoint_Type struct_type, Cpoint_Type element_type) : AccessMemoryInstruction(element_type), is_struct_access_mem(false), structVal(val), member(member), struct_type(struct_type), element_type(element_type) {}
        getStructMember(std::unique_ptr<AccessMemoryInstruction> accessMemInstruction, std::string member, Cpoint_Type struct_type, Cpoint_Type element_type, bool is_ptr) : AccessMemoryInstruction(element_type), is_struct_access_mem(true), structVal(std::move(accessMemInstruction)), member(member), struct_type(struct_type), element_type(element_type) {}
        std::string to_string() override;
    };*/

    class GepArray : public CIR::Instruction {
    public:
        CIR::InstructionRef array;
        CIR::InstructionRef index;
        Cpoint_Type array_type;
        Cpoint_Type element_type;
        GepArray(CIR::InstructionRef array, CIR::InstructionRef index, Cpoint_Type array_type, Cpoint_Type element_type) : Instruction(array_type.deref_type().get_ptr()), array(array), index(index), array_type(array_type), element_type(element_type) {}
        std::string to_string() override;
    };

    class GepStruct : public CIR::Instruction {
    public:
        CIR::InstructionRef struct_ref;
        std::string member_name;
        Cpoint_Type struct_type;
        Cpoint_Type member_type;
        // TODO : add a call to the Instruction Constructor with the type of the member
        GepStruct(CIR::InstructionRef struct_ref, std::string member_name, Cpoint_Type struct_type, Cpoint_Type member_type) : Instruction(member_type.get_ptr()), struct_ref(struct_ref), member_name(member_name), struct_type(struct_type), member_type(member_type) {}
        std::string to_string() override;
    };

    // TODO : add gep ptr to help add optimizations specifically for indexing with arrays ?



    class ConstNumber;
    class ConstVoid;
    class ConstNever;
    class ConstNull;
    class ConstInstruction : public CIR::Instruction {
    public:
        ConstInstruction(){}
        ConstInstruction(Cpoint_Type type) : CIR::Instruction("", type) {}
        friend bool operator==(const ConstInstruction& const1, const ConstInstruction& const2); /*{
            if (dynamic_cast<const ConstNumber*>(&const1) && dynamic_cast<const ConstNumber*>(&const2)){
                auto const1nb = dynamic_cast<const ConstNumber*>(&const1);
                auto const2nb = dynamic_cast<const ConstNumber*>(&const2);
                bool is_enum_val_same = false;
                if (const1nb->is_float == const2nb->is_float){
                    if (const1nb->is_float){
                        is_enum_val_same = const1nb->nb_val.float_nb == const2nb->nb_val.float_nb;
                    } else {
                        is_enum_val_same = const1nb->nb_val.int_nb == const2nb->nb_val.int_nb;
                    }
                }
                return is_enum_val_same && const1nb->type == const1nb->type;
            }
            if (dynamic_cast<const ConstVoid*>(&const1) && dynamic_cast<const ConstVoid*>(&const2)){
                return true;
            }
            if (dynamic_cast<const ConstNever*>(&const1) && dynamic_cast<const ConstNever*>(&const2)){
                return true;
            }
            if (dynamic_cast<const ConstNull*>(&const1) && dynamic_cast<const ConstNull*>(&const2)){
                return true;
            }
            return false;
        }*/
    };
    

    class ConstNumber : public CIR::ConstInstruction {
    public:
        bool is_float;
        union nb_val_ty {
            double float_nb;
            int int_nb;
        } nb_val;
        ConstNumber(Cpoint_Type type, bool is_float, union nb_val_ty nb_val) : ConstInstruction(type), is_float(is_float), nb_val(nb_val) {}
        std::string to_string() override;
    };
    class ConstBool : public CIR::ConstInstruction {
    public:
        bool val;
        ConstBool(bool val) : ConstInstruction(Cpoint_Type(bool_type)), val(val){}
        std::string to_string() override;
    };
    class ConstVector : public CIR::ConstInstruction {
    public:
        Cpoint_Type vector_element_type;
        std::vector<CIR::InstructionRef> VectorMembers; // all of these should be consts, TODO ? : enforce it at the type level (with a CIR::ConstInstructionRef ?)
        ConstVector(Cpoint_Type vector_element_type, std::vector<CIR::InstructionRef> VectorMembers) : ConstInstruction(get_vector_type(vector_element_type, VectorMembers.size())), vector_element_type(vector_element_type), VectorMembers(VectorMembers)  {}
        std::string to_string() override;
    };
    class ConstVoid : public CIR::ConstInstruction {
    public:
        ConstVoid() : ConstInstruction(Cpoint_Type(void_type)) {}
        std::string to_string() override {
            return "void";
        }
    };
    class ConstNever : public CIR::ConstInstruction {
    public:
        ConstNever() : ConstInstruction(Cpoint_Type(never_type)) {}
        std::string to_string() override {
            return "!";
        }
    };
    class ConstNull : public CIR::ConstInstruction {
    public:
        ConstNull() : ConstInstruction(Cpoint_Type(void_type, true)) {} // TODO, add a null type ? (like nullptr_t in cpp)
        std::string to_string() override {
            return "null";
        }
    };

    class ConstStruct : public CIR::ConstInstruction {

    };
}

#endif

#endif