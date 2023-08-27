import ast
import struct
from sys import argv
from os import path
from pprint import pprint

from typing import Union, Callable

global_vars = set([
    
])

if len(argv) <= 2:
    print('Usage: \x1b[35m%s\x1b[39m %s \x1b[36m<output.bsk>\x1b[39m' % (argv[0],'\x1b[32m'+argv[1]+'\x1b[39m' if len(argv)==2 else '\x1b[36m<input.py>\x1b[39m'))
    exit(1)

auto_idx = {}
auto_key = '<auto>'
def auto(key:str=None,init_key:str=None):
    global auto_key
    if key:
        auto_key = key
        auto_idx[auto_key] = auto_idx.get(init_key,-1)+1
    else:
        auto_idx[auto_key] += 1
    return auto_idx[auto_key]

def parts_name(base:str) -> tuple[str,str,list[str]]:
    type:str = ''
    name:str = ''
    tags:list[str] = []
    
    s = ''
    tagz = False
    for c in base:
        if tagz:
            if c == '/':
                tags.append(s)
                s = ''
            else:
                s += c
        else:
            if c == '/': 
                name = s
                s = ''
                tagz = True
            elif c == ';':
                type = s
                s = ''
            else: s += c
 
    if not tagz:
        name += s
    elif s:
        tags.append(s)
            
    return type, name, tags

def jparts_name(type:str, name:str, tags:list[str]) -> str:
    return type+(';' if type else '')+name+('/' if tags else '')+'/'.join(tags)

def add_name(base:str, ext:str) -> str:
    typ,nam,tag = parts_name(base)
    nam += '::'+ext
    return jparts_name(typ,nam,tag)

def remove_tag(base:str, t:str) -> str:
    typ,nam,tag = parts_name(base)
    return jparts_name(typ,nam,list(filter(lambda tt: tt!=t,tag)))

class Label:
    addr: Union[int,None]
    def __init__(self):
        self.addr = None
        
def Enum(name:str, parents: tuple, attributes:dict):
    t = type(name,parents,attributes)
    def has(val:int) -> bool:
        return val in attributes.values()
    t.has = has
    return t

class OpCodes(metaclass=Enum):
    End            = auto('OpCode')
    StoreSimple    = auto()
    LoadSimple     = auto()
    StoreDynamic   = auto()
    LoadDynamic    = auto()
    StoreGlobal    = auto()
    LoadGlobal     = auto()
    PushString     = auto()
    PushChar       = auto()
    PushI16        = auto()
    PushI32        = auto()
    PushI64        = auto()
    ListBegin      = auto()
    ListEnd        = auto()
    ListExpand     = auto()
    RemoveDynamic  = auto()
    Add            = auto()
    Sub            = auto()
    Div            = auto()
    Mul            = auto()
    Pop            = auto()
    Dup            = auto()
    Jump           = auto()
    JumpIf         = auto()
    JumpIfNot      = auto()
    Return         = auto()
    Call           = auto()
    PushNull       = auto()
    Equals         = auto()
    LoadFunction   = auto()
    
class SpecialOp(metaclass=Enum):
    Label       = auto('SpecialOp','OpCode')
    AddCompiled = auto()

Instruction = tuple[int,...]
    
class Program:
    
    constants:    list
    vars:         list[str]
    globals:      set[str]
    instructions: list[Instruction]
    file:         str
    name:         str
    
    def __init__(self,file:str,name:str):
        self.constants = []
        self.vars = []
        self.globals = set([])
        self.instructions = []
        self.file = file
        self.name = name
        
    def add_constant(self, v) -> int:
        self.constants.append(v)
        return len(self.constants)-1
    
    def load_var(self, v:str) -> list[Instruction]:
        """
        Loads a variable
        """
        if v in self.vars:
            return [(OpCodes.LoadSimple,self.vars.index(v))]
        elif v in self.globals:
            return [(OpCodes.LoadGlobal,v)]
        else:
            return [(OpCodes.LoadDynamic,v)]
    
    def explore(self, node:ast.expr, flags:list[str]=[]) -> list[Instruction]:
        i = []
        
        loc = (self.file,node.lineno,node.col_offset)
        
        if isinstance(node,ast.Expr):
            i.extend(self.explore(node.value))
            
        elif isinstance(node,ast.Call):
            i.extend(self.explore(node.func))
            i.append((OpCodes.ListBegin,))
            for sub_node in node.args:
                i.extend(self.explore(sub_node))
            i.append((OpCodes.ListEnd,))
            i.append((OpCodes.Call,))
        
        elif isinstance(node,ast.Name):
            # i.append((OpCodes.LoadDynamic,node.id))
            i.extend(self.load_var(node.id))
            
        elif isinstance(node,ast.Constant):
            if type(node.value) == str:
                i.append((OpCodes.PushString,self.add_constant(node.value)))
            elif type(node.value) == int:
                i.append((OpCodes.PushI64,node.value*(-1 if 'neg' in flags else 1)))
            elif node.value == None:
                i.append((OpCodes.PushNull,))
            else:
                assert False, '%s:%d:%d: Got unknown constant kind: %s' % (*loc,type(node.value))
                
        elif isinstance(node,ast.List):
            i.append((OpCodes.ListBegin,))
            for sub_node in node.elts:
                i.extend(self.explore(sub_node))
            i.append((OpCodes.ListEnd,))
            
        elif isinstance(node,ast.If):
            l_if = Label()
            l_not = Label()
            i.extend(self.explore(node.test))
            i.append((OpCodes.JumpIfNot,l_not))
            for sub_node in node.body:
                i.extend(self.explore(sub_node))
            i.append((OpCodes.Jump,l_if))
            i.append((SpecialOp.Label,l_not))
            for sub_node in node.orelse:
                i.extend(self.explore(sub_node))
            i.append((SpecialOp.Label,l_if))
            
        elif isinstance(node,ast.BinOp):
            
            if isinstance(node.op,ast.Add):
                i.extend(self.explore(node.left))
                i.extend(self.explore(node.right))
                i.append((OpCodes.Add,))
                
            elif isinstance(node.op,ast.Sub):
                i.extend(self.explore(node.left))
                i.extend(self.explore(node.right))
                i.append((OpCodes.Sub,))
                
            elif isinstance(node.op,ast.Mult):
                i.extend(self.explore(node.left))
                i.extend(self.explore(node.right))
                i.append((OpCodes.Mul,))
                
            elif isinstance(node.op,ast.Div):
                i.extend(self.explore(node.left))
                i.extend(self.explore(node.right))
                i.append((OpCodes.Div,))
                
            else:
                assert False, '%s:%d:%d: Got unknown binary operation : %s' % (*loc,repr(type(node.op).__name__),)
                
        elif isinstance(node,ast.UnaryOp):
            
            if isinstance(node.op,ast.USub):
                i.extend(self.explore(node.operand,['neg']))
            
            else:
                assert False, '%s:%d:%d: Got unknown unary operation : %s' % (*loc,repr(type(node.op).__name__),)
                
        elif isinstance(node,ast.Compare):
            
            assert len(node.ops) == 1, '%s:%d:%d: Only one comparison at a time is supported, got %s' % (*loc,repr(list(map(lambda v: type(v).__name__,node.ops))),)
            
            if isinstance(node.ops[0],ast.Eq):
                i.extend(self.explore(node.left))
                i.extend(self.explore(node.comparators[0]))
                i.append((OpCodes.Equals,))
                
            else:
                assert False, '%s:%d:%d: Got unknown comparison : %s' % (*loc,repr(type(node.ops[0]).__name__),)
                
        elif isinstance(node,ast.Assign):
            
            assert len(node.targets) == 1, '%s:%d:%d Expected 1 assignation target, got %s' % (*loc,repr([n for n in node.targets]))
            assert isinstance(node.targets[0],ast.Name), '%s:%d:%d Expected Name as an assignation target, got %s' % (*loc,str(type(node.targets[0])))
            
            i.extend(self.explore(node.value))
            
            tgt:str = node.targets[0].id
            
            if tgt in self.globals:
                i.append((OpCodes.StoreGlobal,tgt))
            elif tgt in self.vars:
                i.append((OpCodes.StoreSimple,tgt))
            else:
                i.append((OpCodes.StoreDynamic,tgt))
                
        elif isinstance(node,ast.FunctionDef):
            
            self.globals.add(node.name)
            
            n = remove_tag(add_name(self.name,node.name),'main')
            b = generate_bytecode(n,self.name,node.body,init_instructions=[(OpCodes.LoadDynamic,'...'),(OpCodes.ListExpand,),*((OpCodes.StoreSimple,i) for i,a in reversed(list(enumerate(node.args.args))))],init_vars=[a.arg for a in node.args.args])
            for c in b: i.append((SpecialOp.AddCompiled,c))
            i.append((OpCodes.LoadFunction,n))
            i.append((OpCodes.StoreGlobal,node.name))
            
        elif isinstance(node,ast.Global):
            for n in node.names: self.globals.add(n)
            
        else:
            assert False, '%s:%d:%d: Got unknown node type: %s' % (*loc,repr(type(node)))
        
        return i
    
class CompiledCode:
    
    name:  str
    bytes: bytearray
    
    def __init__(self, name:str, bytes:bytearray):
        self.name = name
        self.bytes = bytes
    
def repass_u64(l:Label,addr:int) -> Callable[[bytearray],None]:
    def r(b:bytearray):
        b[addr:addr+8] = struct.pack('<Q',l.addr)
    return r

def generate_bytecode( name:str, path:str, body:list[ast.expr], init_instructions:list[Instruction]=[], init_vars:list[str]=[] ) -> list[CompiledCode]:
    
    compiled:list[CompiledCode] = []

    p = Program(path,name)
    p.globals = global_vars
    
    p.instructions.extend(init_instructions)
    p.vars.extend(init_vars)
    
    for node in body:
        p.instructions.extend(p.explore(node) or [])
        
    p.instructions.append((OpCodes.PushNull,))
    p.instructions.append((OpCodes.Return,))
    
    pprint(p.instructions)
    
    header = bytearray()
    
    header += struct.pack('<I',len(p.constants))
    for c in p.constants:
        if type(c) == str:
            header += struct.pack('<I',len(c)+1)
            header += bytes(c,'utf-8') + b'\0'
        else:
            assert False, 'Unsupported constant type `%s`' % (repr(type(c)),)
        
    header += struct.pack('<I',len(p.vars))
    for v in p.vars:
        header += bytes(v,'utf-8') + b'\0'
        
    repass: list[tuple[Callable[[bytearray],None]]] = []
    
    bytecode = bytearray()
        
    for i in p.instructions:
        
        if SpecialOp.has(i[0]):
            
            if i[0] == SpecialOp.Label:
                i[1].addr = len(bytecode)
                
            elif i[0] == SpecialOp.AddCompiled:
                compiled.append(i[1])
            
        else:
        
            bytecode += struct.pack('<B',i[0])
            
            if i[0] in (OpCodes.LoadDynamic,OpCodes.LoadGlobal,OpCodes.LoadFunction,OpCodes.StoreDynamic,OpCodes.StoreGlobal):
                bytecode += bytes(i[1],'utf-8') + b'\0'
                
            elif i[0] in (OpCodes.LoadSimple,OpCodes.StoreSimple):
                bytecode += struct.pack('<I',i[1])
            
            elif i[0] == OpCodes.PushString:
                bytecode += struct.pack('<I',i[1])
            
            elif i[0] == OpCodes.PushChar:
                bytecode += struct.pack('<c',i[1])    
            elif i[0] == OpCodes.PushI16:
                bytecode += struct.pack('<s',i[1])
            elif i[0] == OpCodes.PushI32:
                bytecode += struct.pack('<i',i[1])
            elif i[0] == OpCodes.PushI64:
                bytecode += struct.pack('<q',i[1])
                
            elif i[0] in (OpCodes.JumpIfNot,OpCodes.JumpIf,OpCodes.Jump):
                a = i[1]
                if isinstance(a,Label):
                    repass.append((repass_u64(a,len(bytecode)),))
                    bytecode += struct.pack('<Q',0)
                else:
                    bytecode += struct.pack('<Q',i[1])
    
    for i in repass:
        i[0](bytecode)
        
    compiled.append( CompiledCode(name, header+bytecode) )
        
    return compiled

with open(argv[1],'r') as f:
    m = ast.parse(f.read(),argv[1],'exec',type_comments=True)
    
    b = generate_bytecode('file;'+path.basename(argv[1])+'/main',argv[1],m.body)
    
    objects = [ bytes(o.name,'utf-8','ignore')+b'\0'+o.bytes for o in b ]
    
    with open(argv[2],'wb') as out:
        out.write(struct.pack('<I',len(objects)))
        for obj in objects:
            out.write(struct.pack('<Q',len(obj)))
            out.write(obj)