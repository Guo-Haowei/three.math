/*
** $Id: llex.h,v 1.79.1.1 2017/04/19 17:20:42 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"
#include <vector>

#define FIRST_RESERVED 257

#if !defined(LUA_ENV)
#define LUA_ENV "_ENV"
#endif

/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
    /* terminal symbols denoted by reserved words */
    TK_AND = FIRST_RESERVED,
    TK_BREAK,
    TK_DO,
    TK_ELSE,
    TK_ELSEIF,
    TK_END,
    TK_FALSE,
    TK_FOR,
    TK_FUNCTION,
    TK_GOTO,
    TK_IF,
    TK_IN,
    TK_LOCAL,
    TK_NIL,
    TK_NOT,
    TK_OR,
    TK_REPEAT,
    TK_RETURN,
    TK_THEN,
    TK_TRUE,
    TK_UNTIL,
    TK_WHILE,
    /* other terminal symbols */
    TK_IDIV,
    TK_CONCAT,
    TK_DOTS,
    TK_EQ,
    TK_GE,
    TK_LE,
    TK_NE,
    TK_SHL,
    TK_SHR,
    TK_DBCOLON,
    TK_EOS,
    TK_FLT,
    TK_INT,
    TK_NAME,
    TK_STRING
};

/* number of reserved words */
#define NUM_RESERVED (cast(int, TK_WHILE - FIRST_RESERVED + 1))

typedef union {
    lua_Number r;
    lua_Integer i;
    TString* ts;
} SemInfo; /* semantics information */

typedef struct Token {
    int token;
    SemInfo seminfo;
} Token;

/* state of the lexer plus state of the parser when shared by all
   functions */
class LexState {
public:
    int linenumber;       /* input line counter */
    int lastline;         /* line of last token 'consumed' */
    Token t;              /* current token */
    Token lookahead;      /* look ahead token */
    struct FuncState* fs; /* current function (parser) */
    struct lua_State* L;
    Buffer* buff;       /* buffer for tokens */
    Table* h;            /* to avoid collection/reuse strings */
    struct Dyndata* dyd; /* dynamic structures used by the parser */
    TString* source;     /* current source name */
    TString* envn;       /* environment variable name */

    const char* m_text; // original text input
    const char* m_cur;  // cursor of text

    const char* TokenToStr(int token);
    l_noret SyntaxError(const char* s);
// private:
    l_noret LexError(const char* msg, int token);
    int Lex(SemInfo* seminfo);

    // @TODO: rename to consume
    bool Expect(char c);

    void Save(int c);
    void Next();
    void SaveAndNext();
    bool IsCurNewline();
};
// @TODO: make member
LUAI_FUNC void luaX_setinput(lua_State* L, LexState* ls, TString* source, int firstchar, const char* text);

LUAI_FUNC void luaX_init(lua_State* L);
LUAI_FUNC TString* luaX_newstring(LexState* ls, const char* str, size_t l);
LUAI_FUNC void luaX_next(LexState* ls);
LUAI_FUNC int luaX_lookahead(LexState* ls);

#endif
