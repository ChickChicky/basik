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
    ListExpand,
    RemoveDynamic,
    Add,
    Sub,
    Div,
    Mul
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

constexpr inline const char* get_data_type_str(DataType dt) {
    if (dt > sizeof(DataTypeStr)/sizeof(const char*)) return "<INVALID TYPE>";
    return DataTypeStr[dt];
}

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

struct BasikException {
    const char* text;
    int64_t code;
};

struct Result {
    BasikException* except;
    basik_val* value;
};

struct gc_t {

    struct gc_ref {
        void* v;
        size_t c;  
    };

    Stack<gc_ref> refs = Stack<gc_ref>(256,256);

    bool add_ref( void* v ) {
        for (size_t i = 0; i < refs.size; i++) {
            gc_ref* r = refs.data[i];
            if (r != nullptr && refs.data[i]->v == v) {
                r->c++;
                //printf("\t\t\t\tGC STR %p %zu\n",v,r->c);
                return false;
            }
        }
        //printf("\t\t\t\tGC NEW %p\n",v);
        refs.push(new gc_ref{v,1});
        return true;
    }

    bool remove_ref_ex( void* v, size_t* count ) {
        gc_ref* r = nullptr;
        for (size_t i = 0; i < refs.size; i++) {
            if (refs.data[i] != nullptr && refs.data[i]->v == v) {
                r = refs.data[i];
                break;
            }
        }
        if (r == nullptr) return false;
        if (r->c) r->c--;
        //printf("\t\t\t\tGC REM %p %zu\n",r->v,r->c);
        if (count != nullptr) {
            *count = r->c;
        }
        return true;
    }

    inline bool remove_ref( void* v ) {
        return this->remove_ref_ex(v,nullptr);
    }

};

struct Env {

    basik_val** stack;
    size_t stacki;
    Stack<size_t> list_stack;
    basik_var** simple_vars;
    Stack<basik_var> dynamic_vars;
    const_data_t** const_data;
    const char* bytecode;
    uint8_t* ptr;
    gc_t gc;

    Env() {
        this->stack = new basik_val*[65536];
        this->stacki = 0;
    }

    /**
     * Sets a dynamic variable with the provided name to the provided value
     */
    void dynvar_set(const char* name, basik_val* value) {
        gc.add_ref(value);
        basik_var* v = new basik_var{value,name};
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

    /**
     * Finds a dynamic variable with the provided name
     * returns `nullptr` if it wasn't found
     */
    basik_val* dynvar_get(const char* name) {
        for (size_t i = 0; i < dynamic_vars.size; i++) {
            basik_var* v = dynamic_vars.data[i];
            if (v != nullptr && !strcmp(v->name,name)) {
                return v->data;
            }
        }
        return nullptr;
    }

    bool dynvar_rem(const char* name) {
        for (size_t i = 0; i < dynamic_vars.size; i++) {
            basik_var* v = dynamic_vars.data[i];
            if (v != nullptr && !strcmp(v->name,name)) {
                gc.remove_ref(v->data);
                dynamic_vars.data[i] = nullptr;
                return true;
            }
        }
        return false;
    }

    bool stack_push(basik_val* v) {
        gc.add_ref(v);
        if (stacki >= sizeof(stack)) return false;
        stack[stacki++] = v;
        return true;
    }

    inline basik_val* stack_pop() {
        basik_val* v = stack[--stacki];
        gc.remove_ref(v);
        return v;
    }

};

void pre_run(Env* env) {
    const char*      &bytecode = env->bytecode;

    basik_var**      &simple_vars  = env->simple_vars;
    Stack<basik_var> &dynamic_vars = env->dynamic_vars;
    const_data_t**   &const_data   = env->const_data;

    uint8_t* &ptr = env->ptr;
    ptr = (uint8_t*)bytecode;

    // Const Data Processing

    uint32_t const_data_sz = *(uint32_t*)ptr;
    ptr += 4;

    const_data = new const_data_t*[const_data_sz];

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

}

Result run(Env* env) {
    basik_val**  stack  = env->stack;
    size_t      &stacki = env->stacki;

    gc_t &gc = env->gc;

    Stack<size_t>    &list_stack   = env->list_stack;

    const char*      &bytecode     = env->bytecode;
    basik_var**      &simple_vars  = env->simple_vars;
    Stack<basik_var> &dynamic_vars = env->dynamic_vars;
    const_data_t**   &const_data   = env->const_data;

    uint8_t* &ptr = env->ptr;

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
            basik_val* val = env->stack_pop();
            uint32_t var = *(uint32_t*)prog; prog += 4;
            if (val == nullptr) {
                fprintf(stderr,"Got NULL for StoreSimple\n");
                exit(1);
            }
            if (simple_vars[var]->data != nullptr) gc.remove_ref(simple_vars[var]->data);
            gc.add_ref(val);
            simple_vars[var]->data = val;
        } else

        if (op == OpCodes::StoreDynamic) {
            basik_val* val = env->stack_pop();
            if (val == nullptr) {
                fprintf(stderr,"Got NULL for StoreDynamic\n");
                exit(1);
            }
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            printf("STOR DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            env->dynvar_set(varname,val);
        } else

        // Load

        if (op == OpCodes::LoadSimple) {
            uint32_t var = *(uint32_t*)prog; prog += 4;
            basik_val* val = simple_vars[var]->data;
            if (val == nullptr) {
                fprintf(stderr,"Undefined variable `%s`\n",simple_vars[var]->name);
                exit(1);
            }
            env->stack_push(val);
        } else

        if (op == OpCodes::LoadDynamic) {
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* val = env->dynvar_get(varname);
            if (val == nullptr) {
                fprintf(stderr,"Undefined variable `%s`\n",varname);
                exit(1);
            }
            printf("LOAD DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            env->stack_push(val);
        } else

        // Remove

        if (op == OpCodes::RemoveDynamic) { // Cleans up a dynamic variable
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            env->dynvar_rem(varname);
        } else

        // Integers

        if (op == OpCodes::PushChar) {
            uint8_t data = *(uint8_t*)prog;
            env->stack_push(new basik_val{DataType::Char,new BasikChar(data)});
            prog += 1;
        } else
        if (op == OpCodes::PushI16) {
            int16_t data = *(int16_t*)prog;
            env->stack_push(new basik_val{DataType::I16,new BasikI16(data)});
            prog += 2;
        } else
        if (op == OpCodes::PushI32) {
            int32_t data = *(int32_t*)prog;
            env->stack_push(new basik_val{DataType::I32,new BasikI32(data)});
            prog += 4;
        } else
        if (op == OpCodes::PushI64) {
            int64_t data = *(int64_t*)prog;
            env->stack_push(new basik_val{DataType::I64,new BasikI64(data)});
            prog += 8;
        } else

        // String

        if (op == OpCodes::PushString) {
            int32_t val_addr = *(int32_t*)prog;
            const_data_t* data = const_data[val_addr];
            env->stack_push(new basik_val{DataType::String,new BasikString(data->sz,(const char*)data->data)});
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
                list->append(*stack[base+i]);
            }
            for (size_t i = 0; i < list_size; i++) env->stack_pop();
            env->stack_push(new basik_val{DataType::List,list});
        } else
        if (op == OpCodes::ListExpand) {
            basik_val* val = env->stack_pop();
            if (val == nullptr) {
                fprintf(stderr,"Attempt to expand NULL\n");
                exit(1);
            }
            if (val->type == DataType::List) {
                BasikList l = *(BasikList*)val->data;
                for (size_t i = 0; i < l.data->size; i++) {
                    basik_val v = l[l.data->size-i-1];
                    env->stack_push(new basik_val{v.type,v.data});
                }
            } else {
                fprintf(stderr,"Expand does not support type `%s`\n",get_data_type_str(val->type));
                exit(1);
            }
        } else

        // Arithmetic

        if (op == OpCodes::Add) {
            basik_val* a = env->stack_pop();
            basik_val* b = env->stack_pop();
            if (a == nullptr || b == nullptr) {
                fprintf(stderr,"Attempt to add NULL\n");
                exit(1);
            }
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) {
                    fprintf(stderr,"Unsupported '+' betwen Char and %s\n",get_data_type_str(b->type));
                    exit(1);    
                }
                env->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data+*((BasikChar*)b->data)->data)});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) {
                    fprintf(stderr,"Unsupported '+' betwen I16 and %s\n",get_data_type_str(b->type));
                    exit(1);    
                }
                env->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data+*((BasikI16*)b->data)->data)});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) {
                    fprintf(stderr,"Unsupported '+' betwen I32 and %s\n",get_data_type_str(b->type));
                    exit(1);    
                }
                env->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data+*((BasikI32*)b->data)->data)});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) {
                    fprintf(stderr,"Unsupported '+' betwen I64 and %s\n",get_data_type_str(b->type));
                    exit(1);    
                }
                env->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data+*((BasikI64*)b->data)->data)});
            } else {
                fprintf(stderr,"Unsupported data type for '+': `%s`\n",get_data_type_str(a->type));
                exit(1);
            }
        }

        else {
            fprintf(stderr,"Unknown instruction opcode: `%u`\n",op);
            exit(1);
        }
    }

    return Result{nullptr,nullptr};

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
            "\x02\x00\x00\x00" // 1

        "\x08\x00"             // Push I32
            "\x01\x00\x00\x00" // 2
        
        "\x0E\x00"             // Add

        "\x01\x00"             // Store simple
            "\x00\x00\x00\x00" // 0 (x)

        "\x02\x00"
            "\x00\x00\x00\x00"

        "\x03\x00"             // Store Dynamic
            "z\0"              // "z"

        "\x04\x00"             // Load Dynamic
            "z\0"              // "z"

        "\x0D\x00"             // Remove Dynamic
            "z\0"

        "\x04\x00"             // Load Dynamic
            "z\0"              // "z"
        
        "\x00\x00" // End of program
    ;

    Env* env = new Env();
    env->bytecode = (const char*)ptr;

    pre_run(env);
    run(env);

    // printf("`%s` `%u`\n",((BasikString*)simple_vars[0]->data->data)->data,*((BasikI32*)simple_vars[1]->data->data)->data);

    return 0;
}