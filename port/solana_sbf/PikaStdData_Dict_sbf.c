// BPF-compatible Dict implementation
// Adapted from generated package to work with Solana BPF constraints
// Note: pika_sbf_config.h is force-included via compiler flag (-include)

#include "../../src/PikaObj.h"
#include "../../src/TinyObj.h"
#include "pika_stdlib_compat.h"

// Forward declarations for method wrappers
static void _Dict___len__Method(PikaObj* self, Args* args);
static void _Dict___getitem__Method(PikaObj* self, Args* args);
static void _Dict___setitem__Method(PikaObj* self, Args* args);
static void _Dict___contains__Method(PikaObj* self, Args* args);
static void _Dict_getMethod(PikaObj* self, Args* args);

// Constructor
PikaObj* New_PikaStdData_Dict(Args* args) {
    (void)args;
    // Avoid newNormalObj to prevent function pointer issues on BPF
    PikaObj* self = New_TinyObj(NULL);
    self->refcnt = 1;
    obj_setFlag(self, OBJ_FLAG_ALREADY_INIT);
    __vm_Dict___init__(self);

    // Register methods so len(), d[key], etc. work
    class_defineMethod(self, "__len__()", "", (Method)_Dict___len__Method);
    class_defineMethod(self, "__getitem__(key)", "key", (Method)_Dict___getitem__Method);
    class_defineMethod(self, "__setitem__(key,val)", "key,val", (Method)_Dict___setitem__Method);
    class_defineMethod(self, "__contains__(key)", "key", (Method)_Dict___contains__Method);
    class_defineMethod(self, "get(key)", "key", (Method)_Dict_getMethod);

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
    // Count items by manually iterating through _keys linked list
    Args* keys = obj_getPtr(self, "_keys");
    if (keys == NULL) {
        return arg_newInt(0);
    }

    // Manual linked list count (same as args_getSize but inline for debugging)
    int count = 0;
    Arg* node = (Arg*)keys->firstNode;
    while (node != NULL) {
        count++;
        node = arg_getNext(node);
    }
    return arg_newInt(count);
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

// Method wrappers for class_defineMethod registration
static void _Dict___len__Method(PikaObj* self, Args* args) {
    Arg* res = PikaStdData_Dict___len__(self);
    method_returnArg(args, res);
}

static void _Dict___getitem__Method(PikaObj* self, Args* args) {
    char* key = args_getStr(args, "key");
    Arg* res = PikaStdData_Dict___getitem__(self, key);
    if (res) {
        method_returnArg(args, res);
    }
}

static void _Dict___setitem__Method(PikaObj* self, Args* args) {
    char* key = args_getStr(args, "key");
    Arg* val = args_getArg(args, "val");
    PikaStdData_Dict___setitem__(self, val, key);
}

static void _Dict___contains__Method(PikaObj* self, Args* args) {
    char* key = args_getStr(args, "key");
    pika_bool res = PikaStdData_Dict___contains__(self, key);
    method_returnInt(args, res);
}

static void _Dict_getMethod(PikaObj* self, Args* args) {
    char* key = args_getStr(args, "key");
    Arg* res = PikaStdData_Dict_get(self, key);
    method_returnArg(args, res);
}
