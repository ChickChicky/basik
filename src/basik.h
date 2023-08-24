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

struct const_data_t;
struct basik_val;
struct basik_var;

struct BasikChar;
struct BasikI16;
struct BasikI32;
struct BasikI64;
struct BasikString;
struct BasikList;

struct BasikException;
struct Result;

template<typename T> struct Stack;
struct Buffer;

// Simple Data //

enum OpCodes : uint16_t {
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
    JumpIfNot
};

enum DataType : uint16_t {
    String,
    Char,
    I16,
    I32,
    I64,
    List
};

const char* DataTypeStr[] = {
    "String",
    "Char",
    "I16",
    "I32",
    "I64",
    "List",
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

    void append(basik_val v);

    basik_val inline const operator[](size_t i);
};


struct BasikException {
    // The text of the error
    const char* text;
    // The address at which the erorr occured
    size_t addr;
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