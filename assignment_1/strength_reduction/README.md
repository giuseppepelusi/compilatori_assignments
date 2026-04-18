# ***Compilatori 2025-2026*** 
***Gueham-Mantolini-Pelusi***
## Strength Reduction Optimization Pass

This pass replace multi-cicles operations (expensive), like multiplication or division, with other simpler operations, like shift.

In this case, Strength Reduction Optimization Pass is used for the following situations:

1. **Shift for Multiplication:** 
    - `y = x * 15` --> `(x << 4) - x`

2. **Shift for Division:**
    - `y = x / 8` --> `x >> 3`

### For Example:
    Before the optimization:  
    %1 = mul i32 %0, 15  
    %2 = sdiv i32 %3, 8

    After the optimization:  
    %1 = shl i32 %0, 4
    %2 = sub i32 %1, %0
    %4 = ashr i32 %3, 3

## How to use it
- Create new sub-directory named `build` and enter it  
`mkdir build`  
`cd build`

- Run the following code to configure CMake for this assignment  
`cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../src/`  

- Run `make` to generate `libStrengthReductionPass.so`  
`make`

- If any changes are made to the file `StrengthReductionPass.cpp`, you must run `make` again

## Optimization Pass Application
- Compile C code (`strength_reduction.c`) into LLVM IR (human-readable)  
`clang -O0 -Xclang -disable-O0-optnone -S -emit-llvm test/strength_reduction.c -o test/strength_reduction.ll`

- Applying the optimization pass plugin to the LLVM IR  
`opt -load-pass-plugin build/libStrengthReductionPass.so -passes="mem2reg,sr-pass" -S test/strength_reduction.ll -o test/strength_reduction.optimized.ll`

## Documentation   
https://llvm.org/doxygen/index.html
