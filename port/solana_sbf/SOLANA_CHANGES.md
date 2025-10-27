# Solana SBF Port: Modifications Documentation

This document tracks the specific changes made to PikaPython source files to support the Solana SBF (Sealevel Budgeted Fuel) runtime.

## `src/dataArg.c`

**Purpose:** Unifies argument handling for standard platforms and the constrained Solana BPF environment.

### 1. Argument Limit Workaround (`ArgSetParams`)
*   **Context:** Solana BPF (specifically the older loader or certain toolchain versions) has a strict limit of 5 arguments for function calls.
*   **Change:**
    *   Introduced `ArgSetParams` struct to bundle arguments for `_arg_set_hash`.
    *   Updated `_arg_set_hash`, `arg_create_hash`, and `arg_set` to use this struct when `PIKA_SOLANA_SBF` is defined.

### 2. Software Floating Point Support
*   **Context:** Solana BPF does not support hardware floating-point instructions or standard float ABI.
*   **Change:**
    *   Added `arg_setFloatBits`, `arg_newFloatBits`, and `arg_getFloatBits` under `PIKA_SOLANA_SBF`.
    *   These helper functions treat floats as raw `uint32_t` bit patterns, allowing the `bitfloat_bpf.h` library to handle operations via integer arithmetic.
    *   Modified `arg_getFloat` to return `0.0` (as casted integer 0) or use `bf32_to_i64` for string formatting where appropriate.

### 3. String Formatting (`snprintf`)
*   **Context:** Standard `snprintf` with variadic arguments is problematic or unsupported in the minimal BPF C runtime.
*   **Change:**
    *   Replaced calls to `pika_snprintf` with `_pika_snprintf_impl2` (taking fixed 2 arguments) or `_pika_snprintf_impl1` in `arg_toStrArg`.
    *   Cast pointer addresses and integers to `intptr_t` for formatting to match the implementation expected by the SBF runtime logic.

### 4. Memory Cache Logic
*   **Context:** The standard memory cache logic might use hooks or structures overly complex for the single-threaded, short-lived BPF transaction model.
*   **Change:**
    *   Simplified `_arg_cache_push` and `_arg_cache_pop` under `PIKA_SOLANA_SBF` to remove hook checks (`pika_hook_arg_cache_filter`) and assume simple cache availability.

---

## `src/TinyObj.c`

**Purpose:** Basic object creation.

### 1. Function Pointer Safety
*   **Context:** Solana BPF disallows function pointers that point to addresses outside the text segment or are not properly relocated. Assigning `self->constructor = New_TinyObj` (a function pointer) can trigger "callx outside text segment" or relocation errors.
*   **Change:**
    *   Wrapped the assignment `self->constructor = New_TinyObj;` in `#ifndef PIKA_SOLANA_SBF`.

### 2. Unused Parameter Warning
*   **Change:**
    *   Added `(void)args;` to `New_TinyObj` to suppress compiler warnings.

---

## `src/PikaParser.c`

**Purpose:** Parsing Python source code.

### 1. Function Pointer Removal (Sugar Processors)
*   **Context:** Function pointer arrays (like `Suger_processor_list`) can cause "callx outside text segment" or relocation errors on BPF.
*   **Change:**
    *   Replaced the loop over `Suger_processor_list` in `Parser_sugerProcessOnce` with explicit, direct calls to `Suger_import`, `Suger_semicolon`, and `Suger_multiAssign` when `PIKA_SOLANA_SBF` is defined.

### 2. Argument Count Limits & Casting
*   **Context:** BPF function calls are limited to 5 arguments (r1-r5).
*   **Change:**
    *   Updated `strsFormat` calls to cast arguments to `intptr_t`.

### 3. Stack Usage Optimization (Heap Allocation)
*   **Context:** BPF stack is limited. Large stack allocations can cause verification failure.
*   **Change:**
    *   Used `#ifdef PIKA_SOLANA_SBF` to heap-allocate state structs (`AST_ParseStmtState`, `Parser_Line2AstState`, `Cursor`) in deep recursive functions.
    *   Used `pika_platform_memset` with explicit `sizeof(Args)` instead of `sizeof(buffs)` for safer memory initialization.

---

## `src/PikaCompiler.c`

**Purpose:** Compiler and Linker functions.

### 1. Build Conflict Resolution (`stdio.h`)
*   **Context:** The Solana SBF SDK provides its own `stdio.h` which conflicts with PikaPython's SBF port headers.
*   **Change:**
    *   Guarded the inclusion of `<stdio.h>` with `#ifndef PIKA_SOLANA_SBF`.

### 2. Memory Initialization Safety
*   **Context:** SBF has strict requirements on memory access and stack usage.
*   **Change:**
    *   Replaced `pika_platform_memset` calls with explicit type sizes (e.g., `sizeof(ByteCodeFrame)`) to ensure correct memory initialization.

---

## `src/PikaVM.c`

**Purpose:** PikaPython Virtual Machine implementation.

### 1. SBF-Specific Header Inclusions
*   **Change:**
    *   Added `#include "../port/solana_sbf/bitfloat_bpf.h"`, `#include <sol/return_data.h>` conditionally under `#ifdef PIKA_SOLANA_SBF`.

### 2. Global State & Debug Shell Disablement
*   **Context:** Solana BPF does not allow writable global state.
*   **Change:**
    *   Wrapped declarations of `__pikaMain` and the `__obj_shellLineHandler_debug` function implementation within `#ifndef PIKA_SOLANA_SBF` blocks.

### 3. Argument Passing Workaround (`CallArgContext`)
*   **Context:** BPF's 5-argument limit on function calls.
*   **Change:**
    *   Introduced `CallArgContext` struct for `_load_call_arg`.
    *   Modified `_load_call_arg` to accept and use this context struct when `PIKA_SOLANA_SBF` is defined.

### 4. `print()` Function Short-Circuiting for SBF
*   **Context:** Avoid complex `print` call chains.
*   **Change:**
    *   Added a `#ifdef PIKA_SOLANA_SBF` block in `VM_instruction_handler_RET` to short-circuit the `print` function, using `sol_log()` directly.

### 5. `PikaVMFrame_setSysOut` Argument Fixes
*   **Context:** `PikaVMFrame_setSysOut` was modified to accept fixed arguments.
*   **Change:**
    *   Updated calls to `PikaVMFrame_setSysOut` to cast arguments to `intptr_t` and pad with `0`s where necessary.

---

## `src/dataMemory.c`

**Purpose:** Memory management wrapper.

### 1. Global State Removal
*   **Context:** Global variables (`g_PikaMemInfo`) are not supported in BPF.
*   **Change:**
    *   Guarded `g_PikaMemInfo` and related tracking logic with `#ifndef PIKA_SOLANA_SBF`.

### 2. Direct Allocator Access
*   **Change:**
    *   Modified `pikaMalloc` and `pikaFree` to call `pika_platform_malloc` and `pika_platform_free` directly on SBF, bypassing the tracking logic.

---

## `src/PikaObj.c`

**Purpose:** Object management.

### 1. Function Pointer Guard
*   **Change:**
    *   Guarded `self->constructor = constructor;` in `obj_newObjFromConstructor` with `#ifndef PIKA_SOLANA_SBF` to prevent function pointer relocation issues.