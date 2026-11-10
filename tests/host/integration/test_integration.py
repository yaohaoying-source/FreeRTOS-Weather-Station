"""Tests callback routing with upload replies and a full mock downlink queue."""
import ctypes as C
import re
import llvmlite.binding as llvm
llvm.set_option('', '--opaque-pointers');llvm.initialize()
llvm.initialize_native_target();llvm.initialize_native_asmprinter()
with open('integration_host.ll',encoding='utf-8') as f: src=f.read()
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
class Result(C.Structure):
    _fields_=[('id',C.c_uint32),('code',C.c_uint32),('state',C.c_uint8)]
reset=fn('HostReset',None)
line=fn('HostLine',None,C.c_char_p)
start=fn('HostStart',C.c_uint8,C.c_uint32)
take=fn('HostTake',C.c_uint8,C.c_uint32,C.POINTER(Result))
received=fn('HostReceived',C.c_uint32)
overflow=fn('HostOverflow',C.c_uint32)
discarded=fn('HostDiscarded',C.c_uint32)
offline=fn('HostOffline',None)
prefix=b'+MSUB: "$sys/test_product/test_device/thing/'
setting=prefix+b'property/set",35,{"id":"1","params":{"pm25_limit":30}}'
reply=prefix+b'property/post/reply",30,{"id":"7","code":200}'
wrong=prefix+b'property/post/reply",30,{"id":"8","code":200}'
r=Result()

# Downlink can arrive before or after an upload ACK without stealing either.
for seq in ((setting,reply),(reply,setting)):
    reset();assert start(7)==1
    for text in seq: line(text)
    assert received()==1 and overflow()==0
    assert take(1,C.byref(r))==1 and (r.id,r.code,r.state)==(7,200,1)

reset();assert start(7)==1
line(setting);line(setting);line(setting)
assert received()==2 and overflow()==1
line(wrong);assert take(1,C.byref(r))==0
line(reply);assert take(1,C.byref(r))==1 and r.state==1

reset();offline();line(setting)
assert received()==0 and discarded()==1
print('PASS: upload ACK/downlink ordering, wrong ID, full downlink queue and offline discard')
