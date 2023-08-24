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

struct Code {

    basik_val** stack;
    size_t stacki;
    Stack<size_t> list_stack;
    basik_var** simple_vars;
    Stack<basik_var> dynamic_vars;
    const_data_t** const_data;
    const char* bytecode;
    uint8_t* ptr;
    uint8_t* orig;
    uint8_t* prog;
    gc_t* gc;

    Code( gc_t* gc, const char* bytecode ) {
        this->stack = new basik_val*[65536];
        this->stacki = 0;
        this->gc = gc;
        this->bytecode = bytecode;
    }

    /**
     * Sets a dynamic variable with the provided name to the provided value
     */
    void dynvar_set(const char* name, basik_val* value) {
        gc->add_ref(value);
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
                gc->remove_ref(v->data);
                dynamic_vars.data[i] = nullptr;
                return true;
            }
        }
        return false;
    }

    inline bool stack_push(basik_val* v) {
        if (stacki >= sizeof(stack)) return false;
        gc->add_ref(v);
        stack[stacki++] = v;
        return true;
    }

    inline basik_val* stack_pop() {
        basik_val* v = stack[--stacki];
        gc->remove_ref(v);
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

void pre_run(Code* code) {
    const char*      &bytecode = code->bytecode;

    basik_var**      &simple_vars  = code->simple_vars;
    Stack<basik_var> &dynamic_vars = code->dynamic_vars;
    const_data_t**   &const_data   = code->const_data;

    uint8_t* &ptr = code->ptr;
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

    code->orig = ptr;
    code->prog = ptr;

}

Result run(Code* code) {
    basik_val**  stack  = code->stack;
    size_t      &stacki = code->stacki;

    gc_t* &gc = code->gc;

    Stack<size_t>    &list_stack   = code->list_stack;

    const char*      &bytecode     = code->bytecode;
    basik_var**      &simple_vars  = code->simple_vars;
    Stack<basik_var> &dynamic_vars = code->dynamic_vars;
    const_data_t**   &const_data   = code->const_data;

    uint8_t* &ptr  = code->ptr;
    uint8_t* &prog = code->prog;

    basik_val* ret = nullptr;

    // Running

    while (*(uint16_t*)prog) {
        uint16_t op = *(uint16_t*)prog;
        prog += 2;

        size_t instr = prog-code->orig;

        // printf("----- %d %zu -----\n",op,stacki);

        if (op == OpCodes::End) {
            /* do nothing */
            /* this should not even ever happen */
        }

        // Store

        else if (op == OpCodes::StoreSimple) {
            basik_val* val = code->stack_pop();
            uint32_t var = *(uint32_t*)prog; prog += 4;
            if (val == nullptr) return Result{new BasikException{"Got NULL for StoreSimple",instr},nullptr};
            if (simple_vars[var]->data != nullptr) gc->remove_ref(simple_vars[var]->data);
            gc->add_ref(val);
            simple_vars[var]->data = val;
        }

        else if (op == OpCodes::StoreDynamic) {
            basik_val* val = code->stack_pop();
            if (val == nullptr) return Result{new BasikException{"Got NULL for StoreDynamic",instr},nullptr};
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* var = code->dynvar_get(varname);
            if (var != nullptr) gc->remove_ref(var);
            printf("STOR DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            code->dynvar_set(varname,val);
        }

        // Load

        else if (op == OpCodes::LoadSimple) {
            uint32_t var = *(uint32_t*)prog; prog += 4;
            basik_val* val = simple_vars[var]->data;
            if (val == nullptr) return Result{new BasikException{format("Undefined variable `%s`",simple_vars[var]->name),instr},nullptr};
            code->stack_push(val);
        }

        else if (op == OpCodes::LoadDynamic) {
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* val = code->dynvar_get(varname);
            if (val == nullptr) return Result{new BasikException{format("Undefined variable `%s`",varname),instr},nullptr};
            printf("LOAD DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            code->stack_push(val);
        }

        // Remove

        else if (op == OpCodes::RemoveDynamic) { // Cleans up a dynamic variable
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            code->dynvar_rem(varname);
        }

        // Integers

        else if (op == OpCodes::PushChar) {
            uint8_t data = *(uint8_t*)prog;
            code->stack_push(new basik_val{DataType::Char,new BasikChar(data)});
            prog += 1;
        }
        else if (op == OpCodes::PushI16) {
            int16_t data = *(int16_t*)prog;
            code->stack_push(new basik_val{DataType::I16,new BasikI16(data)});
            prog += 2;
        }
        else if (op == OpCodes::PushI32) {
            int32_t data = *(int32_t*)prog;
            code->stack_push(new basik_val{DataType::I32,new BasikI32(data)});
            prog += 4;
        }
        else if (op == OpCodes::PushI64) {
            int64_t data = *(int64_t*)prog;
            code->stack_push(new basik_val{DataType::I64,new BasikI64(data)});
            prog += 8;
        }

        // String

        else if (op == OpCodes::PushString) {
            int32_t val_addr = *(int32_t*)prog;
            const_data_t* data = const_data[val_addr];
            code->stack_push(new basik_val{DataType::String,new BasikString(data->sz,(const char*)data->data)});
            prog += 4;
        }

        // List

        else if (op == OpCodes::ListBegin) {
            list_stack.push(new size_t(stacki));
        }
        else if (op == OpCodes::ListEnd) {
            if (list_stack.size == 0) return Result{new BasikException{format("Attempt to close a list that was not open"),instr},nullptr};
            size_t base = *list_stack.pop();
            size_t list_size = stacki-base;
            BasikList* list = new BasikList();
            for (size_t i = 0; i < list_size; i++) {
                list->append(*stack[base+i]);
            }
            for (size_t i = 0; i < list_size; i++) code->stack_pop();
            code->stack_push(new basik_val{DataType::List,list});
        }
        else if (op == OpCodes::ListExpand) {
            basik_val* val = code->stack_pop();
            if (val == nullptr) return Result{new BasikException{format("Attempt to expand NULL"),instr},nullptr};
            if (val->type == DataType::List) {
                BasikList l = *(BasikList*)val->data;
                for (size_t i = 0; i < l.data->size; i++) {
                    basik_val v = l[l.data->size-i-1];
                    code->stack_push(new basik_val{v.type,v.data});
                }
            } else
                return Result{new BasikException{format("Expand does not support type `%s`",get_data_type_str(val->type)),instr},nullptr};
        }

        // Arithmetic

        else if (op == OpCodes::Add) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException{format("Attempt to add NULL"),instr},nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException{format("Unsupported '+' betwen Char and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data+*((BasikChar*)b->data)->data)});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException{format("Unsupported '+' betwen I16 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data+*((BasikI16*)b->data)->data)});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException{format("Unsupported '+' betwen I32 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data+*((BasikI32*)b->data)->data)});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException{format("Unsupported '+' betwen I64 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data+*((BasikI64*)b->data)->data)});
            } else
                return Result{new BasikException{format("Unsupported '+' for `%s`\n",get_data_type_str(a->type)),instr},nullptr};
        }

        else if (op == OpCodes::Sub) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException{format("Attempt to add NULL"),instr},nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException{format("Unsupported '-' betwen Char and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data-*((BasikChar*)b->data)->data)});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException{format("Unsupported '-' betwen I16 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data-*((BasikI16*)b->data)->data)});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException{format("Unsupported '-' betwen I32 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data-*((BasikI32*)b->data)->data)});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException{format("Unsupported '-' betwen I64 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data-*((BasikI64*)b->data)->data)});
            } else
                return Result{new BasikException{format("Unsupported '-' for `%s`\n",get_data_type_str(a->type)),instr},nullptr};
        }

        else if (op == OpCodes::Mul) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException{format("Attempt to add NULL"),instr},nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException{format("Unsupported '*' betwen Char and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data*(*((BasikChar*)b->data)->data))});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException{format("Unsupported '*' betwen I16 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data*(*((BasikI16*)b->data)->data))});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException{format("Unsupported '*' betwen I32 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data*(*((BasikI32*)b->data)->data))});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException{format("Unsupported '*' betwen I64 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data*(*((BasikI64*)b->data)->data))});
            } else
                return Result{new BasikException{format("Unsupported '*' for `%s`\n",get_data_type_str(a->type)),instr},nullptr};
        }

        else if (op == OpCodes::Div) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException{format("Attempt to add NULL"),instr},nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException{format("Unsupported '/' betwen Char and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data/(*((BasikChar*)b->data)->data))});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException{format("Unsupported '/' betwen I16 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data/(*((BasikI16*)b->data)->data))});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException{format("Unsupported '/' betwen I32 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data/(*((BasikI32*)b->data)->data))});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException{format("Unsupported '/' betwen I64 and %s",get_data_type_str(b->type)),instr},nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data/(*((BasikI64*)b->data)->data))});
            } else
                return Result{new BasikException{format("Unsupported '/' for `%s`\n",get_data_type_str(a->type)),instr},nullptr};
        }

        // Stack

        else if (op == OpCodes::Pop) {
            code->stack_pop();
        }

        else if (op == OpCodes::Dup) {
            basik_val* v = code->stack_pop();
            code->stack_push(v);
            code->stack_push(v);
        }

        // Jumping

        else if (op == OpCodes::Jump) {
            uint64_t addr = *(uint64_t*)prog;
            prog += 8;
            prog = code->orig + addr;
        }

        else if (op == OpCodes::JumpIf) {
            uint64_t addr = *(uint64_t*)prog; prog += 8;
            basik_val* v = code->stack_pop();
            if (code->is_val_true(v)) {
                prog = code->orig + addr;
            }
        }

        else if (op == OpCodes::JumpIfNot) {
            uint64_t addr = *(uint64_t*)prog; prog += 8;
            basik_val* v = code->stack_pop();
            if (!code->is_val_true(v)) {
                prog = code->orig + addr;
            }
        }

        // ???

        else 
            return Result{new BasikException{format("Unknown instruction opcode: `%u`\n",op),instr},nullptr};
    
        gc->collect();

    }


    return Result{nullptr,ret};

}

int main() {

    uint8_t* bin = (uint8_t*)
        "\x01\x00\x00\x00"     // Const data pool
            "\x06\x00\x00\x00" //
            "HELLO\x00"        //

        "\x02\x00\x00\x00"     // Variable data pool
            "x\0"              // x
            "y\0"              // y
        
        "\x08\x00"             // Push I32
            "\x00\x00\x00\x00" // 0

        "\x13\x00" // Dup

        "\x16\x00" // Jump If Not
            "\x20\x00\x00\x00\x00\x00\x00\x00" // 32

        "\x03\x00" // Store Dynamic
            "x\0"  // 'x'

        "\x14\x00" // Jump
            "\x24\x00\x00\x00\x00\x00\x00\x00" // 36

        "\x03\x00"
            "y\0"

        "\x00\x00" // End of program
    ;

    gc_t* gc = new gc_t();
    Code* code = new Code(gc,(const char*)bin);

    pre_run(code);

    Result res = run(code);

    if (res.except != nullptr) {
        fprintf(stderr,"ERROR: Runtime exception: (%zu)\n  %s\n",res.except->addr,res.except->text);
        exit(1);
    }

    // printf("`%s` `%u`\n",((BasikString*)simple_vars[0]->data->data)->data,*((BasikI32*)simple_vars[1]->data->data)->data);

    return 0;
}