"""Tests callback routing with upload replies and a full mock downlink queue."""
import ctypes as C
import re
import llvmlite.binding as llvm
llvm.set_option('', '--opaque-pointers');llvm.initialize()
llvm.initialize_native_target();llvm.initialize_native_asmprinter()
with open('event_flow_host.ll',encoding='utf-8') as f: src=f.read()
src=src.replace('or disjoint ','or ').replace('nofpclass(nan inf) ','').replace(' nneg ',' ')
src=src.replace('@__2snprintf','@snprintf')
src=re.sub(r'^@llvm\.used = .*$','',src,flags=re.M)
src=re.sub(r'^attributes #(\d+) = .*$',r'attributes #\1 = { nounwind }',src,flags=re.M)
src=re.sub(r'^target datalayout = .*$','',src,flags=re.M)
src=re.sub(r'^target triple = .*$','target triple = "'+llvm.get_default_triple()+'"',src,flags=re.M)
lib=C.CDLL('msvcrt')
for name in ('strcmp','strncmp','strspn','strstr','strlen','strchr','memcpy','memset'):
    llvm.add_symbol(name,C.cast(getattr(lib,name),C.c_void_p).value)
llvm.add_symbol('snprintf',C.cast(lib._snprintf,C.c_void_p).value)
module=llvm.parse_assembly(src);module.verify()
target=llvm.Target.from_default_triple().create_target_machine();module.data_layout=str(target.target_data)
engine=llvm.create_mcjit_compiler(module,target);engine.finalize_object()
def fn(name, ret, *types):
    addr=engine.get_function_address(name);assert addr,name
    return C.CFUNCTYPE(ret,*types)(addr)
push=fn('HostPush',None,C.c_float,C.c_float)
step=fn('HostStep',None)
advance=fn('HostAdvance',None,C.c_uint32)
line=fn('HostLine',None,C.c_char_p)
count=fn('HostCount',C.c_uint32)
dropped=fn('HostDropped',C.c_uint32)
queued=fn('HostQueued',C.c_uint32)
success=fn('HostSuccess',C.c_uint32)
sends=fn('HostSends',C.c_uint32)
state=fn('HostState',C.c_uint32)
sets=fn('HostSets',C.c_uint32)
for i in range(5): push(40.0+i,30.0)
assert (count(),queued(),dropped())==(4,4,1)
step();assert count()==4 and sends()==1 and state()==2
advance(3001);step()
assert count()==4 and sends()==1 and state()==3
advance(9999);step();assert count()==4 and sends()==1
advance(1);step();assert count()==4 and sends()==2
prefix=b'+MSUB: "$sys/test_product/test_device/thing/'
line(prefix+b'property/set",35,{"id":"6","params":{"pm25_limit":20}}')
line(prefix+b'event/post/reply",30,{"id":"1","code":200}')
step()
assert count()==3 and success()==1 and sets()==1
print('PASS: four-entry event queue, overflow count, timeout retention, delayed retry, ACK removal and simultaneous set')
