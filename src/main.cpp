#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cinttypes>

using namespace std;

struct Buffer {

    size_t    size;
    uint8_t*  data;
    size_t   _size;
    uint8_t* _data;

    Buffer(size_t size) {
        this->data = this->_data = (uint8_t*)malloc(size);
        this->size = this->_size = size;
    }

    Buffer(size_t size, void* data) {
        this->data = this->_data = (uint8_t*)malloc(size);
        this->size = this->_size = size;
        memcpy(this->data,data,size);
    }

    ~Buffer() {
        free(this->_data);
    }

};

template<typename T>
struct Stack {
    T** data;
    size_t cap_grow;
    size_t cap;
    size_t size;
    Stack() {
        this->cap = 20;
        this->cap_grow = 20;
        data = new T*[20];
        this->size = 0;
    }
    Stack(size_t size) {
        this->cap = size;
        this->cap_grow = 20;
        data = new T*[size];
        this->size = 0;
    }
    Stack(size_t size, size_t cap_grow) {
        this->cap = size;
        this->cap_grow = cap_grow;
        this->data = new T*[size];
        this->size = 0;
    }
    void grow(size_t size) {
        if (this->cap <= size) return;
        T** new_data = new T*[size];
        memcpy(new_data,this->data,this->size);
        // Destroy all of the ghost data
        for (size_t i = this->size; i < this->cap; i++) if (this->data[i] != nullptr) delete this->data[i];
        this->data = new_data;
        this->cap = size;
    }
    void push(T* value) {
        if (this->size == this->cap-1) this->grow(this->size+this->cap_grow);
        this->data[this->size++] = value;
    }
    T* pop() {
        if (this->size == 0) return nullptr;
        return this->data[--this->size];
    }
    ~Stack() {
        delete[] this->data;
    }
};

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
    ListExpand
};

enum DataType : uint16_t {
    String,
    Char,
    I16,
    I32,
    I64,
    List
};

struct const_data_t {
    uint64_t sz;
    void* data;
};

struct basik_val {
    DataType type;
    void* data;
};

struct basik_var {
    basik_val* data;
    const char* name;
};

struct BasikString {
    uint8_t* data;
    size_t len;
    BasikString(size_t len, const char* data) {
        this->data = new uint8_t[len];
        this->len = len;
        memcpy(this->data,data,len);
    }
    ~BasikString() {
        delete[] this->data;
        this->len = 0;
    }
};

struct BasikChar  {
    uint8_t* data;
    BasikChar(uint8_t data) {
        this->data = new uint8_t(data);
    }
    ~BasikChar() {
        delete this->data;
    }
};

struct BasikI16  {
    uint16_t* data;
    BasikI16(uint16_t data) {
        this->data = new uint16_t(data);
    }
    ~BasikI16() {
        delete this->data;
    }
};

struct BasikI32  {
    uint32_t* data;
    BasikI32(uint32_t data) {
        this->data = new uint32_t(data);
    }
    ~BasikI32() {
        delete this->data;
    }
};

struct BasikI64 {
    uint64_t* data;
    BasikI64(uint64_t data) {
        this->data = new uint64_t(data);
    }
    ~BasikI64() {
        delete this->data;
    }
};

struct BasikList {
    Stack<basik_val>* data;
    BasikList() {
        this->data = new Stack<basik_val>();
    }
    void append(basik_val v) {
        this->data->push(new basik_val{v.type,v.data});
    }
    basik_val inline const operator[](size_t i) {
        return *this->data->data[i];
    }
    ~BasikList() {
        delete this->data;
    }
};

basik_val stack[65536];
size_t stacki = 0;

Stack<size_t> list_stack;

basik_var** simple_vars;
Stack<basik_var> dynamic_vars;

bool stack_push(basik_val v) {
    if (stacki >= sizeof(stack)) return false;
    stack[stacki++] = v;
    return true;
}

basik_val stack_pop() {
    return stack[--stacki];
}

/**
 * Finds a dynamic variable with the provided name
 * returns `nullptr` if it wasn't found
 */
basik_val* dynvar_get(const char* name) {
    for (size_t i = 0; i < dynamic_vars.size; i++) {
        basik_var* v = dynamic_vars.data[i];
        if (v != nullptr && !strcmp(v->name,name)) return v->data;
    }
    return nullptr;
}

/**
 * Sets a dynamic variable with the provided name to the provided value
 */
void dynvar_set(const char* name, basik_val value) {
    basik_var* v = new basik_var{new basik_val{value.type,value.data},name};
    // Tries to find an empty space in the variables
    for (size_t i = 0; i < dynamic_vars.size; i++) {
        if (dynamic_vars.data[i] == nullptr) {
            dynamic_vars.data[i] = v;
            return;
        }
    }
    // Adds a new one if no space was found
    dynamic_vars.push(v);
}

int main() {

    uint8_t* ptr = (uint8_t*)
        "\x01\x00\x00\x00"     // Const data pool
            "\x06\x00\x00\x00" //
            "HELLO\x00"        //

        "\x02\x00\x00\x00"     // Variable data pool
            "x\0"              // x
            "y\0"              // y
        
        "\x08\x00"             // Push I32
            "\x42\x00\x00\x00" // 66

        "\x03\x00"             // Store Dynamic
            "z\0"              // "z"

        "\x04\x00"             // Load Dynamic
            "z\0"              // "z"
        
        "\x00\x00" // End of program
    ;

    // Const Data Processing

    uint32_t const_data_sz = *(uint32_t*)ptr;
    ptr += 4;

    const_data_t** const_data = new const_data_t*[const_data_sz];

    for (uint32_t i = 0; i < const_data_sz; i++) {
        uint32_t sz = *(uint32_t*)ptr;
        ptr += 4;
        const_data[i] = new const_data_t{sz,ptr};
        ptr += sz;
    }

    // Variable Data Processing

    uint32_t simple_variable_data_sz = *(uint32_t*)ptr;
    ptr += 4;

    simple_vars = new basik_var*[simple_variable_data_sz];

    for (uint32_t i = 0; i < simple_variable_data_sz; i++) {
        size_t l = strlen((const char*)ptr);
        simple_vars[i] = new basik_var{nullptr,(const char*)ptr};
        ptr += l+1;
    }

    // Running

    uint8_t* prog = ptr;
    uint8_t* orig = ptr;

    while (*(uint16_t*)prog) {
        uint16_t op = *(uint16_t*)prog;
        prog += 2;

        if (op == OpCodes::End) {
            /* do nothing */
            /* this should not even ever happen */
        } else

        // Store

        if (op == OpCodes::StoreSimple) {
            basik_val val = stack_pop();
            uint32_t var = *(uint32_t*)prog; prog += 4;
            if (val.data == nullptr) {
                fprintf(stderr,"Got NULL for StoreSimple\n");
                exit(1);
            }
            simple_vars[var]->data = new basik_val{val.type,val.data};
        } else

        if (op == OpCodes::StoreDynamic) {
            basik_val val = stack_pop();
            if (val.data == nullptr) {
                fprintf(stderr,"Got NULL for StoreDynamic\n");
                exit(1);
            }
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            printf("STOR DYN %s = %d\n",varname,*((BasikI32*)val.data)->data);
            dynvar_set(varname,val);
        } else

        // Load

        if (op == OpCodes::LoadSimple) {
            uint32_t var = *(uint32_t*)prog; prog += 4;
            basik_val* val = simple_vars[var]->data;
            if (val == nullptr) {
                fprintf(stderr,"Undefined variable `%s`\n",simple_vars[var]->name);
                exit(1);
            }
            stack_push(basik_val{val->type,val->data});
        } else

        if (op == OpCodes::LoadDynamic) {
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* val = dynvar_get(varname);
            if (val == nullptr) {
                fprintf(stderr,"Undefined variable `%s`\n",varname);
                exit(1);
            }
            printf("LOAD DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            stack_push(basik_val{val->type,val->data});
        } else

        // Integers

        if (op == OpCodes::PushChar) {
            uint8_t data = *(uint8_t*)prog;
            stack_push(basik_val{DataType::Char,new BasikChar(data)});
            prog += 1;
        } else
        if (op == OpCodes::PushI16) {
            int16_t data = *(int16_t*)prog;
            stack_push(basik_val{DataType::I16,new BasikI16(data)});
            prog += 2;
        } else
        if (op == OpCodes::PushI32) {
            int32_t data = *(int32_t*)prog;
            stack_push(basik_val{DataType::I32,new BasikI32(data)});
            prog += 4;
        } else
        if (op == OpCodes::PushI64) {
            int64_t data = *(int64_t*)prog;
            stack_push(basik_val{DataType::I64,new BasikI64(data)});
            prog += 8;
        } else

        // String

        if (op == OpCodes::PushString) {
            int32_t val_addr = *(int32_t*)prog;
            const_data_t* data = const_data[val_addr];
            stack_push(basik_val{DataType::String,new BasikString(data->sz,(const char*)data->data)});
            prog += 4;
        } else

        // List

        if (op == OpCodes::ListBegin) {
            list_stack.push(new size_t(stacki));
        } else
        if (op == OpCodes::ListEnd) {
            if (list_stack.size == 0) {
                fprintf(stderr,"Attempt to close a list that was not open\n");
                exit(1);
            }
            size_t base = *list_stack.pop();
            size_t list_size = stacki-base;
            BasikList* list = new BasikList();
            for (size_t i = 0; i < list_size; i++) {
                list->append(stack[base+i]);
            }
            for (size_t i = 0; i < list_size; i++) stack_pop();
            stack_push(basik_val{DataType::List,list});
        } else
        if (op == OpCodes::ListExpand) {
            basik_val val = stack_pop();
            if (val.type == DataType::List) {
                BasikList l = *(BasikList*)val.data;
                for (size_t i = 0; i < l.data->size; i++) {
                    basik_val v = l[l.data->size-i-1];
                    stack_push(basik_val{v.type,v.data});
                }
            } else {
                fprintf(stderr,"Unsupported data type for list expand\n");
                exit(1);
            }
        }

        else {
            fprintf(stderr,"Unknown instruction opcode: `%u`\n",op);
            exit(1);
        }
    }

    // printf("`%s` `%u`\n",((BasikString*)simple_vars[0]->data->data)->data,*((BasikI32*)simple_vars[1]->data->data)->data);

    return 0;
}