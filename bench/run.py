#!/usr/bin/env python3
"""Sequential CPU/FPS benchmarks; isolated Xvfb by default, --desktop is visible."""
import argparse
import contextlib
import json
import math
import os
from pathlib import Path
import select
import shlex
import signal
import struct
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
TICKS = os.sysconf('SC_CLK_TCK')


def cpu_seconds(pid):
    fields = Path(f'/proc/{pid}/stat').read_text().split(') ', 1)[1].split()
    return (int(fields[11]) + int(fields[12])) / TICKS


def memory_kib(pid):
    for line in Path(f'/proc/{pid}/status').read_text().splitlines():
        if line.startswith('VmRSS:'):
            return int(line.split()[1])
    return 0


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


@contextlib.contextmanager
def xvfb(width, height):
    readfd, writefd = os.pipe()
    process = subprocess.Popen(
        ['Xvfb', '-displayfd', str(writefd), '-screen', '0', f'{width}x{height}x24',
         '-nolisten', 'tcp'], pass_fds=(writefd,),
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.close(writefd)
    try:
        if not select.select([readfd], [], [], 10)[0]:
            raise RuntimeError('Xvfb startup timeout')
        display = ':' + os.read(readfd, 80).decode().strip()
        if display == ':' or process.poll() is not None:
            raise RuntimeError('Xvfb cannot create its display socket')
        yield display, [process.pid]
    finally:
        os.close(readfd)
        stop(process)


def measure(binary, directory, display, servers, width, height, size, args, index):
    history = directory / f'world-{index}.tsv'
    env = dict(os.environ, DISPLAY=display,
               LIFE_BENCH_FRAMES=str(directory / f'frames-{index}'))
    env.pop('XSCREENSAVER_WINDOW', None)
    command = [str(binary), '-fullscreen' if args.desktop else '-root',
               '-size', str(size), '-fps', str(args.fps), '-seed', str(args.seed),
               '-seconds', '3600', '--mode', getattr(args, 'mode', 'random'),
               '--history', str(history)]
    if getattr(args, 'engine', 'cpu') == 'gpu':
        command.append('--gpu')
    if getattr(args, 'style', 'classic') == 'fancy':
        command.append('--fancy')
    # On the real desktop this includes other clients' Xorg/compositor activity.
    initial = sum(cpu_seconds(pid) for pid in servers)
    initial_time = time.monotonic()
    time.sleep(args.baseline)
    baseline = 100 * (sum(cpu_seconds(pid) for pid in servers) - initial) / (time.monotonic() - initial_time)
    launched = time.monotonic()
    process = subprocess.Popen(command, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        time.sleep(args.warmup)
        if process.poll() is not None:
            raise RuntimeError(process.stderr.read().decode() or 'Preview closed during warmup')
        if args.desktop:
            windows = subprocess.check_output(
                ['xdotool', 'search', '--onlyvisible', '--all', '--pid', str(process.pid),
                 '--class', '^GameOfLife$'], text=True).split()
            if len(windows) != 1:
                raise RuntimeError('Expected exactly one visible GameOfLife window')
            geometry = subprocess.check_output(
                ['xdotool', 'getwindowgeometry', '--shell', windows[0]], text=True)
            fields = dict(line.split('=', 1) for line in geometry.splitlines())
            if (int(fields['WIDTH']), int(fields['HEIGHT'])) != (width, height):
                raise RuntimeError(f'Actual desktop window differs from requested {width}x{height}: {geometry}')
        client0 = cpu_seconds(process.pid)
        server0 = sum(cpu_seconds(pid) for pid in servers)
        start = time.monotonic()
        phases = []
        previous, previous_client, previous_server = start, client0, server0
        for duration in getattr(args, 'phases', None) or [args.seconds]:
            time.sleep(duration)
            end = time.monotonic()
            if process.poll() is not None:
                raise RuntimeError(process.stderr.read().decode() or 'Preview closed during measurement')
            client_now = cpu_seconds(process.pid)
            server_now = sum(cpu_seconds(pid) for pid in servers)
            phase_client = 100 * (client_now - previous_client) / (end - previous)
            phase_server = 100 * (server_now - previous_server) / (end - previous)
            phases.append(dict(start_s=previous - launched, end_s=end - launched,
                               client_cpu=phase_client, graphics_cpu=phase_server,
                               total_cpu=phase_client + phase_server))
            if getattr(args, 'phases', None):
                print(json.dumps(phases[-1]), flush=True)
            previous, previous_client, previous_server = end, client_now, server_now
        client = 100 * (client_now - client0) / (end - start)
        server = 100 * (server_now - server0) / (end - start)
        memory = memory_kib(process.pid)
        stop(process)
        error = process.stderr.read().decode()
        if process.returncode or error:
            raise RuntimeError(error or f'Exit status {process.returncode}')
        raw = b'' if args.no_frame_probe else Path(env['LIFE_BENCH_FRAMES']).read_bytes()
        frames = [item[0] for item in struct.iter_unpack('d', raw)]
        for phase in phases:
            begin, finish = launched + phase['start_s'], launched + phase['end_s']
            phase['fps'] = None if args.no_frame_probe else sum(begin <= t <= finish for t in frames) / (finish - begin)
        frames = [t for t in frames if start <= t <= end]
        gaps = sorted(b - a for a, b in zip(frames, frames[1:]))
        # The application's primary 6x10 core font reserves a 26-pixel footer.
        # Pass the actual height when testing a server using the fallback font.
        footer = args.footer_height if width >= 40 and height > args.footer_height + 16 else 0
        grid_height = height - footer
        # Read the actual generated grid; do not duplicate the program's defaults.
        recipe = history.read_text().splitlines()[0].split('\t')
        columns, rows = int(recipe[3]), int(recipe[4])
        actual_size = size or max(1, min(width // columns, grid_height // rows))
        result = dict(backend='desktop' if args.desktop else 'Xvfb',
                      width=width, height=height, size=size, actual_size=actual_size,
                      columns=columns, rows=rows, cells=columns * rows, footer_px=footer,
                      mode=recipe[2], engine=getattr(args, 'engine', 'cpu'),
                      style=getattr(args, 'style', 'classic'),
                      target_fps=args.fps, seed=args.seed, warmup_s=args.warmup,
                      elapsed_s=end - start, client_cpu=client, graphics_cpu=server,
                      graphics_baseline_cpu=baseline, total_cpu=client + server,
                      rss_kib=memory, fps=None if args.no_frame_probe else len(frames) / (end - start),
                      p95_frame_ms=1000 * gaps[math.ceil(len(gaps) * .95) - 1] if gaps else None,
                      phases=phases)
        print(json.dumps(result), flush=True)
        return result
    finally:
        stop(process)
        process.stderr.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--desktop', action='store_true')
    parser.add_argument('--engine', choices=('cpu', 'gpu'), default='cpu',
                        help='GPU needs OpenGL 4.3; Xvfb usually uses software GL, not hardware')
    parser.add_argument('--style', choices=('classic', 'fancy'), default='classic',
                        help='Fancy flame rendering requires --engine gpu')
    parser.add_argument('--graphics-pids', default='', help='Comma-separated Xorg/compositor PIDs for --desktop')
    parser.add_argument('--cases', default='1920x1080:0,1920x1080:4,1920x1080:3,1920x1080:2,1920x1080:1,3840x2160:0,3840x2160:8,3840x2160:6,3840x2160:4,7680x4320:0,7680x4320:16')
    parser.add_argument('--seconds', type=float, default=10)
    parser.add_argument('--phases', type=lambda s: [float(x) for x in s.split(',')],
                        help='Consecutive measurement durations, e.g. 15,30,45 to follow a world aging')
    parser.add_argument('--binary', type=Path, help='Existing binary; instrumented unless --no-frame-probe is used')
    parser.add_argument('--warmup', type=float, default=3)
    parser.add_argument('--baseline', type=float, default=1)
    parser.add_argument('--fps', type=int, default=15)
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--mode', choices=('random', 'random-figures'), default='random',
                        help='Random cells or a sparse mix of library figures')
    parser.add_argument('--footer-height', type=int, default=26,
                        help='Caption footer pixels; default 26 for the 6x10 font')
    parser.add_argument('--no-frame-probe', action='store_true', help='Measure the shipped binary without the XSync/frame counter')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.style == 'fancy' and args.engine != 'gpu':
        parser.error('--style fancy needs --engine gpu')
    if min(args.seconds, args.warmup, args.baseline, *(args.phases or [1])) <= 0:
        parser.error('Durations must be positive')
    if args.desktop and not args.graphics_pids:
        parser.error('--desktop needs --graphics-pids')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    results = []
    with tempfile.TemporaryDirectory(prefix='life-bench-') as temp:
        directory = Path(temp)
        binary = args.binary.resolve() if args.binary else (ROOT / 'game_of_life' if args.no_frame_probe else directory / 'game_of_life')
        if not args.no_frame_probe and not args.binary:
            gpu_flags = (['-DLIFE_HAVE_GPU=1'] + shlex.split(subprocess.check_output(
                ['pkg-config', '--cflags', '--libs', 'epoxy'], text=True))) if args.engine == 'gpu' else []
            subprocess.run(['cc', '-O3', '-std=c99', '-Wall', '-Wextra', '-Wpedantic',
                            '-DXFlush=bench_flush', str(ROOT / 'game_of_life.c'),
                            str(ROOT / 'life.c'), str(ROOT / 'figures.c'), str(ROOT / 'scene.c'),
                            str(ROOT / 'journal.c'),
                            str(ROOT / 'gpu.c'),
                            str(ROOT / 'bench/frames.c'),
                            '-lX11', '-lXss', *gpu_flags, '-o', str(binary)], check=True)
        for index, case in enumerate(args.cases.split(',')):
            dimensions, size = case.split(':')
            width, height = map(int, dimensions.split('x'))
            print(f'Running {case} at {args.fps} fps ({index + 1}/{len(args.cases.split(","))})', flush=True)
            if args.desktop:
                environment = contextlib.nullcontext((os.environ.get('DISPLAY', ':0'),
                                    [int(pid) for pid in args.graphics_pids.split(',')]))
            else:
                environment = xvfb(width, height)
            with environment as (display, servers):
                result = measure(binary, directory, display, servers, width, height, int(size), args, index)
            results.append(result)
            args.output.write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
