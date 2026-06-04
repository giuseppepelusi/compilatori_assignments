# ***Compilatori 2025-2026*** 
***Gueham-Mantolini-Pelusi***
## Loop Fusion Optimization Pass

The main idea of Loop Fusion optimization consist of taking two or more adjacent loops that iterate over the same range of values and fusing them into a single loop.

## Fusion Conditions
1. Loops L0 and L1 must be **adjacent**: there cannot be any statements that execute between the end of L0 and the beginning of L1;
2. Loops L0 and L1 must iterate the **same amount** of times;
3. Loops L0 and L1 must be **control flow equivalent**: when L0 executes, L1 also executes, and vice versa;
4. There cannot be any **negative distance dependencies** between Loops L0 and L1: a negative distance dependence occurs between L0 and L1 (L0 before L1) when an iteration m from L1 uses a value taht is computed by L0 at a future iteration m+n (n>0).

## Concept
```c
// Two separate loops (not optimized)
for (int i = 0; i < N; i++) {
    a[i] = b[i] * c[i];
}
for (int i = 0; i < N; i++) {
    d[i] = a[i] + e[i];
}
```
```c
// A single fused loop (optimized)
for (int i = 0; i < N; i++) {
    a[i] = b[i] * c[i];
    d[i] = a[i] + e[i];
}
```

## Key Beneficts
There are several benefits to using Loop Fusion:

- *Better Cache Locality*: Data written to a[i] in the first step is immediately available in the CPU cache to be used in the calculation of d[i], drastically reducing slow RAM accesses.
- *Reduced Overhead*: Halves the branching instructions and counter increments (i++).
- *Potential Parallelism*: Sets the stage for further techniques like vectorization

## How to use it
- Create new sub-directory named `build` and enter it  
`mkdir build`
`cd build`

- Run the following code to configure CMake for this assignment  
`cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../src/`  

- Run `make` to generate `libLoopFusionPass.so`  
`make`

- If any changes are made to the file `LoopFusionPass.cpp`, you must run `make` again

## Optimization Pass Application
- Compile C code (`loop_fusion.c`) into LLVM IR (human-readable)  
`clang -S -O0 -emit-llvm -Xclang -disable-O0-optnone test/loop_fusion.c -o test/loop_fusion.ll`

- Applying the optimization pass plugin to the LLVM IR  
`opt -load-pass-plugin build/libLoopFusionPass.so -passes="mem2reg,loop-simplify,loop-rotate,lf-pass" -S test/loop_fusion.ll -o test/loop_fusion.optimized.ll`

## Documentation   
https://llvm.org/doxygen/index.html
