# ***Compilatori 2025-2026*** 
***Gueham-Mantolini-Pelusi***
## Multi-Instruction Optimization Pass

This pass detects pairs of consecutive instructions that cancel each other out algebrically, replacing the result of the second instruction with the original operand.

In this case, Multi-Instruction Optimization Pass is used for the following situations:

1. **Add followed by subtract with same constant:** 
    - `a = b + k`, `c = a - k` --> `a = b + k`, `c = b`

2. **Subtract followed by add with same constant:**
    - `a = b - k`, `c = a + k` --> `a = b - k`, `c = b`

### For Example:
    Before the optimization:  
    %1 = add i32 %0, 1
    %2 = sub i32 %3, 1 

    After the optimization:  
    %1 = add i32 %0, 1
    %2 = %0
    

## How to use it
- Create new sub-directory named `build` and enter it  
`mkdir build`  
`cd build`

- Run the following code to configure CMake for this assignment  
`cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../src/`  

- Run `make` to generate `libMultiInstructionOptimizationPass.so`  
`make`

- If any changes are made to the file `MultiInstructionOptimizationPass.cpp`, you must run `make` again

## Optimization Pass Application
- Compile C code (`multi-instruction_optimization.c`) into LLVM IR (human-readable)  
`clang -O0 -Xclang -disable-O0-optnone -S -emit-llvm test/multi-instruction_optimization.c -o test/multi-instruction_optimization.ll`

- Applying the optimization pass plugin to the LLVM IR  
`opt -load-pass-plugin build/libMultiInstructionOptimizationPass.so -passes="mem2reg,mio-pass" -S test/multi-instruction_optimization.ll -o test/multi-instruction_optimization.optimized.ll`

## Documentation   
https://llvm.org/doxygen/index.html
