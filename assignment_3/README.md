# ***Compilatori 2025-2026*** 
***Gueham-Mantolini-Pelusi***
## Loop-Invariant Code Motion Pass

The main idea of Loop-Invariant Code Motion (LICM) is to move instructions whose values do not change during the execution of a loop outside, before the loop begins (into the preheader).

### Code Motion Eligibility

An instruction is a candidate for relocation if it meets the following criteria:

* **It is loop-invariant:** An instruction is loop-invariant if all of its operands are defined outside the loop, or if they are defined by other instructions that have already been identified as loop-invariant.
* **It satisfies the correctness conditions:**
	* **a.** The block containing the instruction dominates all loop exits.
	* **b.** The variable being assigned is not assigned by any other instruction inside the loop.
	* **c.** The instruction dominates all uses of the assigned variable, or the variable is "dead" at the loop exit.

---

### The LICM Algorithm

1. **Reaching Definitions:** Step 1.
Compute the reaching definitions for the entire control flow graph (CFG) of the loop.

2. **Loop-Invariant Identification:** Step 2.
Iteratively find all instructions within the loop that qualify as loop-invariant based on their operands.

3. **Dominator Tree:** Step 3.
Compute the dominators and construct the dominator tree for the relevant basic blocks.

4. **Loop Exits:** Step 4.
Identify all the exit blocks of the loop.

5. **Correctness Verification:** Step 5.
Verify the code motion safety requirements for each loop-invariant candidate:

	* It is loop-invariant.
	* It resides in a basic block that dominates all loop exits.
	* It defines a variable that is not assigned anywhere else in the loop.
	* It dominates all uses of that defined variable (or the variable is dead at loop exit).

6. **Depth-First Search (DFS):** Step 6.
Perform a Depth-First Search traversal of the basic blocks inside the loop to preserve valid dependency order.

7. **Code Hoisting:** Step 7.
Move the instructions that successfully satisfy all requirements out of the loop and into the preheader.

## How to use it
- Create new sub-directory named `build` and enter it  
`mkdir build`  
`cd build`

- Run the following code to configure CMake for this assignment  
`cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../src/`  

- Run `make` to generate `libLoopICMPass.so`  
`make`

- If any changes are made to the file `LoopICMPass.cpp`, you must run `make` again

## Optimization Pass Application
- Compile C code (`licm.c`) into LLVM IR (human-readable)  
`clang -S -O0 -emit-llvm -Xclang -disable-O0-optnone test/licm.c -o test/licm.ll`

- Applying the optimization pass plugin to the LLVM IR  
`opt -load-pass-plugin build/libLoopICMPass.so -passes="mem2reg,loop-icm-pass" -S test/licm.ll -o test/licm.optimized.ll`

## Documentation   
https://llvm.org/doxygen/index.html
