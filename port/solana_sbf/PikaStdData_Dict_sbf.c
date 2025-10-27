// BPF-compatible Dict implementation
// Adapted from generated package to work with Solana BPF constraints
// Note: pika_sbf_config.h is force-included via compiler flag (-include)

#include "../../src/PikaObj.h"
#include "../../src/TinyObj.h"
#include "pika_stdlib_compat.h"

// Constructor
PikaObj* New_PikaStdData_Dict(Args* args) {
    (void)args;
    // Avoid newNormalObj to prevent function pointer issues on BPF
    PikaObj* self = New_TinyObj(NULL);
    self->refcnt = 1;
    obj_setFlag(self, OBJ_FLAG_ALREADY_INIT);
    __vm_Dict___init__(self);
    return self;
}

// __init__()
void PikaStdData_Dict___init__(PikaObj* self) {
    __vm_Dict___init__(self);
}

// __setitem__(key, val)
void PikaStdData_Dict___setitem__(PikaObj* self, Arg* arg, char* key) {
    __vm_Dict_set(self, arg, key);
}

// __getitem__(key)
Arg* PikaStdData_Dict___getitem__(PikaObj* self, char* key) {
    // pikaDict_get expects the wrapper object (uses _OBJ2DICT internally)
    Arg* res = pikaDict_get(self, key);
    if (NULL == res) {
        obj_setErrorCode(self, PIKA_RES_ERR_ARG_NO_FOUND);
        obj_setSysOut(self, "[error] key not found");
        return NULL;
    }
    return arg_copy(res);
}

// __len__()
Arg* PikaStdData_Dict___len__(PikaObj* self) {
    // pikaDict_getSize expects the wrapper object (uses _OBJ2DICT internally)
    return arg_newInt(pikaDict_getSize(self));
}

// get(key) - simplified without default parameter
Arg* PikaStdData_Dict_get(PikaObj* self, char* key) {
    // pikaDict_get expects the wrapper object (uses _OBJ2DICT internally)
    Arg* res = pikaDict_get(self, key);
    if (NULL == res) {
        return arg_newNull();
    }
    return arg_copy(res);
}

// __contains__(key)
pika_bool PikaStdData_Dict___contains__(PikaObj* self, char* key) {
    // pikaDict_get expects the wrapper object (uses _OBJ2DICT internally)
    Arg* res = pikaDict_get(self, key);
    return (res != NULL) ? pika_true : pika_false;
}
