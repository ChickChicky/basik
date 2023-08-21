#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cinttypes>
#include <stdexcept>

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

enum OpCodes : uint16_t {
    NoOp,
    Store,
    PushString,
    PushI32,
};

enum DataType : uint16_t {
    String,
    I32
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

struct BasikI32  {
    uint32_t* data;
    BasikI32(uint32_t data) {
        this->data = new uint32_t(data);
    }
    ~BasikI32() {
        delete this->data;
    }
};

struct header_data_t {
    uint64_t sz;
    void* data;
};

struct egg_val {
    uint16_t type;
    void* data;
};

egg_val stack[65536];
egg_val* stackp = stack;

bool stack_push(egg_val v) {
    if ((size_t)stackp >= (size_t)stack+sizeof(stack)) return false;
    *stackp++ = v;
    return true;
}

egg_val stack_pop() {
    return *--stackp;
}

int main() {
    uint8_t* ptr = (uint8_t*)
        "\x01\x00\x00\x00" // Header Items count
            "\x06\x00\x00\x00"
            "HELLO\x00"
        
        "\x03\x00" // PushI32
            "\xff\x01\x00\x00" // -> 511
        "\x02\x00" // PushString
            "\x00\x00\x00\x00" // -> 0 ("HELLO")
        "\x01\x00"
        "\x01\x00"
    ;

    uint32_t header_data_count = *(uint32_t*)ptr;
    ptr += 4;

    header_data_t** header_data = new header_data_t*[header_data_count];
    header_data_t** header_data_base = header_data;

    for (uint32_t i = 0; i < header_data_count; i++) {
        uint32_t sz = *(uint32_t*)ptr;
        ptr += 4;
        *header_data = new header_data_t{sz,ptr};
        ptr += sz;
    }

    header_data = header_data_base;

    uint8_t* prog = ptr;
    uint8_t* orig = prog;
    size_t prog_len = 16;

    while (prog < orig+prog_len) {
        uint16_t op = *(uint16_t*)prog;
        prog += 2;
        printf("OP -> %d\n",op);
        if (op == OpCodes::NoOp) {
            /* do nothing */
        } else
        if (op == OpCodes::Store) {
            egg_val val = stack_pop();
            if (val.data != nullptr) {
                if (val.type == DataType::I32) {
                    printf("Store I32 %d\n",*((BasikI32*)val.data)->data);
                    delete (BasikI32*)val.data;
                }
                else if (val.type == DataType::String) {
                    printf("Store String `%s`\n",((BasikString*)val.data)->data);
                    delete (BasikString*)val.data;
                }
                else {
                    fprintf(stderr,"Unknown data type for store: `%d`\n",val.type);
                    exit(1);
                }
            }
        } else
        if (op == OpCodes::PushI32) {
            int32_t data = *(int32_t*)prog;
            stack_push(egg_val{DataType::I32,new BasikI32(data)});
            prog += 4;
        } else
        if (op == OpCodes::PushString) {
            int32_t val_addr = *(int32_t*)prog;
            header_data_t* data = header_data[val_addr];
            stack_push(egg_val{DataType::String,new BasikString(data->sz,(const char*)data->data)});
            prog += 4;
        }
        else {
            fprintf(stderr,"Unknown instruction opcode: `%u`\n",op);
            exit(1);
        }
    }

    return 0;
}