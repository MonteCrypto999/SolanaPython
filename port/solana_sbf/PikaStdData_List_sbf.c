// BPF-compatible List implementation
// Adapted from generated package to work with Solana BPF constraints
// Note: pika_sbf_config.h is force-included via compiler flag (-include)

#include "../../src/PikaObj.h"
#include "../../src/TinyObj.h"
#include "pika_stdlib_compat.h"

// Constructor
// Note: The VM calls pikaList_init() after constructing, so we don't init here
PikaObj* New_PikaStdData_List(Args* args) {
    (void)args;
    PikaObj* self = New_TinyObj(NULL);
    self->refcnt = 1;
    obj_setFlag(self, OBJ_FLAG_ALREADY_INIT);
    // Don't call __vm_List___init__ here - the VM will call pikaList_init()
    return self;
}

// append(item)
void PikaStdData_List_append(PikaObj* self, Arg* arg) {
    __vm_List_append(self, arg);
}

// __setitem__(index, item)
void PikaStdData_List___setitem__(PikaObj* self, Arg* __key, Arg* arg) {
    int i = arg_getInt(__key);
    // pikaList_set expects the wrapper object (uses _OBJ2LIST internally)
    if (PIKA_RES_OK != pikaList_set(self, i, arg)) {
        obj_setErrorCode(self, PIKA_RES_ERR_OUT_OF_RANGE);
        obj_setSysOut(self, "[error] list index out of range");
    }
}

// __init__()
void PikaStdData_List___init__(PikaObj* self) {
    __vm_List___init__(self);
}

// __getitem__(index)
Arg* PikaStdData_List___getitem__(PikaObj* self, Arg* __key) {
    int i = arg_getInt(__key);
    // pikaList_getArg expects the wrapper object (uses _OBJ2LIST internally)
    Arg* res = pikaList_getArg(self, i);
    if (NULL == res) {
        obj_setErrorCode(self, PIKA_RES_ERR_OUT_OF_RANGE);
        obj_setSysOut(self, "[error] list index out of range");
        return NULL;
    }
    return arg_copy(res);
}

// __len__()
Arg* PikaStdData_List___len__(PikaObj* self) {
    // pikaList_getSize expects the wrapper object (uses _OBJ2LIST internally)
    return arg_newInt(pikaList_getSize(self));
}

// pop() - simplified version without optional index parameter
Arg* PikaStdData_List_pop(PikaObj* self) {
    // pikaList_pop expects the wrapper object (uses _OBJ2LIST internally)
    return pikaList_pop(self);
}
