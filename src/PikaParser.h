/*
 * This file is part of the PikaPython project.
 * http://github.com/pikastech/pikapython
 *
 * MIT License
 *
 * Copyright (c) 2021 lyon liang6516@outlook.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __PIKA_PARSER__H
#define __PIKA_PARSER__H
#include "PikaVM.h"
#include "dataQueueObj.h"
#include "dataStack.h"

typedef QueueObj AST;

typedef enum TokenType {
    TOKEN_strEnd = 0,
    TOKEN_symbol,
    TOKEN_keyword,
    TOKEN_operator,
    TOKEN_devider,
    TOKEN_literal,
} TokenType;

enum StmtType {
    STMT_reference,
    STMT_tuple,
    STMT_string,
    STMT_bytes,
    STMT_number,
    STMT_method,
    STMT_chain,
    STMT_operator,
    STMT_inhert,
    STMT_import,
    STMT_list,
    STMT_slice,
    STMT_dict,
    STMT_none,
};

typedef struct Asmer Asmer;
struct Asmer {
    char* asm_code;
    uint8_t block_deepth_now;
    uint8_t is_new_line;
    char* line_pointer;
};

typedef enum _GenRuleValType {
    VAL_NONEVAL,
    VAL_DYNAMIC,
    VAL_STATIC_,
} GenRuleValType;

typedef struct GenRule {
    char* ins;
    GenRuleValType type;
    char* ast;
    char* val;
} GenRule;

typedef struct BlockState {
    Stack* stack;
    int deepth;
} BlockState;

typedef struct Parser Parser;
typedef char* (*fn_parser_Ast2Target)(Parser* self, AST* ast);
typedef char* (*fn_parser_Lines2Target)(Parser* self, char* sPyLines);

#define _VAL_NEED_INIT -1
#define PIKA_BLOCK_SPACE 4

struct Parser {
    Args lineBuffs;
    Args genBuffs;
    BlockState blockState;
    int blockDeepthOrigin;
    fn_parser_Ast2Target fn_ast2Target;
    pika_bool isGenBytecode;
    ByteCodeFrame* bytecode_frame;
    uint8_t thisBlockDeepth;
    uint32_t label_pc;
};

typedef struct LexToken LexToken;
struct LexToken {
    char* tokenStream;
    enum TokenType type;
    char* pyload;
};

typedef struct Cursor ParsetState;
struct Cursor {
    char* tokenStream;
    uint16_t length;
    uint16_t iter_index;
    int8_t bracket_deepth;
    struct LexToken token1;
    struct LexToken token2;
    Arg* last_token;
    Args* iter_buffs;
    Args* buffs_p;
    PIKA_RES result;
};

char* Lexer_getTokenStream(Args* outBuffs, char* stmt);
char* Lexer_printTokenStream(Args* outBuffs, char* tokenStream);

char* pika_file2Asm(Args* outBuffs, char* filename);
char* pika_lines2Asm(Args* outBuffs, char* multiLine);
char* pika_lines2Array(char* lines);
char* pika_line2Asm(Args* buffs_p, char* line, Stack* blockStack);
AST* parser_line2Ast(Parser* self, char* line);
char* parser_file2Doc(Parser* self, char* sPyFile);
int parser_file2DocFile(Parser* self, char* sPyFile, char* sDocFile);
char* parser_ast2Asm(Parser* self, AST* ast);
char* parser_lines2Doc(Parser* self, char* sPyLines);
char* parser_file2Doc(Parser* self, char* filename);
AST* line2Ast(char* line);

PIKA_RES pika_lines2Bytes(ByteCodeFrame* bf, char* py_lines);
char* parser_line2Target(Parser* self, char* line);

Parser* parser_create(void);
int parser_deinit(Parser* parser);

char* Cursor_popLastToken(Args* outBuffs, char** pStmt, char* str);
char* Cursor_getCleanStmt(Args* outBuffs, char* cmd);
uint8_t Cursor_count(char* stmt, TokenType type, char* pyload);
uint8_t _Cursor_count(char* stmt,
                      TokenType type,
                      char* pyload,
                      pika_bool bSkipbracket);

AST* AST_parseStmt(AST* ast, char* stmt);
AST* AST_create(void);
char* AST_genAsm_top(AST* oAST, Args* outBuffs);
int32_t AST_deinit(AST* ast);

char* instructUnit_fromAsmLine(Args* outBuffs, char* pikaAsm);
ByteCodeFrame* byteCodeFrame_appendFromAsm(ByteCodeFrame* bf, char* pikaAsm);

/* Unified pointer-based macros */
#define _Cursor_forEach(cursor_ptr)  \
    _Cursor_beforeIter(cursor_ptr);  \
    for (int __i = 0; __i < cursor_ptr->length; __i++)

#define Cursor_forEachExistPs(cursor_ptr, stmt) \
    _Cursor_init(cursor_ptr);                   \
    _Cursor_parse(cursor_ptr, stmt);            \
    _Cursor_forEach(cursor_ptr)

#ifdef PIKA_SOLANA_SBF
/* SBF: heap-allocate Cursor to reduce stack usage */
#define Cursor_forEach(cursor_name, stmt)                                         \
    struct Cursor* cursor_name = (struct Cursor*)pika_platform_malloc(sizeof(struct Cursor)); \
    if (cursor_name) { \
        /* Initialize to avoid garbage if init fails */ \
        pika_platform_memset(cursor_name, 0, sizeof(struct Cursor)); \
    } \
    Cursor_forEachExistPs(cursor_name, stmt)

#define Cursor_deinit_heap(cursor_ptr) do { \
    if (cursor_ptr) { \
        Cursor_deinit(cursor_ptr);              \
        pika_platform_free(cursor_ptr);         \
    } \
} while(0)

/* Helper to free early returns */
#define Cursor_free(cursor_ptr) pika_platform_free(cursor_ptr)

#else
/* Non-SBF: stack-allocated struct, exposed as pointer */
#define Cursor_forEach(cursor_name, stmt) \
    struct Cursor _##cursor_name##_stack; \
    struct Cursor* cursor_name = &_##cursor_name##_stack; \
    Cursor_forEachExistPs(cursor_name, stmt)

#define Cursor_deinit_heap(cursor_ptr) Cursor_deinit(cursor_ptr)
#define Cursor_free(cursor_ptr) ((void)0)
#endif

uint16_t TokenStream_getSize(char* tokenStream);
Arg* arg_strAddIndent(Arg* aStrIn, int indent);
Arg* arg_strAddIndentMuti(Arg* aStrIn, int indent);

#endif

#ifdef __cplusplus
}
#endif
