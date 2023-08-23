#include "basik.h"

/********************************\ 
* Implementations for the header *
\********************************/

/**
 * Retreives the string representation of a data type value
 */
constexpr inline const char* get_data_type_str(DataType dt) {
    if (dt > sizeof(DataTypeStr)/sizeof(const char*)) return "<INVALID TYPE>";
    return DataTypeStr[dt];
}

/**
 * A structure to store data with an access towards the top and with configurable growth
 * NOTE: It will probably be put in the header at some point, but for now I'm a little too lazy because of the template thing
 */
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
        if (this->cap > size) return;
        T** new_data = new T*[size];
        memcpy(new_data,this->data,this->size*sizeof(T*));
        // Destroy all of the ghost data at the top of the stack
        for (size_t i = this->size; i < this->cap; i++) if (this->data[i] != nullptr) delete this->data[i];
        delete[] this->data;
        // printf("STAC %zu %zu\n",this->size,this->cap);
        this->data = new_data;
        this->cap = size;
        // printf("STAK %zu %zu\n",this->size,this->cap);
    }
    void push(T* value) {
        if (this->size >= this->cap-1) this->grow(this->size+this->cap_grow);
        this->data[this->size++] = value;
    }
    void prune() {
        T** tmp_data = new T*[this->size];
        size_t sz = 0;
        for (size_t i = this->size; i < this->cap; i++) if (this->data[i] != nullptr) delete this->data[i];
        for (size_t i = 0; i < this->size; i++)         if (this->data[i] != nullptr) tmp_data[sz++] = this->data[i];
        delete[] this->data;
        this->data = new T*[sz];
        memcpy(this->data,tmp_data,sz*sizeof(T*));
        delete[] tmp_data;
        this->size = sz;
        this->cap = sz;
    }
    T* pop() {
        if (this->size == 0) return nullptr;
        return this->data[--this->size];
    }
    ~Stack() {
        delete[] this->data;
    }
};

// Basic Data Structures //

basik_val::~basik_val() {
         if (this->type == DataType::Char)   delete ((BasikChar*)  this->data);
    else if (this->type == DataType::I16)    delete ((BasikI16*)   this->data);
    else if (this->type == DataType::I32)    delete ((BasikI32*)   this->data);
    else if (this->type == DataType::I64)    delete ((BasikI64*)   this->data);
    else if (this->type == DataType::String) delete ((BasikString*)this->data);
    else if (this->type == DataType::List)   delete ((BasikList*)  this->data);
    else {
        fprintf(stderr,"Unkown data type for free\n");
        exit(1);
    }
}

// Types Data Structures //

// Char
BasikChar::BasikChar(uint8_t data) {
    this->data = new uint8_t(data);
}
BasikChar::~BasikChar() {
    delete this->data;
}

// I16
BasikI16::BasikI16(int16_t data) {
    this->data = new int16_t(data);
}
BasikI16::~BasikI16() {
    delete this->data;
}

// I32
BasikI32::BasikI32(int32_t data) {
    this->data = new int32_t(data);
}
BasikI32::~BasikI32() {
    delete this->data;
}

// I64
BasikI64::BasikI64(int64_t data) {
    this->data = new int64_t(data);
}
BasikI64::~BasikI64() {
    delete this->data;
}

// String
BasikString::BasikString(size_t len, const char* data) {
    this->data = new uint8_t[len];
    this->len = len;
    memcpy(this->data,data,len);
}
BasikString::~BasikString() {
    delete[] this->data;
    this->len = 0;
}

// List
BasikList::BasikList() {
    this->data = new Stack<basik_val>();
}
void BasikList::append(basik_val v) {
    this->data->push(new basik_val{v.type,v.data});
}
basik_val inline const BasikList::operator[](size_t i) {
    return *this->data->data[i];
}
BasikList::~BasikList() {
    delete this->data;
}

// Other Data Structures //

Buffer::Buffer(size_t size) {
    this->data = this->_data = (uint8_t*)malloc(size);
    this->size = this->_size = size;
}

Buffer::Buffer(size_t size, void* data) {
    this->data = this->_data = (uint8_t*)malloc(size);
    this->size = this->_size = size;
    memcpy(this->data,data,size);
}

Buffer::~Buffer() {
    free(this->_data);
}

/********************************\ 
*           Main stuff           *
\********************************/

/**
 * A structure allowing to store references to stuff and delete them when they're no longer used
 */
struct gc_t {

    struct gc_ref {
        basik_val* v;
        size_t c;  
    };

    Stack<gc_ref> refs = Stack<gc_ref>(256,256);

    /**
     * Adds a reference to a pointer, giving back the amount of references to the pointer after the operation
     * Returns whether a new entry was created
     */
    inline bool add_ref_ex( basik_val* v, size_t* count ) {
        for (size_t i = 0; i < refs.size; i++) {
            gc_ref* r = refs.data[i];
            if (r != nullptr && refs.data[i]->v == v) {
                r->c++;
                // printf("\t\t\t\tGC ADD %p %zu\n",v,r->c);
                if (count != nullptr) *count = r->c;
                return false;
            }
        }
        // printf("\t\t\t\tGC NEW %p 1\n",v);
        if (count != nullptr) *count = 1;
        refs.push(new gc_ref{v,1});
        return true;
    }

    /**
     * Adds a reference to a pointer
     */ 
    inline bool add_ref( basik_val* v ) {
        return add_ref_ex(v,nullptr);
    }

    /**
     * Removes a reference to a pointer, giving back the amount of references to the pointer after the operation
     * Returns whether the reference was found
     */
    bool remove_ref( basik_val* v ) {
        for (size_t i = 0; i < refs.size; i++) {
            if (refs.data[i] != nullptr && refs.data[i]->v == v) {
                if(refs.data[i]->c)--refs.data[i]->c;
                // printf("\t\t\t\tGC REM %p %zu\n",refs.data[i]->v,refs.data[i]->c);
                return true;
            }
        }
        return false;
    }

    inline size_t collect( void ) {
        size_t c = 0;
        for (size_t i = 0; i < this->refs.size; i++) {
            gc_ref* r = this->refs.data[i];
            if (r != nullptr && r->c == 0) {
                // printf("\t\t\t\tGC COL %p\n",r->v);
                delete r->v;
                delete r;
                c++;
            }
        }
        this->refs.prune();
        return c;
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
    uint8_t* orig;
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
        for (size_t i = 0; i < dynamic_vars.size; i++) {
            basik_var* vv = dynamic_vars.data[i];
            if (vv != nullptr && !strcmp(vv->name,name)) {
                vv->data = value;
                return;
            }
        }
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

    inline bool stack_push(basik_val* v) {
        if (stacki >= sizeof(stack)) return false;
        gc.add_ref(v);
        stack[stacki++] = v;
        return true;
    }

    inline basik_val* stack_pop() {
        basik_val* v = stack[--stacki];
        gc.remove_ref(v);
        return v;
    }

    bool is_val_true(basik_val* v) {
        if (v == nullptr) return false;
        if (v->type == DataType::Char)   return *((BasikChar*)v->data)->data       != 0;
        if (v->type == DataType::I16)    return *((BasikI16*)v->data)->data        != 0;
        if (v->type == DataType::I32)    return *((BasikI32*)v->data)->data        != 0;
        if (v->type == DataType::I64)    return *((BasikI64*)v->data)->data        != 0;
        if (v->type == DataType::I64)    return *((BasikI64*)v->data)->data        != 0;
        if (v->type == DataType::List)   return  ((BasikList*)v->data)->data->size != 0;
        if (v->type == DataType::String) return  ((BasikString*)v->data)->len      != 0;
        return false;
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

    env->orig = ptr;

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

    while (*(uint16_t*)prog) {
        uint16_t op = *(uint16_t*)prog;
        prog += 2;

        // printf("----- %d %zu -----\n",op,stacki);

        if (op == OpCodes::End) {
            /* do nothing */
            /* this should not even ever happen */
        }

        // Store

        else if (op == OpCodes::StoreSimple) {
            basik_val* val = env->stack_pop();
            uint32_t var = *(uint32_t*)prog; prog += 4;
            if (val == nullptr) {
                fprintf(stderr,"Got NULL for StoreSimple\n");
                exit(1);
            }
            if (simple_vars[var]->data != nullptr) gc.remove_ref(simple_vars[var]->data);
            gc.add_ref(val);
            simple_vars[var]->data = val;
        }

        else if (op == OpCodes::StoreDynamic) {
            basik_val* val = env->stack_pop();
            if (val == nullptr) {
                fprintf(stderr,"Got NULL for StoreDynamic\n");
                exit(1);
            }
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* var = env->dynvar_get(varname);
            if (var != nullptr) {
                // printf("\t\t\t\tR %p\n",var);
                gc.remove_ref(var);
            }
            printf("STOR DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            env->dynvar_set(varname,val);
        }

        // Load

        else if (op == OpCodes::LoadSimple) {
            uint32_t var = *(uint32_t*)prog; prog += 4;
            basik_val* val = simple_vars[var]->data;
            if (val == nullptr) {
                fprintf(stderr,"Undefined variable `%s`\n",simple_vars[var]->name);
                exit(1);
            }
            env->stack_push(val);
        }

        else if (op == OpCodes::LoadDynamic) {
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* val = env->dynvar_get(varname);
            if (val == nullptr) {
                fprintf(stderr,"Undefined variable `%s`\n",varname);
                exit(1);
            }
            printf("LOAD DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            env->stack_push(val);
        }

        // Remove

        else if (op == OpCodes::RemoveDynamic) { // Cleans up a dynamic variable
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            env->dynvar_rem(varname);
        }

        // Integers

        else if (op == OpCodes::PushChar) {
            uint8_t data = *(uint8_t*)prog;
            env->stack_push(new basik_val{DataType::Char,new BasikChar(data)});
            prog += 1;
        }
        else if (op == OpCodes::PushI16) {
            int16_t data = *(int16_t*)prog;
            env->stack_push(new basik_val{DataType::I16,new BasikI16(data)});
            prog += 2;
        }
        else if (op == OpCodes::PushI32) {
            int32_t data = *(int32_t*)prog;
            env->stack_push(new basik_val{DataType::I32,new BasikI32(data)});
            prog += 4;
        }
        else if (op == OpCodes::PushI64) {
            int64_t data = *(int64_t*)prog;
            env->stack_push(new basik_val{DataType::I64,new BasikI64(data)});
            prog += 8;
        }

        // String

        else if (op == OpCodes::PushString) {
            int32_t val_addr = *(int32_t*)prog;
            const_data_t* data = const_data[val_addr];
            env->stack_push(new basik_val{DataType::String,new BasikString(data->sz,(const char*)data->data)});
            prog += 4;
        }

        // List

        else if (op == OpCodes::ListBegin) {
            list_stack.push(new size_t(stacki));
        }
        else if (op == OpCodes::ListEnd) {
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
        }
        else if (op == OpCodes::ListExpand) {
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
        }

        // Arithmetic

        else if (op == OpCodes::Add) {
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

        // Stack

        else if (op == OpCodes::Pop) {
            env->stack_pop();
        }

        else if (op == OpCodes::Dup) {
            basik_val* v = env->stack_pop();
            env->stack_push(v);
            env->stack_push(v);
        }

        // Jumping

        else if (op == OpCodes::Jump) {
            uint64_t addr = *(uint64_t*)prog;
            prog += 8;
            prog = env->orig + addr;
        }

        else if (op == OpCodes::JumpIf) {
            uint64_t addr = *(uint64_t*)prog; prog += 8;
            basik_val* v = env->stack_pop();
            if (env->is_val_true(v)) {
                prog = env->orig + addr;
            }
        }

        // ???

        else {
            fprintf(stderr,"Unknown instruction opcode: `%u`\n",op);
            exit(1);
        }
    
        gc.collect();

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
            "\x00\x00\x00\x00" // 0

        "\x13\x00" // Dup

        "\x15\x00" // Jump If
            "\x20\x00\x00\x00\x00\x00\x00\x00" // 32

        "\x03\x00" // Store Dynamic
            "x\0"  // 'x'

        "\x14\x00" // Jump
            "\x24\x00\x00\x00\x00\x00\x00\x00" // 36

        "\x03\x00"
            "y\0"

        "\x00\x00" // End of program
    ;

    Env* env = new Env();
    env->bytecode = (const char*)ptr;

    pre_run(env);
    run(env);

    // printf("`%s` `%u`\n",((BasikString*)simple_vars[0]->data->data)->data,*((BasikI32*)simple_vars[1]->data->data)->data);

    return 0;
}