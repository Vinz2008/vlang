#include <iostream>
#include <fstream>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <libintl.h>
#include <locale.h>
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "config.h"
#include "lexer.h"
#include "ast.h"
#include "codegen.h"
#include "preprocessor.h"
#include "errors.h"
#include "debuginfo.h"
#include "linker.h"
#include "target-triplet.h"
#include "cli.h"
#include "imports.h"
#include "log.h"
#include "utils.h"
#include "color.h"
#include "gettext.h"

using namespace std;
using namespace llvm;
using namespace llvm::sys;

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

int return_status = 0;

extern std::unique_ptr<DIBuilder> DBuilder;
extern std::vector<std::string> PackagesAdded;
extern bool is_template_parsing_definition;
extern bool is_template_parsing_struct;
struct DebugInfo CpointDebugInfo;
std::unique_ptr<Compiler_context> Comp_context;
// TODO : add those variable in a struct called cpointContext or in Compiler_context
string std_path = DEFAULT_STD_PATH;
string filename ="";
/*bool std_mode = true;
bool gc_mode = true;*/
extern std::string IdentifierStr;
//bool debug_mode = false;
bool debug_info_mode = false;
//bool test_mode = false;
bool silent_mode = false;

//bool is_release_mode = false;

bool errors_found = false;
int error_count = 0;

std::map<std::string, int> BinopPrecedence;
extern std::unique_ptr<Module> TheModule;

std::string first_filename = "";

extern int CurTok;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
//std::unique_ptr<llvm::raw_fd_ostream> file_out_ostream;
llvm::raw_ostream* file_out_ostream;
ifstream file_in;
ofstream file_log;
bool last_line = false;

extern int modifier_for_line_count;

extern std::vector<std::string> modulesNamesContext;

extern Source_location emptyLoc;

void add_manually_extern(std::string fnName, Cpoint_Type cpoint_type, std::vector<std::pair<std::string, Cpoint_Type>> ArgNames, unsigned Kind, unsigned BinaryPrecedence, bool is_variable_number_args, bool has_template, std::string TemplateName){
  // TODO : verify in FunctionProtos if function already exists before redeclaring it
  auto FnAST =  std::make_unique<PrototypeAST>(emptyLoc, fnName, std::move(ArgNames), cpoint_type, Kind != 0, BinaryPrecedence, is_variable_number_args);
  FunctionProtos[fnName] = FnAST->clone();
  Log::Info() << "add extern name " << fnName << "\n";
  FnAST->codegen();
}

void add_externs_for_gc(){
  std::vector<std::pair<std::string, Cpoint_Type>> args_gc_init;
  add_manually_extern("gc_init", Cpoint_Type(void_type), std::move(args_gc_init), 0, 30, false, false, "");
  std::vector<std::pair<std::string, Cpoint_Type>> args_gc_malloc;
  args_gc_malloc.push_back(make_pair("size", Cpoint_Type(int_type)));
  add_manually_extern("gc_malloc", Cpoint_Type(void_type, true), std::move(args_gc_malloc), 0, 30, false, false, "");
  std::vector<std::pair<std::string, Cpoint_Type>> args_gc_realloc;
  args_gc_realloc.push_back(make_pair("ptr", Cpoint_Type(void_type, true)));
  args_gc_realloc.push_back(make_pair("size", Cpoint_Type(int_type)));
  add_manually_extern("gc_realloc", Cpoint_Type(void_type, true), std::move(args_gc_realloc), 0, 30, false, false, "");
}

void add_externs_for_test(){
  std::vector<std::pair<std::string, Cpoint_Type>> args_printf;
  args_printf.push_back(make_pair("format", Cpoint_Type(i8_type, true)));
  add_manually_extern("printf", Cpoint_Type(int_type), std::move(args_printf), 0, 30, true, false, "");
}

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
	if (/*auto *FnIR =*/ FnAST->codegen()) {
    Log::Info() << "Parsed a function definition." << "\n";
    //FnIR->print(*file_out_ostream);
    }
  } else {
    if (!is_template_parsing_definition){
    // Skip token for error recovery.
    getNextToken();
    }
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (/*auto *FnIR =*/ ProtoAST->codegen()) {
      Log::Info() << "Read Extern" << "\n";
      //FnIR->print(*file_out_ostream);
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTypeDef(){
  if (auto typeDefAST = ParseTypeDef()){
    typeDefAST->codegen();
  } else {
    getNextToken();
  }
}

static void HandleMod(){
  if (auto modAST = ParseMod()){
    modAST->codegen();
  }
}

static void HandleGlobalVariable(){
  if (auto GlobalVarAST = ParseGlobalVariable()){
    GlobalVarAST->codegen();
  } else {
    getNextToken();
  }

}

static void HandleStruct() {
  if (auto structAST = ParseStruct()){
    if (/*auto* structIR =*/ structAST->codegen()){
      //ObjIR->print(*file_out_ostream);
    }
  } else {
    if (!is_template_parsing_struct){
        getNextToken();
    }
  }
}

static void HandleUnion(){
  if (auto unionAST = ParseUnion()){
    unionAST->codegen();
  } else {
    getNextToken();
  }
}

static void HandleEnum(){
    if (auto enumAST = ParseEnum()){
        enumAST->codegen();
    } else {
        getNextToken();
    }
}

void HandleComment(){
  Log::Info() << "token bef : " << CurTok << "\n";
  getNextToken(); // pass tok_single_line_comment token
  Log::Info() << "go to next line : " << CurTok << "\n";
  go_to_next_line();
  getNextToken();
  //handlePreprocessor();
  Log::Info() << "token : " << CurTok << "\n";
}

void HandleTest(){
  if (auto testAST = ParseTest()){
      testAST->codegen();
  } else {
    getNextToken();
  }
}

static void MainLoop() {
  while (1) {
    if (Comp_context->debug_mode){
    std::flush(file_log);
    }
    if (last_line == true){
      if (Comp_context->debug_mode){
      file_log << "exit" << "\n";
      }
      return;
    }
    switch (CurTok) {
    case tok_eof:
      last_line = true; 
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_func:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    case tok_struct:
      HandleStruct();
      break;
    case tok_union:
      HandleUnion();
      break;
    case tok_enum:
      HandleEnum();
      break;
    case tok_class:
      //HandleClass();
      HandleStruct();
      break;
    case tok_var:
      HandleGlobalVariable();
      break;
    case tok_typedef:
      HandleTypeDef();
      break;
    case tok_mod:
      HandleMod();
      break;
    case tok_single_line_comment:
      Log::Info() << "found single-line comment" << "\n";
      Log::Info() << "char found as a '/' : " << CurTok << "\n";
      if (Comp_context->debug_mode){
      file_log << "found single-line comment" << "\n";
      }
      HandleComment();
      break;
    default:
      bool is_in_module_context = CurTok == '}' && !modulesNamesContext.empty();
      if (is_in_module_context){
        modulesNamesContext.pop_back();
        getNextToken();
      } else {
      if (CurTok == tok_identifier && IdentifierStr == "test"){
        HandleTest();
        break;
      }
      Log::Info() << "CurTok : " << CurTok << "\n";
      Log::Info() << "identifier : " << IdentifierStr << "\n";
      LogError("TOP LEVEL EXPRESSION FORBIDDEN");
      getNextToken();
      }
      break;
    }
  }
}


int main(int argc, char **argv){
    setlocale(LC_ALL, "");
    bindtextdomain("cpoint", /*"/usr/share/locale/"*/ /*"./locales/"*/ NULL);
    textdomain("cpoint");
    Comp_context = std::make_unique<Compiler_context>("", 1, 0, "<empty line>");
    string object_filename = "out.o";
    string exe_filename = "a.out";
    string temp_output = "";
    bool output_temp_found = false;
    string target_triplet_found;
    bool target_triplet_found_bool = false;
    bool link_files_mode = true;
    bool verbose_std_build = false;
    bool remove_temp_file = true;
    bool import_mode = true;
    bool rebuild_gc = false;
    bool rebuild_std = true;
    bool asm_mode = false;
    bool run_mode = false;
    bool is_optimised = false;
    bool explicit_with_gc = false; // add gc even with -no-std
    bool PICmode = false;
    std::string linker_additional_flags = "";
    for (int i = 1; i < argc; i++){
        string arg = argv[i];
        if (arg.compare("-d") == 0){
            cout << "debug mode" << endl;
            Comp_context->debug_mode = true;
        } else if (arg.compare("-o") == 0){
          i++;
          temp_output = argv[i];
          Log::Info() << "object_filename " << object_filename << "\n";
          output_temp_found = true;
        }  else if (arg.compare("-silent") == 0){
          silent_mode = true;
        } else if (arg.compare("-std") == 0){
          i++;
          std_path = argv[i];
          Log::Print() << _("custom path std : ") << std_path << "\n";
        } else if (arg.compare("-c") == 0){
          link_files_mode = false;
        } else if (arg.compare("-S") == 0){
          asm_mode = true;
        } else if (arg.compare("-O") == 0){
          is_optimised = true;
        } else if (arg.compare("-g") == 0){
          debug_info_mode = true;
        } else if (arg.compare("-nostd") == 0){
          Comp_context->std_mode = false;
        } else if (arg.compare("-fPIC") == 0){
          PICmode = true;
        } else if(arg.compare("-target-triplet") == 0){
          target_triplet_found_bool = true;
          i++;
          target_triplet_found = argv[i];
          cout << "target triplet : " << target_triplet_found << endl;
        } else if (arg.compare("-build-mode") == 0){
            i++;
            if ((std::string)argv[i] == "release"){
                Comp_context->is_release_mode = true;
            } else if ((std::string)argv[i] == "debug"){
                Comp_context->is_release_mode = false;
            } else {
                Log::Warning(emptyLoc) << "Unkown build mode, defaults to debug mode" << "\n";
                Comp_context->is_release_mode = false;
            }
        } else if (arg.compare("-verbose-std-build") == 0){
          verbose_std_build = true;
        } else if (arg.compare("-no-delete-import-file") == 0){
          remove_temp_file = false;
        } else if (arg.compare("-no-gc") == 0){
          Comp_context->gc_mode = false;
        } else if (arg.compare("-with-gc") == 0){
          explicit_with_gc = true;
        } else if (arg.compare("-no-imports") == 0){
          import_mode = false;
        } else if (arg.compare("-rebuild-gc") == 0) {
	        rebuild_gc = true;
        } else if (arg.compare("-no-rebuild-std") == 0){
          rebuild_std = false;
        } else if (arg.compare("-test") == 0){
          Comp_context->test_mode = true;
        } else if (arg.compare("-run") == 0){
          run_mode = true;
        } else if (arg.compare("-run-test") == 0){
          run_mode = true;
          Comp_context->test_mode = true;
        } else if (arg.compare(0, 14,  "-linker-flags=") == 0){
          size_t pos = arg.find('=');
          linker_additional_flags += arg.substr(pos+1, arg.size());
          linker_additional_flags += ' ';
          cout << "linker flag " << linker_additional_flags << endl; 
        } else {
          Log::Print() << _("filename : ") << arg << "\n";
          filename = arg;
        }
    }
    Log::Info() << "filename at end : " << filename << "\n";
    first_filename = filename;
    if (output_temp_found){
      if (link_files_mode){
        exe_filename = temp_output;
      } else {
        object_filename = temp_output;
      }
    }
    if (asm_mode){
      link_files_mode = false;
      if (object_filename == "out.o"){
        object_filename = "out.S";
      }
    }
    init_context_preprocessor();
    Comp_context->filename = filename;
    std::string temp_filename = filename;
    temp_filename.append(".temp");
    if (import_mode){
    int nb_imports = 0;
    nb_imports = generate_file_with_imports(filename, temp_filename);
    Log::Info() << "before remove line count because of import mode " << Comp_context->lexloc.line_nb << " lines" << "\n";
    Log::Info() << "lines count to remove because of import mode " << modifier_for_line_count << "\n";
    Comp_context->lexloc.line_nb -= modifier_for_line_count+nb_imports /*+1*/ /**2*/; // don't know why there are 2 times too much increment so we need to make the modifier double
    Comp_context->curloc.line_nb -= modifier_for_line_count;
    Log::Info() << "after remove line count because of import mode " << Comp_context->lexloc.line_nb << " lines" << "\n";
    filename = temp_filename;
    }
    std::error_code ec;
    if (Comp_context->debug_mode == false){
#ifdef _WIN32
      file_log.open("nul");
#else
      file_log.open("/dev/null");
#endif
    } else {
    file_log.open("cpoint.log");
    }
    file_out_ostream = new raw_fd_ostream(llvm::StringRef("out.ll"), ec);
    file_in.open(filename);
    //file_out_ostream = std::make_unique<llvm::raw_fd_ostream>(llvm::StringRef(filename), &ec);
    //file_out_ostream = raw_fd_ostream(llvm::StringRef(filename), &ec);
    // Install standard binary operators.
    // 1 is lowest precedence.
    BinopPrecedence["="] = 5;

    BinopPrecedence["||"] = 10;
    BinopPrecedence["&&"] = 11;
    BinopPrecedence["|"] = 12;
    BinopPrecedence["^"] = 13;

    BinopPrecedence["!="] = 15;
    BinopPrecedence["=="] = 15;

    BinopPrecedence["<"] = 16;
    BinopPrecedence["<="] = 16;
    BinopPrecedence[">"] = 16;
    BinopPrecedence[">="] = 16;

    BinopPrecedence["<<"] = 20;
    BinopPrecedence[">>"] = 20;

    BinopPrecedence["+"] = 25;
    BinopPrecedence["-"] = 25;

    BinopPrecedence["*"] = 30;
    BinopPrecedence["%"] = 30;
    BinopPrecedence["/"] = 30;

#if ARRAY_MEMBER_OPERATOR_IMPL
    BinopPrecedence["["] = 35;
#endif



    /*BinopPrecedence["^"] = 5;
    BinopPrecedence["|"] = 7;
    BinopPrecedence["<"] = 10;
    BinopPrecedence[">"] = 10;
    BinopPrecedence["="] = 10;
    BinopPrecedence["+"] = 20;
    BinopPrecedence["-"] = 20;
    BinopPrecedence["%"] = 40;
    BinopPrecedence["*"] = 40;*/  // highest.

    string TargetTriple;
    legacy::PassManager pass;
    if (target_triplet_found_bool){
    TargetTriple = target_triplet_found;
    } else {
    TargetTriple = sys::getDefaultTargetTriple();
    }
    std::string os_name = get_os(TargetTriple);

    Log::Info() << "os from target triplet : " << os_name << "\n";
    setup_preprocessor(TargetTriple);
    Log::Info() << "TEST AFTER PREPROCESSOR" << "\n";
    getNextToken();
    InitializeModule(first_filename);
    if (is_optimised){
    pass.add(createInstructionCombiningPass());
    pass.add(createReassociatePass());
    pass.add(createGVNPass());
    pass.add(createCFGSimplificationPass());
    }
    if (debug_info_mode){
    TheModule->addModuleFlag(Module::Warning, "Debug Info Version",
                           DEBUG_METADATA_VERSION);
    if (Triple(sys::getProcessTriple()).isOSDarwin()){
      TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);
    }
    DBuilder = std::make_unique<DIBuilder>((*TheModule));
    CpointDebugInfo.TheCU = DBuilder->createCompileUnit(
      dwarf::DW_LANG_C, DBuilder->createFile(first_filename, "."),
      "Cpoint Compiler", is_optimised, "", 0);
    }
    if ((Comp_context->std_mode || explicit_with_gc) && Comp_context->gc_mode){
      add_externs_for_gc();
    }
    if (Comp_context->test_mode){
      add_externs_for_test();
    }
    MainLoop();
    codegenTemplates();
    //codegenStructTemplates();
    afterAllTests();
    if (debug_info_mode){
    DBuilder->finalize();
    }
    TheModule->print(*file_out_ostream, nullptr);
    file_in.close();
    file_log.close();
    if (errors_found){
      fprintf(stderr, _(RED "exited with %d errors\n" CRESET), error_count);
      exit(1);
    }
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();
    TheModule->setTargetTriple(TargetTriple);
    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
    if (!Target) {
        errs() << Error;
        return 1;
    }
    auto CPU = "generic";
    auto Features = "";
    TargetOptions opt;
    llvm::Optional<llvm::Reloc::Model> RM;
    if (PICmode){
      RM = Optional<Reloc::Model>(Reloc::Model::PIC_);
    } else {
      RM = Optional<Reloc::Model>(Reloc::Model::DynamicNoPIC);
    }
    auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
    TheModule->setDataLayout(TheTargetMachine->createDataLayout());
    std::error_code EC;
    raw_fd_ostream dest(llvm::StringRef(object_filename), EC, sys::fs::OF_None);
    if (EC) {
        errs() << _("Could not open file: ") << EC.message();
        return 1;
    }
    llvm::CodeGenFileType FileType = CGFT_ObjectFile;
    if (asm_mode){
      FileType = CGFT_AssemblyFile;
    }
    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
    errs() << _("TheTargetMachine can't emit a file of this type");
    return 1;
    }
    pass.run(*TheModule);
    dest.flush();
    std::string gc_path = DEFAULT_GC_PATH;
    Log::Print() << _("Wrote ") << object_filename << "\n";
    std::string std_static_path = std_path;
    if (asm_mode){
        goto end_asm_mode;
    }
    if (std_path.back() != '/'){
      std_static_path.append("/");
    }
    std_static_path.append("libstd.a");
    if (Comp_context->std_mode && link_files_mode){
      if (rebuild_std){
      Log::Print() << _("Built the standard library") << "\n";
      if (build_std(std_path, TargetTriple, verbose_std_build) == -1){
        fprintf(stderr, "Could not build std at path : %s\n", std_path.c_str());
        exit(1);
      }
      } else {
        if (!FileExists(std_static_path)){
          fprintf(stderr, "std static library %s has not be builded. You need to at least compile a file one time without the -no-rebuild-std flag\n", std_static_path.c_str());
        }
      }
      if (Comp_context->gc_mode == true){
      gc_path = std_path;
      if (rebuild_gc){
      gc_path.append("/../bdwgc");
      Log::Print() << "Build the garbage collector" << "\n";
      build_gc(gc_path, TargetTriple);
      }
      }
    }
    if (link_files_mode){
      std::vector<string> vect_obj_files;
      if (TargetTriple.find("wasm") == std::string::npos){
      std::string gc_static_path = gc_path;
      if (gc_path.back() != '/'){
      gc_static_path.append("/");
      }
      gc_static_path.append("../bdwgc_prefix/lib/libgc.a");
      /*std::string std_static_path = "-L";
      std_static_path.append(std_path);
      std_static_path.append(" -lstd");*/
      vect_obj_files.push_back(object_filename);
      if (Comp_context->std_mode){
      vect_obj_files.push_back(std_static_path);
      if (Comp_context->gc_mode){
      vect_obj_files.push_back(gc_static_path);
      }
      }
      } else {
        vect_obj_files.push_back(object_filename);
      }
      if (PackagesAdded.empty() == false){
        std::string package_archive_path;
        for (int i = 0; i < PackagesAdded.size(); i++){
          package_archive_path = DEFAULT_PACKAGE_PATH;
          package_archive_path.append("/");
          package_archive_path.append(PackagesAdded.at(i));
          package_archive_path.append("/lib.a");
          vect_obj_files.push_back(package_archive_path);
        }
      }
      Log::Print() << _("Linked the executable") << "\n";
      link_files(vect_obj_files, exe_filename, TargetTriple, linker_additional_flags);
    }
    if (run_mode){
      if (!link_files_mode){
        fprintf(stderr, "Can't run without linking so it is ignored\n");
        exit(1);
      }
      char actualpath [PATH_MAX+1];
      realpath(exe_filename.c_str(), actualpath);
      Log::Print() << "run executable at " << actualpath << "\n";
      runCommand(actualpath);
    }
end_asm_mode:
    if (remove_temp_file /*&& !asm_mode*/){
      remove(temp_filename.c_str());
    }
    return return_status;
}
