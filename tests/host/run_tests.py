"""Run host simulations against the current firmware source (Windows only).

Example:
  python tests/host/run_tests.py --armclang "C:/Keil_v5/ARM/ARMCLANG/bin/armclang.exe"
Requires Python and llvmlite. No device credentials or hardware are used.
"""
import argparse
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
PROJECT = HERE.parent.parent

def extract_function(text, marker):
    start = text.index(marker)
    first = text.index('{', start)
    depth = 0
    mode = 'code'
    index = first
    while index < len(text):
        c = text[index]
        n = text[index + 1] if index + 1 < len(text) else ''
        if mode == 'code':
            if c == '"' or c == "'":
                mode = c
            elif c == '/' and n == '*':
                mode = 'block'
                index += 1
            elif c == '/' and n == '/':
                mode = 'line'
                index += 1
            elif c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return text[start:index + 1]
        elif mode == 'block' and c == '*' and n == '/':
            mode = 'code'
            index += 1
        elif mode == 'line' and c == '\n':
            mode = 'code'
        elif mode in ('"', "'"):
            if c == '\\':
                index += 1
            elif c == mode:
                mode = 'code'
        index += 1
    raise ValueError('Unclosed function: ' + marker)

def prepare_extracts():
    main = (PROJECT / 'Core' / 'Src' / 'main.c').read_text(encoding='utf-8')
    callback = extract_function(main, 'static void Network_OnMqttLine(')
    event_fn = extract_function(main, 'static void Network_ProcessPm25Event(void)')
    begin = main.index('if (xQueueSend(pm25_event_queue, &event, 0) == pdPASS)')
    dropped = main.index('g_pm25_event_dropped++;', begin)
    end = main.index('}', dropped) + 1
    for folder in ('integration', 'event_flow'):
        (HERE / folder / 'network_callback.inc').write_text(callback, encoding='utf-8')
    (HERE / 'event_flow' / 'event_function.inc').write_text(event_fn, encoding='utf-8')
    (HERE / 'event_flow' / 'event_enqueue.inc').write_text(main[begin:end], encoding='utf-8')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--armclang', default=shutil.which('armclang'),
                        help='Path to the Keil armclang executable')
    args = parser.parse_args()
    if not args.armclang:
        parser.error('Pass the path to armclang with --armclang')
    if sys.platform != 'win32':
        parser.error('These ctypes/JIT test scripts currently require Windows')
    prepare_extracts()
    suites = (
        (HERE, 'onenet_payload_host.c', 'onenet_payload_host.ll', 'test_payload.py'),
        (HERE / 'air', 'air_host.c', 'air_host.ll', 'test_air.py'),
        (HERE / 'connect', 'connect_host.c', 'connect_host.ll', 'test_connect.py'),
        (HERE / 'integration', 'integration_host.c', 'integration_host.ll', 'test_integration.py'),
        (HERE / 'event_flow', 'event_flow_host.c', 'event_flow_host.ll', 'test_event_flow.py'),
    )
    for folder, source, output, test in suites:
        print('Running ' + test, flush=True)
        includes = [folder, HERE / 'common', PROJECT / 'Core' / 'Inc']
        if folder.name == 'air':
            includes = [folder, PROJECT / 'Core' / 'Inc', HERE / 'common']
        cmd = [args.armclang, '--target=arm-arm-none-eabi', '-mcpu=cortex-m3',
               '-mthumb', '-std=c99', '-S', '-emit-llvm', '-O1']
        cmd += ['-I' + str(include) for include in includes]
        cmd += [source, '-o', output]
        subprocess.run(cmd, cwd=folder, check=True)
        subprocess.run([sys.executable, test], cwd=folder, check=True)
    print('All host simulations passed. Hardware tests are still required.')

if __name__ == '__main__':
    main()
