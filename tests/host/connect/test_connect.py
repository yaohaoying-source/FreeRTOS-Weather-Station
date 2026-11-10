"""Connection and MQTT query tests against the real onenet_connect.c."""
import ctypes as C
import re
import llvmlite.binding as llvm
llvm.set_option('', '--opaque-pointers');llvm.initialize()
llvm.initialize_native_target();llvm.initialize_native_asmprinter()
with open('connect_host.ll', encoding='utf-8') as f: src=f.read()
src=src.replace('or disjoint ','or ').replace('nofpclass(nan inf) ','').replace(' nneg ',' ')
src=re.sub(r'^attributes #(\d+) = .*$',r'attributes #\1 = { nounwind }',src,flags=re.M)
src=re.sub(r'^target datalayout = .*$','',src,flags=re.M)
src=re.sub(r'^target triple = .*$','target triple = "'+llvm.get_default_triple()+'"',src,flags=re.M)
lib=C.CDLL('msvcrt')
for name in ('strcmp','strncmp','strspn','strstr','strcpy','strlen'):
    llvm.add_symbol(name,C.cast(getattr(lib,name),C.c_void_p).value)
module=llvm.parse_assembly(src);module.verify()
target=llvm.Target.from_default_triple().create_target_machine();module.data_layout=str(target.target_data)
engine=llvm.create_mcjit_compiler(module,target);engine.finalize_object()
def fn(name, ret, *types):
    addr=engine.get_function_address(name);assert addr,name
    return C.CFUNCTYPE(ret,*types)(addr)
class Info(C.Structure):
    _fields_=[('at',C.c_int),('config',C.c_int),('tcp',C.c_int),
              ('close',C.c_int),('mqtt',C.c_int),('sub',C.c_int)]
reset=fn('HostReset',None,C.c_uint32)
begin_status=fn('HostBeginStatus',None,C.c_uint32)
calls=fn('HostCalls',C.c_uint32)
saw_set=fn('HostSawSet',C.c_uint32)
begins=fn('HostBegins',C.c_uint32)
ends=fn('HostEnds',C.c_uint32)
sends=fn('HostSends',C.c_uint32)
tick=fn('HostTick',C.c_uint32)
connect=fn('OneNET_Connect',C.c_int,C.c_char_p,C.POINTER(Info))
query=fn('OneNET_QueryMqttState',C.c_int,C.POINTER(C.c_uint8),C.c_uint32)
i=Info()
reset(0);assert connect(b'AT+MCONFIG=dummy',C.byref(i))==0 and i.sub==0 and saw_set()==1
reset(1);assert connect(b'AT+MCONFIG=dummy',C.byref(i))==1 and i.sub==1 and saw_set()==0
reset(2);assert connect(b'AT+MCONFIG=dummy',C.byref(i))==0 and calls()==11
s=C.c_uint8()
reset(3);assert query(C.byref(s),100)==0 and s.value==1 and begins()==1 and ends()==1
reset(4);assert query(C.byref(s),100)==1 and s.value==255 and ends()==1
reset(5);assert query(C.byref(s),100)==3 and tick()>=100 and ends()==1
reset(3);begin_status(2);assert query(C.byref(s),100)==2 and sends()==0 and ends()==0
print('PASS: subscription failure, stale connection path, state-before-OK, timeout and AT lock release')
