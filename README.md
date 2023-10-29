# C.


A programming language compiler written in C++ which is definitely not finished. It compile the C. code to LLVM IR.

## Advantages

- simple C-like language
- blazingly fast compile times
- predictable name-mangling
- compiler less than 700 KB stripped, ≈20MB not stripped
- classes support
- goto support
- automatic casting

## Cloning the git repo

To clone the git repo, run 

```
git clone --recurse-submodules https://github.com/Vinz2008/C.
```

## WASM support

You can compile C. code to wasm using the ```-target-triplet wasm32-unknown-wasi``` option

## TODO

- [x] Generics support
- [x] Importing structs with function members
- [ ] Finish refactoring the code for operators to make it compatible with every types
- [ ] Make redeclarations just the equal operator
- [ ] Fix array members and array member redeclaration bugs
- [ ] Maybe verify and warn in the manual allocation module in the standard library if an address was already freed by verifying in a list
- [ ] Add type checking in separate file and remove implicit cast and only have explicit casts with the "cast" keyword
- [x] Add rust-styled enums with match
- [x] Add a syntax to import every cpoint files in package
- [ ] Add chained struct members
- [x] Add chained else if
- [ ] Fix bug with type inference and struct pointers (for example in linked_list.cpoint "var tail = self.tail" that we needed to replace with "var tail : struct node_linked_list ptr = self.tail")
- [ ] Add unions support in import
- [ ] Add enum support in import
- [x] Add automatically when calling panic the file and line number to panic call
- [x] Move functions from headers to cpp files to lower compile times
- [x] Work on imports of template structs (for now in single file project you could include them) ( a way to fix problems with linking problems when generating the template would be to namespace the template with the filename)
- [x] Add macro functions (for example with a syntax like #function())
- [ ] Make if, for and while return values like in rust
- [ ] Move preprocessing to import stage
- [x] Deduplicate identical strings when creating them by keeping them in a hashmap when generating them
- [x] Add the string version of the expression in the expect macro
- [ ] Add rust-like "traits" for simple types like i32 or float (It will be called "members" and not traits but it will be the same : add methods to types, but it will need to make the '.' an operator)  
- [x] Add number variable support to match (using it like a switch in c) 
- [x] Add static struct declaration (like with arrays) like in C ((struct struct_example){val1, val2})
- [ ] Use precompiled header for the compiler code to speed up compilation (problem : https://stackoverflow.com/questions/9580058/in-gcc-can-precompiled-headers-be-included-from-other-headers)
- [ ] Remove constructors in the language (to make it rust-like)
- [x] Add members to structs with "members" blocks
- [x] Add recursive install for binaries with subprojects with cpoint-build
- [ ] In cpoint-build, find subdependencies, compile them and link them
- [ ] Warn about/Remove unused functions
- [x] In cpoint-build, find libraries linked to dependencies and link them
- [ ] Add "-compile-time-sizeof" flag to have sizeof not use getelementptr and the compiler insert the values at compile time
- [ ] Have specific flags for optimizations (ex : -optimize-return) or checking (-check-arrays) and global flags (-O2, -C0 for checking level 0) 
- [ ] maybe create a config file to have specific profile for flags (you could have -build-mode fast-no-opti with custom profile that would be defined in the config)
- [ ] Make comments in structs, enums, etc work
- [ ] Make numbers in compiler ints when there is no decimal part
- [x] Add custom numbers for enum without types included (example : variant being 0x200 or 0x100)
- [x] Make va_args work in C. (maybe reimplement printfmt in c and compare the generated assemblies : see it with objdump -S)
- [x] Implement dbg macro
- [ ] Automatically make additional args in variadic functions i32s ? (for now it works without)
- [ ] Add hex notation (example 0x77)

## Benchmarks compared to other languages

[Complete benchmarks](https://github.com/Vinz2008/C./tree/master/docs/benchmarks.md)

Length of benchmarks in seconds, lower is better

![fibonacci benchmark](https://raw.githubusercontent.com/Vinz2008/C./master/docs/tools/fibonacci.png)

![factors benchmark](https://raw.githubusercontent.com/Vinz2008/C./master/docs/tools/factors.png)

## Note about the name

C. is pronounced like C point. It was named like it and not C dot or period but C point because I just named the executable cpoint without thinking about it and then remembered that it is not a transparent translation from french and that it should have been name ccomma, but hey I am too lazy to fix it  ¯\\_(ツ)_/¯

## Locales

The locales are not installed by default. If you want the locales file installed on your system, run make install in the "locales" directory