#include "basik.h"

/********************************\ 
* Implementations for the header *
\********************************/

/**
 * Retreives the string representation of a data type value
 */
constexpr inline const char* get_data_type_str(DataType dt) {
    if (dt >= sizeof(DataTypeStr)/sizeof(const char*)) return "<INVALID TYPE>";
    return DataTypeStr[dt];
}

template<typename ... A>
const char* format(const char* format, A ...args) {
    int size = snprintf(nullptr,0,format,args...) + 1;
    char* buff = new char[size];
    snprintf(buff,(size_t)size,format,args...);
    return buff;
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
        this->data = (T**)malloc(sizeof(T*)*20);
        this->size = 0;
    }
    Stack(size_t size) {
        this->cap = size;
        this->cap_grow = 20;
        this->data = (T**)malloc(sizeof(T*)*size);
        this->size = 0;
    }
    Stack(size_t size, size_t cap_grow) {
        this->cap = size;
        this->cap_grow = cap_grow;
        this->data = (T**)malloc(sizeof(T*)*size);
        this->size = 0;
    }
    void grow(size_t size) {
        if (this->cap >= size) return;
        T** new_data = (T**)malloc(sizeof(T*)*size);
        for (size_t i = 0; i < size; i++) new_data[i] = 0; // Apparently some data from this->data gets copied for no reason as well as some junk
        memcpy(new_data,this->data,this->size*sizeof(T*));
        free(this->data);
        this->data = new_data;
        this->cap = size;
    }
    void push(T* value) {
        if (this->size >= this->cap-1) this->grow(this->cap+this->cap_grow);
        this->data[this->size++] = value;
    }
    void prune() {
        T** tmp_data = (T**)malloc(sizeof(T*)*this->size);
        size_t sz = 0;
        for (size_t i = 0; i < this->size; i++) if (this->data[i] != nullptr) tmp_data[sz++] = this->data[i];
        free(this->data);
        this->data = (T**)malloc(sizeof(T*)*sz);
        memcpy(this->data,tmp_data,sz*sizeof(T*));
        this->size = sz;
        this->cap = sz;
    }
    T* pop() {
        if (this->size == 0) return nullptr;
        return this->data[--this->size];
    }
    ~Stack() {
        free(this->data);
    }
};

struct CodeObj {
    const char* full_name;
    Stack<const char> tags;
    const char* type;
    const char* name;
    uint8_t* data;
    size_t data_sz;
    Code* code;
};

/**
 * A structure used to store global data that can be accessed across scopes.
 * NOTE: It might be a little bit costy, so it should be used as little as possible.
 * NOTE: For now it pretty much behaves like dynvars, except that is values can be used multiple times.
 */
struct Globals {

    gc_t* gc;
    Stack<basik_var> vars;

    Globals(gc_t* gc) {
        this->gc = gc;
    }

    void set(const char* name, basik_val* value) {
        gc->add_ref(value);
        for (size_t i = 0; i < vars.size; i++) {
            basik_var* vv = vars.data[i];
            if (vv != nullptr && !strcmp(vv->name,name)) {
                gc->remove_ref(vv->data);
                vv->data = value;
                return;
            }
        }
        basik_var* v = new basik_var{value,name};
        for (size_t i = 0; i < vars.size; i++) {
            if (vars.data[i] == nullptr) {
                vars.data[i] = v;
                return;
            }
        }
        vars.push(v);
    }

    basik_val* get(const char* name) {
        for (size_t i = 0; i < vars.size; i++) {
            basik_var* v = vars.data[i];
            if (v != nullptr && !strcmp(v->name,name)) {
                return v->data;
            }
        }
        return nullptr;
    }

};

// Basic Data Structures //

basik_val::~basik_val() {
         if (this->type == DataType::Char)     delete ((BasikChar*)      this->data);
    else if (this->type == DataType::I16)      delete ((BasikI16*)       this->data);
    else if (this->type == DataType::I32)      delete ((BasikI32*)       this->data);
    else if (this->type == DataType::I64)      delete ((BasikI64*)       this->data);
    else if (this->type == DataType::String)   delete ((BasikString*)    this->data);
    else if (this->type == DataType::List)     delete ((BasikList*)      this->data);
    else if (this->type == DataType::Function) delete ((BasikFunction*)  this->data);
    else if (this->type == DataType::Bool)     delete ((BasikBool*)      this->data);
    else {
        fprintf(stderr,"Attempt to destroy unsupported `%s` (%u)\n",get_data_type_str(this->type),this->type);
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
    this->data = new uint8_t[len+1];
    this->len = len;
    memcpy(this->data,data,len);
    this->data[len-1] = 0;
}
BasikString::~BasikString() {
    delete[] this->data;
    this->len = 0;
}

// List
BasikList::BasikList() {
    this->data = new Stack<basik_val>();
}
void BasikList::append(basik_val* v) {
    this->data->push(v);
}
constexpr inline basik_val* BasikList::operator[](size_t i) {
    return this->data->data[i];
}
BasikList::~BasikList() {
    delete this->data;
}

// Function
BasikFunction::BasikFunction(Code* code) {
    this->code = code;
    this->callback = nullptr;
}
BasikFunction::BasikFunction(Result(*callback)(Code*,size_t,basik_val**)) {
    this->callback = callback;
    this->code = nullptr;
}
BasikFunction::~BasikFunction() {

}

// Bool
BasikBool::BasikBool(bool data) {
    this->data = new bool(data);
}
BasikBool::~BasikBool() {
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

gc_t::gc_t() {
    this->refs = new Stack<gc_ref>(256,256);
}

inline bool gc_t::add_ref_ex( basik_val* v, size_t* count ) {
    if (v == nullptr) return false;
    for (size_t i = 0; i < refs->size; i++) {
        gc_ref* r = refs->data[i];
        if (r != nullptr && refs->data[i]->v == v) {
            r->c++;
            // printf("\t\t\t\tGC ADD %p %zu\n",v,r->c);
            if (count != nullptr) *count = r->c;
            return false;
        }
    }
    // printf("\t\t\t\tGC NEW %p 1\n",v);
    if (count != nullptr) *count = 1;
    refs->push(new gc_ref{v,1});
    return true;
}

inline bool gc_t::add_ref( basik_val* v ) {
    if (v == nullptr) return false;
    return add_ref_ex(v,nullptr);
}

bool gc_t::remove_ref( basik_val* v ) {
    for (size_t i = 0; i < refs->size; i++) {
        if (refs->data[i] != nullptr && refs->data[i]->v == v) {
            if(refs->data[i]->c)--refs->data[i]->c;
            // printf("\t\t\t\tGC REM %p %zu\n",refs->data[i]->v,refs->data[i]->c);
            return true;
        }
    }
    return false;
}
 
size_t gc_t::collect( void ) {
    size_t c = 0;
    for (size_t i = 0; i < this->refs->size; i++) {
        gc_ref* &r = this->refs->data[i];
        if (r != nullptr && r->c == 0) {
            // printf("\t\t\t\tGC COL %p\n",r->v);
            delete r->v;
            delete r;
            r = nullptr;
            c++;
        }
    }
    this->refs->prune();
    return c;
}

/********************************\ 
*           Main stuff           *
\********************************/

struct Code {

    bool initialized;

    basik_val** stack;
    size_t stacki;
    
    Stack<size_t> list_stack;
    basik_var** simple_vars;
    Stack<basik_var> dynamic_vars;
    Globals* glob;
    const_data_t** const_data;

    const char* bytecode;
    uint8_t* ptr;
    uint8_t* orig;
    uint8_t* prog;

    Stack<CodeObj>* objects;

    gc_t* gc;

    Code( gc_t* gc, Globals* glob, const char* bytecode, Stack<CodeObj>* objects ) {
        this->stack = new basik_val*[65536];
        this->stacki = 0;
        this->gc = gc;
        this->bytecode = bytecode;
        this->glob = glob;
        this->initialized = false;
        this->objects = objects;
    }

    /**
     * Sets a dynamic variable with the provided name to the provided value
     */
    void dynvar_set(const char* name, basik_val* value) {
        gc->add_ref(value);
        for (size_t i = 0; i < dynamic_vars.size; i++) {
            basik_var* vv = dynamic_vars.data[i];
            if (vv != nullptr && !strcmp(vv->name,name)) {
                gc->remove_ref(vv->data);
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
        if (v->type == DataType::Bool)   return *((BasikBool*)v->data)->data;
        return false;
    }

};

/********************************\ 
*             Globals            *
\********************************/

Stack<CodeObj>* objects = new Stack<CodeObj>();

void pre_run(Code* code) {
    if (code->initialized) return; // Do not init again if it already was

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

    code->initialized = true;
}

Result run(Code* code) {
    basik_val** &stack  = code->stack;
    size_t      &stacki = code->stacki;

    gc_t* &gc = code->gc;

    Stack<size_t>    &list_stack   = code->list_stack;

    const char*      &bytecode     = code->bytecode;
    basik_var**      &simple_vars  = code->simple_vars;
    Stack<basik_var> &dynamic_vars = code->dynamic_vars;
    Globals*         &glob         = code->glob;
    const_data_t**   &const_data   = code->const_data;

    uint8_t* &ptr  = code->ptr;
    uint8_t* &prog = code->prog;

    basik_val* ret = nullptr;

    // Running

    while (*(uint8_t*)prog) {
        uint8_t op = *(uint8_t*)prog;
        prog += 1;

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
            if (val == nullptr) return Result{new BasikException("Got NULL for StoreSimple",instr,code),nullptr};
            if (simple_vars[var]->data != nullptr) gc->remove_ref(simple_vars[var]->data);
            gc->add_ref(val);
            simple_vars[var]->data = val;
        }

        else if (op == OpCodes::StoreDynamic) {
            basik_val* val = code->stack_pop();
            if (val == nullptr) return Result{new BasikException("Got NULL for StoreDynamic",instr,code),nullptr};
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* var = code->dynvar_get(varname);
            if (var != nullptr) gc->remove_ref(var);
            // printf("STOR DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            code->dynvar_set(varname,val);
        }

        else if (op == OpCodes::StoreGlobal) {
            basik_val* val = code->stack_pop();
            if (val == nullptr) return Result{new BasikException("Got NULL for StoreGlobal",instr,code),nullptr};
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* var = glob->get(varname);
            if (var != nullptr) gc->remove_ref(var);
            // printf("STOR DYN %s = %d\n",varname,*((BasikI32*)val->data)->data);
            glob->set(varname,val);
        }

        // Load

        else if (op == OpCodes::LoadSimple) {
            uint32_t var = *(uint32_t*)prog; prog += 4;
            basik_val* val = simple_vars[var]->data;
            if (val == nullptr) return Result{new BasikException(format("Undefined variable `%s`",simple_vars[var]->name),instr,code),nullptr};
            code->stack_push(val);
        }

        else if (op == OpCodes::LoadDynamic) {
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* val = code->dynvar_get(varname);
            if (val == nullptr) return Result{new BasikException(format("Undefined local variable `%s`",varname),instr,code),nullptr};
            code->stack_push(val);
        }

        else if (op == OpCodes::LoadGlobal) {
            const char* varname = (const char*)prog; prog += strlen((const char*)prog)+1;
            basik_val* val = glob->get(varname);
            if (val == nullptr) return Result{new BasikException(format("Undefined global variable `%s`",varname),instr,code),nullptr};
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

        // NULL

        else if (op == OpCodes::PushNull) {
            code->stack_push(nullptr);
        }

        // List

        else if (op == OpCodes::ListBegin) {
            list_stack.push(new size_t(stacki));
        }
        else if (op == OpCodes::ListEnd) {
            if (list_stack.size == 0) return Result{new BasikException(format("Attempt to close a list that was not open"),instr,code),nullptr};
            size_t base = *list_stack.pop();
            size_t list_size = stacki-base;
            BasikList* list = new BasikList();
            for (size_t i = 0; i < list_size; i++) {
                gc->add_ref(stack[base+i]);
                list->append(stack[base+i]);
            }
            for (size_t i = 0; i < list_size; i++) code->stack_pop();
            code->stack_push(new basik_val{DataType::List,list});
        }
        else if (op == OpCodes::ListExpand) {
            basik_val* val = code->stack_pop();
            if (val == nullptr) return Result{new BasikException(format("Attempt to expand NULL"),instr,code),nullptr};
            if (val->type == DataType::List) {
                BasikList l = *(BasikList*)val->data;
                for (size_t i = 0; i < l.data->size; i++) {
                    basik_val* v = l[l.data->size-i-1];
                    code->stack_push(v);
                }
            } else
                return Result{new BasikException(format("Expand does not support type `%s`",get_data_type_str(val->type)),instr,code),nullptr};
        }

        // Arithmetic

        else if (op == OpCodes::Add) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException(format("Attempt to add NULL"),instr,code),nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException(format("Unsupported '+' betwen Char and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data+*((BasikChar*)b->data)->data)});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException(format("Unsupported '+' betwen I16 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data+*((BasikI16*)b->data)->data)});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException(format("Unsupported '+' betwen I32 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data+*((BasikI32*)b->data)->data)});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException(format("Unsupported '+' betwen I64 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data+*((BasikI64*)b->data)->data)});
            } else
                return Result{new BasikException(format("Unsupported '+' for `%s`\n",get_data_type_str(a->type)),instr,code),nullptr};
        }

        else if (op == OpCodes::Sub) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException(format("Attempt to add NULL"),instr,code),nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException(format("Unsupported '-' betwen Char and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data-*((BasikChar*)b->data)->data)});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException(format("Unsupported '-' betwen I16 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data-*((BasikI16*)b->data)->data)});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException(format("Unsupported '-' betwen I32 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data-*((BasikI32*)b->data)->data)});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException(format("Unsupported '-' betwen I64 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data-*((BasikI64*)b->data)->data)});
            } else
                return Result{new BasikException(format("Unsupported '-' for `%s`\n",get_data_type_str(a->type)),instr,code),nullptr};
        }

        else if (op == OpCodes::Mul) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException(format("Attempt to add NULL"),instr,code),nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException(format("Unsupported '*' betwen Char and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data*(*((BasikChar*)b->data)->data))});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException(format("Unsupported '*' betwen I16 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data*(*((BasikI16*)b->data)->data))});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException(format("Unsupported '*' betwen I32 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data*(*((BasikI32*)b->data)->data))});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException(format("Unsupported '*' betwen I64 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data*(*((BasikI64*)b->data)->data))});
            } else
                return Result{new BasikException(format("Unsupported '*' for `%s`\n",get_data_type_str(a->type)),instr,code),nullptr};
        }

        else if (op == OpCodes::Div) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr || b == nullptr) return Result{new BasikException(format("Attempt to add NULL"),instr,code),nullptr};
            if (a->type == DataType::Char) {
                if (b->type != DataType::Char) return Result{new BasikException(format("Unsupported '/' betwen Char and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::Char,new BasikChar(*((BasikChar*)a->data)->data/(*((BasikChar*)b->data)->data))});
            } else if (a->type == DataType::I16) {
                if (b->type != DataType::I16) return Result{new BasikException(format("Unsupported '/' betwen I16 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I16,new BasikI16(*((BasikI16*)a->data)->data/(*((BasikI16*)b->data)->data))});
            } else if (a->type == DataType::I32) {
                if (b->type != DataType::I32) return Result{new BasikException(format("Unsupported '/' betwen I32 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I32,new BasikI16(*((BasikI32*)a->data)->data/(*((BasikI32*)b->data)->data))});
            } else if (a->type == DataType::I64) {
                if (b->type != DataType::I64) return Result{new BasikException(format("Unsupported '/' betwen I64 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                code->stack_push(new basik_val{DataType::I64,new BasikI64(*((BasikI64*)a->data)->data/(*((BasikI64*)b->data)->data))});
            } else
                return Result{new BasikException(format("Unsupported '/' for `%s`\n",get_data_type_str(a->type)),instr,code),nullptr};
        }

        else if (op == OpCodes::Equals) {
            basik_val* b = code->stack_pop();
            basik_val* a = code->stack_pop();
            if (a == nullptr) {
                code->stack_push(new basik_val{DataType::Bool,new BasikBool(b == nullptr)});
            } else if (a->type == DataType::Char) {
                if (b == nullptr) code->stack_push(new basik_val{DataType::Bool,new BasikBool(false)});
                else {
                    if (b->type != DataType::Char) return Result{new BasikException(format("Unsupported '==' betwen Char and %s",get_data_type_str(b->type)),instr,code),nullptr};
                    code->stack_push(new basik_val{DataType::Bool,new BasikBool(*((BasikChar*)a->data)->data==(*((BasikChar*)b->data)->data))});
                }
            } else if (a->type == DataType::I16) {
                if (b == nullptr) code->stack_push(new basik_val{DataType::Bool,new BasikBool(false)});
                else {
                    if (b->type != DataType::I16) return Result{new BasikException(format("Unsupported '==' betwen I16 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                    code->stack_push(new basik_val{DataType::Bool,new BasikBool(*((BasikI16*)a->data)->data==(*((BasikI16*)b->data)->data))});
                }
            } else if (a->type == DataType::I32) {
                if (b == nullptr) code->stack_push(new basik_val{DataType::Bool,new BasikBool(false)});
                else {
                    if (b->type != DataType::I32) return Result{new BasikException(format("Unsupported '==' betwen I32 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                    code->stack_push(new basik_val{DataType::Bool,new BasikBool(*((BasikI32*)a->data)->data==(*((BasikI32*)b->data)->data))});
                }
            } else if (a->type == DataType::I64) {
                if (b == nullptr) code->stack_push(new basik_val{DataType::Bool,new BasikBool(false)});
                else {
                    if (b->type != DataType::I64) return Result{new BasikException(format("Unsupported '==' betwen I64 and %s",get_data_type_str(b->type)),instr,code),nullptr};
                    code->stack_push(new basik_val{DataType::Bool,new BasikBool(*((BasikI64*)a->data)->data==(*((BasikI64*)b->data)->data))});
                }
            } else if (a->type == DataType::String) {
                if (b == nullptr) code->stack_push(new basik_val{DataType::Bool,new BasikBool(false)});
                else {
                    if (b->type != DataType::String) return Result{new BasikException(format("Unsupported '==' betwen String and %s",get_data_type_str(b->type)),instr,code),nullptr};
                    const char* da = (const char*)((BasikString*)a->data)->data;
                    const char* db = (const char*)((BasikString*)b->data)->data;
                    /*printf("%zu %zu\n",strlen(da),strlen(db));
                    printf("`%s` `%s`\n",da,db);
                    for (size_t i = 0; i < strlen(da); i++) {
                        printf("%d\n",da[i]);
                    }
                    printf("\n");
                    for (size_t i = 0; i < strlen(db); i++) {
                        printf("%d\n",db[i]);
                    }*/
                    code->stack_push(new basik_val{DataType::Bool,new BasikBool(!strcmp((const char*)((BasikString*)a->data)->data,(const char*)((BasikString*)b->data)->data))});
                }
            } else
                code->stack_push(new basik_val{DataType::Bool,new BasikBool(a == b)});
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

        // Functions

        else if (op == OpCodes::Return) {
            basik_val* v = code->stack_pop();
            gc->add_ref(v);
            ret = v;
            break;
        }

        else if (op == OpCodes::Call) {
            basik_val* vb = code->stack_pop();
            basik_val* va = code->stack_pop();
            if (va == nullptr) return Result{new BasikException("Attempt to call NULL",instr,code),nullptr};
            if (vb == nullptr) return Result{new BasikException("Attempt to call with NULL",instr,code),nullptr};
            if (va->type != DataType::Function) return Result{new BasikException(format("Attempt to call non-function `%s`",get_data_type_str(va->type)),instr,code),nullptr};
            if (vb->type != DataType::List) return Result{new BasikException(format("Attempt to call with `%s`",get_data_type_str(vb->type)),instr,code),nullptr};
            BasikFunction* f = (BasikFunction*)va->data;
            BasikList* args = (BasikList*)vb->data;
            if (f->code) {
                pre_run(f->code);
                f->code->dynvar_set("...",vb);
                Result r = run(f->code);
                if (r.except != nullptr)
                    return Result{r.except->add_trace(instr,code),nullptr};
                code->stack_push(r.value);
            } else if (f->callback) {
                Result r = f->callback(code,args->data->size,args->data->data);
                if (r.except != nullptr)
                    return Result{r.except->add_trace(instr,code),nullptr};
                code->stack_push(r.value);
            }
        }

        else if (op == OpCodes::LoadFunction) {
            const char* id = (const char*)prog; prog += strlen((const char*)prog)+1;
            for (size_t i = 0; i < objects->size; i++) {
                CodeObj* obj = objects->data[i];
                if (!strcmp(obj->full_name,id)) {
                    code->stack_push(new basik_val{DataType::Function,new BasikFunction(obj->code)});
                    break;
                }
            }
        }

        // ???

        else 
            return Result{new BasikException(format("Unknown instruction opcode: `%u`\n",op),instr,code),nullptr};

        gc->collect();
    
    }
    
    // Dynvars cleanup
    for (size_t i = 0; i < dynamic_vars.size; i++) {
        basik_var* &v = dynamic_vars.data[i];
        if (v != nullptr && v->data != nullptr) {
            gc->remove_ref(v->data);
            delete v;
            v = nullptr;
        }
    }

    // Stack cleanup
    for (size_t i = 0; i < stacki; i++)
        gc->remove_ref(stack[i]);

    stacki = 0;

    gc->collect();
    dynamic_vars.prune();

    // Removes the reference after the GC cleanup to make it live
    // after the end of the call
    if (ret) gc->remove_ref(ret);

    return Result{nullptr,ret};

}

bool ends_with(const char* str, const char* end) {
    size_t strl = strlen(str);
    size_t endl = strlen(end);
    if (strl < endl) return false;
    for (size_t i = 0; i < endl; i++)
        if (str[strl-i-1] != end[endl-i-1]) 
            return false;
    return true;
}

bool has( Stack<const char> s, const char* v ) {
    for (size_t i = 0; i < s.size; i++) {
        if (!strcmp(s.data[i],v)) return true;
    }
    return true;
}

void print_repr(basik_val* v) {
         if (v->type == DataType::Char)   printf("'%u'",*((BasikChar*)v->data)->data); // Should escape it
    else if (v->type == DataType::I16)    printf("%di16",  *((BasikI16*)v->data)->data);
    else if (v->type == DataType::I32)    printf("%di32",  *((BasikI32*)v->data)->data);
    else if (v->type == DataType::I64)    printf("%zii64", *((BasikI64*)v->data)->data);
    else if (v->type == DataType::String) printf("\"%s\"",   ((BasikString*)v->data)->data); // Should also escape it
}

Result basik_std_print(Code* code, size_t argc, basik_val** argv) {
    for (size_t i = 0; i < argc; i++) {
        basik_val* arg = argv[i];
        if (arg == nullptr) printf("None");
        else {
                 if (arg->type == DataType::Char)   putchar(*((BasikChar*)arg->data)->data);
            else if (arg->type == DataType::I16)    printf("%d", *((BasikI16*)arg->data)->data);
            else if (arg->type == DataType::I32)    printf("%d", *((BasikI32*)arg->data)->data);
            else if (arg->type == DataType::I64)    printf("%zi",*((BasikI64*)arg->data)->data);
            else if (arg->type == DataType::String) printf("%s",  ((BasikString*)arg->data)->data);
            else if (arg->type == DataType::Bool)   printf("%s", *((BasikBool*)arg->data)->data ? "True" : "False");
            else printf("<object at %p>",arg);
        }
        if (i < argc-1) printf(" ");
    }
    printf("\n");
    return Result{nullptr,nullptr};
}

Result basik_std_input(Code* code, size_t argc, basik_val** argv) {
    char* value = new char[65536];
    scanf("%s",value);
    BasikString* val = new BasikString(strlen(value)+1,value);
    delete value;
    return Result{nullptr,new basik_val{DataType::String,val}};
}

int main(int argc, const char** argv) {

    if (argc == 1) {
        printf("Please provide a program to run.\n");
        exit(1);
    }

    FILE* f = fopen64(argv[1],"r");
    fseeko64(f,0,SEEK_END);
    size_t bin_len = ftello64(f);
    fseeko64(f,0,SEEK_SET);
    uint8_t* raw_bin = new uint8_t[bin_len+1];
    fread(raw_bin,1,bin_len,f);
    fclose(f);

    uint32_t object_count = *(uint32_t*)raw_bin; raw_bin += 4;
    for (uint32_t i = 0; i < object_count; i++) {
        CodeObj* obj = new CodeObj();
        uint64_t object_sz = *(uint64_t*)raw_bin;
        const char* object_full_name = (const char*)raw_bin+8;
        size_t object_full_name_len = strlen(object_full_name);
        uint8_t* object_data = raw_bin+8+object_full_name_len;
        size_t object_type_sep = -1llu;
        for (size_t i = 0; i < object_full_name_len; i++) {
            if (object_full_name[i] == ';') {
                object_type_sep = i;
                break;
            }
        }
        size_t np = 0;
        if (object_type_sep != -1llu) {
            obj->type = new char[object_type_sep+1];
            memcpy((void*)obj->type,object_full_name,object_type_sep);
            np = object_type_sep+1;
        }
        size_t object_tags_sep = object_full_name_len;
        for (size_t i = np; i < object_full_name_len; i++) {
            if (object_full_name[i] == '/') {
                object_tags_sep = i;
                break;
            }
        }
        obj->name = new char[object_tags_sep+1];
        memcpy((void*)obj->name,object_full_name+np,object_tags_sep-np);
        if (object_tags_sep != object_full_name_len) {
            object_tags_sep++;
            char* n = new char[4096];
            size_t ni = 0;
            while (object_tags_sep < object_full_name_len) {
                if (object_full_name[object_tags_sep] == '/') {
                    obj->tags.push(n);
                    n = new char[4096];
                    ni = 0;
                } else
                    n[ni++] = object_full_name[object_tags_sep];
                object_tags_sep++;
            }
            if (ni) {
                obj->tags.push(n);
            }
        }
        obj->full_name = object_full_name;
        obj->data = object_data+1;
        objects->push(obj);
        raw_bin += object_sz+8;
    }

    // Use this to debug the objects inside of the file
    /*printf("Found %zu objects\n",objects.size);
    for (size_t i = 0; i < objects.size; i++) {
        CodeObj* obj = objects.data[i];
        printf("%s : `%s` `%s`\n",obj->full_name,obj->type==nullptr?";nil;":obj->type,obj->name);
        for (size_t t = 0; t < obj->tags.size; t++) {
            printf("  - `%s`\n",obj->tags.data[t]);
        }
    }*/

    gc_t* gc = new gc_t();
    Globals* glob = new Globals(gc);

    glob->set("print",new basik_val{DataType::Function,new BasikFunction(basik_std_print)});
    glob->set("input",new basik_val{DataType::Function,new BasikFunction(basik_std_input)});

    Code* code = nullptr;

    for (size_t i = 0; i < objects->size; i++) {
        CodeObj* obj = objects->data[i];
        obj->code = new Code(gc,glob,(const char*)obj->data,objects);
        if (has(obj->tags,"main"))
            code = obj->code;
    }

    if (code == nullptr) {
        fprintf(stderr,"Could not find entry point, exitting.\n");
        return 1;
    }
    
    pre_run(code);

    Result res = run(code);

    if (res.except != nullptr) {
        fprintf(stderr,"ERROR: Runtime exception:\n");
        for (size_t i = 0; i < res.except->traci; i++) { 
            size_t j = res.except->traci-i-1;
            fprintf(stderr,"  in %p (at %zu):\n",res.except->code_trace[j],res.except->trace[j]);
        }
        fprintf(stderr,"    : %s\n",res.except->text);
        exit(1);
    }

    // printf("`%s` `%u`\n",((BasikString*)simple_vars[0]->data->data)->data,*((BasikI32*)simple_vars[1]->data->data)->data);

    return 0;
}