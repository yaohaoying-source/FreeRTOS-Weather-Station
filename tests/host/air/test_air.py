"""Tests the real air780e.c against a simulated UART/DMA and clock."""
import ctypes as C
import re
import llvmlite.binding as llvm
llvm.set_option('', '--opaque-pointers')
llvm.initialize()
llvm.initialize_native_target()
llvm.initialize_native_asmprinter()
with open('air_host.ll', encoding='utf-8') as f: src=f.read()
src=src.replace('or disjoint ', 'or ').replace('nofpclass(nan inf) ', '').replace(' nneg ', ' ')
src=re.sub(r'^attributes #(\d+) = .*$',r'attributes #\1 = { nounwind }',src,flags=re.M)
src=re.sub(r'^target datalayout = .*$','',src,flags=re.M)
src=re.sub(r'^target triple = .*$','target triple = "'+llvm.get_default_triple()+'"',src,flags=re.M)
lib=C.CDLL('msvcrt')
for name in ('strcmp','memcmp','memcpy','memset'):
    llvm.add_symbol(name,C.cast(getattr(lib,name),C.c_void_p).value)
module=llvm.parse_assembly(src);module.verify()
target=llvm.Target.from_default_triple().create_target_machine()
module.data_layout=str(target.target_data)
engine=llvm.create_mcjit_compiler(module,target);engine.finalize_object()
def fn(name, ret, *types):
    addr=engine.get_function_address(name)
    assert addr, name
    return C.CFUNCTYPE(ret,*types)(addr)
reset=fn('HostReset',None,C.c_uint32)
set_task=fn('HostSetTask',None,C.c_uint32)
inject=fn('HostInject',None,C.c_char_p)
send=fn('Air780E_SendAndWait',C.c_int,C.c_char_p,C.c_char_p,C.c_uint32)
has_error=fn('Air780E_HasRxError',C.c_uint8)
restart=fn('Air780E_RestartRx',C.c_int)
writes=fn('HostWrites',C.c_uint32)
callbacks=fn('HostCallbacks',C.c_uint32)
tick=fn('HostTick',C.c_uint32)
nested=fn('HostNestedStatus',C.c_int)
overflows=fn('HostLineOverflow',C.c_uint32)
errors=fn('HostErrorCount',C.c_uint32)
restarts=fn('HostRestartCount',C.c_uint32)
collisions=fn('HostCollisionCount',C.c_uint32)

reset(0)
assert send(b'AT\r\n',b'OK',100)==0 and writes()==1
reset(1)
inject(b'OK\r\n') # an old response already buffered cannot satisfy a new command
assert send(b'AT\r\n',b'OK',100)==3 and tick()>=100
reset(0)
inject(b'OLD\r\n'*40)
assert send(b'AT\r\n',b'OK',100)==2 and writes()==0  # bounded pre-drain
assert send(b'AT\r\n',b'OK',100)==0 and writes()==1
reset(3)
assert send(b'AT\r\n',b'OK',100)==0 and callbacks()==1
reset(4)
before=collisions()
assert send(b'AT\r\n',b'OK',100)==0
assert callbacks()==1 and nested()==2 and writes()==1 and collisions()==before+1
reset(5)
assert send(b'AT\r\n',b'OK',100)==0 and overflows()==1
reset(2)
old=errors()
assert send(b'AT\r\n',b'OK',100)==1 and has_error()==1 and errors()==old+1
old=restarts()
assert restart()==0 and has_error()==0 and restarts()==old+1
reset(0)
set_task(2)
assert send(b'AT\r\n',b'OK',100)==2 and writes()==0
set_task(1)
assert send(b'AT\r\n',b'OK',100)==0 and writes()==1
print('PASS: bounded no-response, buffered stale OK, unsolicited message, collision, long line, RX error/restart, task ownership')
