#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cinttypes>

/**
 * Formats a string
 * NOTE: Not that great, might have to implement this in a better way at some point
 */ 
template<typename ... A>
const char* format(const char* format, A ...args);

using namespace std;

// Forward-Decls //

struct Code;

struct const_data_t;
struct basik_val;
struct basik_var;

struct BasikChar;
struct BasikI16;
struct BasikI32;
struct BasikI64;
struct BasikString;
struct BasikList;
struct BasikFunction;

struct BasikException;
struct Result;

template<typename T> struct Stack;
struct Buffer;

// Simple Data //

enum OpCodes : uint8_t {
    End,
    StoreSimple,
    LoadSimple,
    StoreDynamic,
    LoadDynamic,
    PushString,
    PushChar,
    PushI16,
    PushI32,
    PushI64,
    ListBegin,
    ListEnd,
    ListExpand,
    RemoveDynamic,
    Add,
    Sub,
    Div,
    Mul,
    Pop,
    Dup,
    Jump,
    JumpIf,
    JumpIfNot,
    Return,
    Call,
    PushNull,
    Equals
};

enum DataType : uint16_t {
    String,
    Char,
    I16,
    I32,
    I64,
    List,
    Function,
    Bool
};

enum FunctionKind : uint8_t {
    BuiltinFunction,
    CodeFunction
};

const char* DataTypeStr[] = {
    "String",
    "Char",
    "I16",
    "I32",
    "I64",
    "List",
    "Function",
    "Bool"
};

constexpr inline const char* get_data_type_str(DataType dt);

// Basic Structures //

struct const_data_t {
    uint64_t sz;
    void* data;
};

struct basik_val {
    DataType type;
    void* data;
    ~basik_val();
};

struct basik_var {
    basik_val* data;
    const char* name;
};

// Type Data Structures //

struct BasikChar  {
    uint8_t* data;

    BasikChar(uint8_t data);
    ~BasikChar();
};

struct BasikI16  {
    int16_t* data;

    BasikI16(int16_t data);
    ~BasikI16();
};

struct BasikI32  {
    int32_t* data;

    BasikI32(int32_t data);
    ~BasikI32();
};

struct BasikI64 {
    int64_t* data;

    BasikI64(int64_t data);
    ~BasikI64();
};

struct BasikString {
    uint8_t* data;
    size_t len;

    BasikString(size_t len, const char* data);
    ~BasikString();
};

struct BasikList {
    Stack<basik_val>* data;

    BasikList();
    ~BasikList();

    void append(basik_val* v);

    constexpr inline basik_val* operator[](size_t i);
};

struct BasikFunction {
    Code* code;
    Result(*callback)(Code*,size_t,basik_val**);

    BasikFunction(Code*code);
    BasikFunction(Result(*callback)(Code*,size_t,basik_val**));
    ~BasikFunction();
};

struct BasikBool {
    bool* data;

    BasikBool(bool data);
    ~BasikBool();
};

struct BasikException {
    // The text of the error
    const char* text;
    // The addresses where the error has occured
    size_t* trace;
    // The code objects where the error has occured
    Code**  code_trace;
    // The trace size
    size_t  traci;

    BasikException(const char* text, size_t origin, Code* origin_code) {
        this->text = text;
        this->trace = new size_t[65536];
        this->code_trace = new Code*[65536];
        this->trace[0] = origin;
        this->code_trace[0] = origin_code;
        this->traci = 1;
    }
    ~BasikException() {
        delete[] this->trace;
    }

    BasikException* add_trace(size_t addr, Code* code) {
        this->code_trace[this->traci] = code;
        this->trace[this->traci++]    = addr;
        return this;
    }
};

// Data Structures // 

struct Result {
    BasikException* except;
    basik_val* value;
};

struct Buffer {

    size_t    size;
    uint8_t*  data;
    size_t   _size;
    uint8_t* _data;

    Buffer(size_t size);
    Buffer(size_t size, void* data);

    ~Buffer();

};