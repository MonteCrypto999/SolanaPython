// BPF-compatible Tuple implementation
// Adapted from generated package to work with Solana BPF constraints
// Note: pika_sbf_config.h is force-included via compiler flag (-include)

#include "../../src/PikaObj.h"
#include "../../src/TinyObj.h"
#include "pika_stdlib_compat.h"

// Constructor
PikaObj* New_PikaStdData_Tuple(Args* args) {
    (void)args;
    // Avoid newNormalObj to prevent function pointer issues on BPF
    PikaObj* self = New_TinyObj(NULL);
    self->refcnt = 1;
    obj_setFlag(self, OBJ_FLAG_ALREADY_INIT);
    __vm_List___init__(self);  // Tuple uses List internally
    return self;
}

// __init__()
void PikaStdData_Tuple___init__(PikaObj* self) {
    __vm_List___init__(self);
}

// __getitem__(index)
Arg* PikaStdData_Tuple___getitem__(PikaObj* self, Arg* __key) {
    PikaList* list = obj_getPtr(self, "list");
    int i = arg_getInt(__key);
    Arg* res = pikaList_getArg(list, i);
    if (NULL == res) {
        obj_setErrorCode(self, PIKA_RES_ERR_OUT_OF_RANGE);
        obj_setSysOut(self, "[error] tuple index out of range");
        return NULL;
    }
    return arg_copy(res);
}

// __len__()
Arg* PikaStdData_Tuple___len__(PikaObj* self) {
    PikaList* list = obj_getPtr(self, "list");
    return arg_newInt(pikaList_getSize(list));
}
