#include "targets.h"

struct TargetInfo wasm64_unknown_unknown_get_target_infos(){
    return TargetInfo {
        .llvm_target_triple = "",
        .pointer_size = 64,
        .features = "+bulk-memory,+mutable-globals,+sign-ext,+nontrapping-fptoint",
    };
}