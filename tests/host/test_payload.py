"""Host-side tests of the real onenet_upload.c with a dummy topic.
The generated LLVM is retargeted for a local test only; hardware verification is separate.
"""
import ctypes as C
import json
import re
import llvmlite.binding as llvm

llvm.set_option('', '--opaque-pointers')
llvm.initialize()
llvm.initialize_native_target()
llvm.initialize_native_asmprinter()
with open('onenet_payload_host.ll', encoding='utf-8') as f:
    src = f.read()
src = src.replace('or disjoint ', 'or ').replace('nofpclass(nan inf) ', '')
src = src.replace('@__2snprintf', '@snprintf')
src = re.sub(r'^@llvm\.used = .*$','',src,flags=re.M)
src = re.sub(r'^attributes #(\d+) = .*$',r'attributes #\1 = { nounwind }',src,flags=re.M)
src = re.sub(r'^target datalayout = .*$','',src,flags=re.M)
src = re.sub(r'^target triple = .*$','target triple = "'+llvm.get_default_triple()+'"',src,flags=re.M)
lib=C.CDLL('msvcrt')
for name in ('strchr','strlen','strncmp','strspn','strstr'):
    llvm.add_symbol(name,C.cast(getattr(lib,name),C.c_void_p).value)
llvm.add_symbol('snprintf',C.cast(lib._snprintf,C.c_void_p).value)
module=llvm.parse_assembly(src)
module.verify()
target=llvm.Target.from_default_triple().create_target_machine()
module.data_layout=str(target.target_data)
engine=llvm.create_mcjit_compiler(module,target)
engine.finalize_object()
def fn(name, ret, *types):
    addr=engine.get_function_address(name)
    assert addr, name
    return C.CFUNCTYPE(ret,*types)(addr)

class Report(C.Structure):
    _fields_=[('dht',C.c_uint8),('temp',C.c_uint8),('humi',C.c_uint8),
              ('bmp',C.c_uint8),('light_valid',C.c_uint8),('light',C.c_uint8),
              ('dust_valid',C.c_uint8),('press',C.c_double),('pm25',C.c_double),
              ('limit',C.c_double),('enable',C.c_uint8),('alarm',C.c_uint8),
              ('buzzer',C.c_uint8)]
class Result(C.Structure):
    _fields_=[('id',C.c_uint32),('code',C.c_uint32),('state',C.c_uint8)]
class Pending(C.Structure):
    _fields_=[('result',Result),('started',C.c_uint32),('timeout',C.c_uint32),
              ('kind',C.c_uint8),('active',C.c_uint8),('ready',C.c_uint8)]

fmt=fn('OneNET_FormatWeather',C.c_int,C.c_void_p,C.c_uint32,C.c_uint32,C.POINTER(Report))
fmt_event=fn('OneNET_FormatPm25Event',C.c_int,C.c_void_p,C.c_uint32,C.c_uint32,C.c_float,C.c_float)
start=fn('OneNET_PendingStart',C.c_uint8,C.POINTER(Pending),C.c_uint8,C.c_uint32,C.c_uint32,C.c_uint32)
on_line=fn('OneNET_PendingOnLine',None,C.POINTER(Pending),C.c_char_p)
fail=fn('OneNET_PendingFail',None,C.POINTER(Pending),C.c_uint8)
take=fn('OneNET_PendingTake',C.c_uint8,C.POINTER(Pending),C.c_uint32,C.POINTER(Result))
def parse(buf):
    cmd=buf.value.decode('ascii')
    assert cmd.endswith('"\r\n'),cmd
    payload=cmd.split('0,0,"',1)[1][:-3].replace(r'\22','"')
    return cmd,json.loads(payload)

for bits in range(16):
    d=Report(bits&1,23,57,(bits>>1)&1,(bits>>2)&1,61,(bits>>3)&1,
             1001.25,42.75,35.0,1,1,0)
    b=C.create_string_buffer(512)
    n=fmt(b,len(b),123,C.byref(d))
    assert 0<n<len(b),(bits,n)
    cmd,j=parse(b)
    assert '/thing/property/post' in cmd and j['id']=='123'
    p=j['params']
    expected={'pm25_limit','buzzer_enable','buzzer_state'}
    if bits&1: expected.update(('temp','humi'))
    if bits&2: expected.add('press')
    if bits&4: expected.add('light')
    if bits&8: expected.update(('pm25','pm25_alarm'))
    assert set(p)==expected,(bits,p)
    assert p['pm25_limit']['value']==35
    assert p['buzzer_enable']['value'] is True
    assert p['buzzer_state']['value'] is False
    if bits&1: assert (p['temp']['value'],p['humi']['value'])==(23,57)
    if bits&2: assert p['press']['value']==1001.25
    if bits&4: assert p['light']['value']==61
    if bits&8: assert p['pm25']['value']==42.75 and p['pm25_alarm']['value'] is True

maximum=Report(1,85,100,1,1,100,1,1100.0,500.0,500.0,1,1,1)
b=C.create_string_buffer(512)
assert 0 < fmt(b,len(b),0xFFFFFFFF,C.byref(maximum)) < len(b)
_,j=parse(b)
assert j['id']=='4294967295' and j['params']['pm25']['value']==500.0

# Guard bytes and failure on truncation.
d=Report(1,23,57,1,1,61,1,1001.25,42.75,35,1,1,0)
for size in (1,8,32,80,160,256):
    area=(C.c_ubyte*(size+8))()
    for k in range(size,size+8): area[k]=0xA5
    assert fmt(area,size,1,C.byref(d))==-1,size
    assert list(area[size:])==[0xA5]*8,size

b=C.create_string_buffer(256)
assert fmt_event(b,len(b),7,C.c_float(42.5),C.c_float(35))>0
_,j=parse(b)
assert j['id']=='7' and j['params']['pm25_alert']['value']=={'pm25_value':42.5,'limit_value':35}

def line(topic,id,code):
    return ('+MSUB: "$sys/test_product/test_device/thing/'+topic+
            '",30,{"id":"'+str(id)+'","code":'+str(code)+'}').encode()
p=Pending();r=Result()
assert start(C.byref(p),0,17,100,500)==1
assert start(C.byref(p),0,18,100,500)==0  # overlapping pending upload
on_line(C.byref(p),line('event/post/reply',17,200))
on_line(C.byref(p),line('property/post/reply',16,200))
assert take(C.byref(p),599,C.byref(r))==0
on_line(C.byref(p),line('property/post/reply',17,200))  # ACK may precede modem OK
assert take(C.byref(p),599,C.byref(r))==1 and (r.id,r.code,r.state)==(17,200,1)
assert take(C.byref(p),600,C.byref(r))==0
assert start(C.byref(p),1,18,0xFFFFFFF0,50)==1
assert take(C.byref(p),0x10,C.byref(r))==0
assert take(C.byref(p),0x22,C.byref(r))==1 and r.state==3 # tick rollover
assert start(C.byref(p),1,19,10,100)==1
on_line(C.byref(p),line('event/post/reply',19,403))
assert take(C.byref(p),20,C.byref(r))==1 and (r.code,r.state)==(403,0)
assert start(C.byref(p),0,20,10,100)==1
on_line(C.byref(p),line('property/post/reply',20,200))
fail(C.byref(p),0)  # modem failure must override an early cloud ACK
assert take(C.byref(p),11,C.byref(r))==1 and r.state==0
print('PASS: 16 sensor combinations; truncation guards; event JSON; matching, errors, early ACK and rollover')
