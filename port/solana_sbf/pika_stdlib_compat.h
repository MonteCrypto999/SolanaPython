// Compatibility layer for stdlib implementations on Solana BPF
// Provides VM helper functions and API bridges
#pragma once

#include "../../src/PikaObj.h"

// VM helper functions that generated stdlib expects
// These initialize and manipulate List/Dict objects

static inline void __vm_List___init__(PikaObj* self) {
    // Create internal list structure directly to avoid circular dependency
    // Don't use New_pikaList() which calls New_PikaStdData_List recursively!
    Args* list = New_args(NULL);

    // Verify list pointer
    if (!list) {
        return;
    }

    Arg* top_arg = arg_newInt(0);
    if (!top_arg) {
        return;
    }

    PIKA_RES res = args_pushArg_name(list, "top", top_arg);

    // Verify we can read it back immediately
    int64_t test_top = args_getInt(list, "top");

    obj_setPtr(self, "list", list);
}

static inline void __vm_List_append(PikaObj* self, Arg* arg) {
    // pikaList_append expects the wrapper object, not the internal Args!
    // It will use _OBJ2LIST macro to extract the Args itself
    pikaList_append(self, arg);
}

static inline void __vm_Dict___init__(PikaObj* self) {
    /* Create just the Args storage, not full PikaDict objects
     * This avoids infinite recursion since New_PikaDict calls __vm_Dict___init__ */
    Args* dict = New_args(NULL);
    Args* keys = New_args(NULL);
    obj_setPtr(self, "dict", dict);
    obj_setPtr(self, "_keys", keys);
}

static inline void __vm_Dict_set(PikaObj* self, Arg* arg, char* key) {
    Args* dict = obj_getPtr(self, "dict");
    Args* keys = obj_getPtr(self, "_keys");

    Arg* arg_key = arg_setStr(NULL, key, key);
    Arg* arg_new = arg_copy(arg);
    arg_setName(arg_new, key);

    // Use args_setArg directly since dict/keys are Args*, not PikaObj*
    args_setArg(dict, arg_new);
    args_setArg(keys, arg_key);
}

// API compatibility functions
// Bridge between generated package API and our API

static inline PIKA_RES pikaList_setArg(PikaList* self, int index, Arg* arg) {
    // Generated package uses pikaList_setArg, we have pikaList_set
    return pikaList_set(self, index, arg);
}
