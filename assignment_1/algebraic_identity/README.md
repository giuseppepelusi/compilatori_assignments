# ***Compilatori 2025-2026*** 
***Gueham-Mantolini-Pelusi***
## Algebraic Identity Optimization Pass

This pass uses algebraic properties to simplify expressions and code, removing useless operations for subsequent optimizations.

In this case, Algebraic Identity Optimization Pass is used for the following situations:

1. **Addition with zero:** 
    - `x + 0` or `0 + x` --> `x`

2. **Multiplication by one:**
    - `x * 1` or `1 * x` --> `x`

### For Example:
    Before the optimization:  
    %1 = add i32 %0, 0 
    %2 = mul i32 %3, 1 

    After the optimization:  
    %1 = %0  
    %2 = %3

## How to use it
- Create new sub-directory named `build` and enter it  
`mkdir build`  
`cd build`

- Run the following code to configure CMake for this assignment  
`cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../src/`  

- Run `make` to generate `libAlgebraicIdentityPass.so`  
`make`

- If any changes are made to the file `AlgebraicIdentityPass.cpp`, you must run `make` again

## Optimization Pass Application
- Compile C code (`algebraic_identity.c`) into LLVM IR (human-readable)  
`clang -O0 -Xclang -disable-O0-optnone -S -emit-llvm test/algebraic_identity.c -o test/algebraic_identity.ll`

- Applying the optimization pass plugin to the LLVM IR  
`opt -load-pass-plugin build/libAlgebraicIdentityPass.so -passes="mem2reg,ai-pass" -S test/algebraic_identity.ll -o test/algebraic_identity.optimized.ll`

## Documentation   
https://llvm.org/doxygen/index.html
