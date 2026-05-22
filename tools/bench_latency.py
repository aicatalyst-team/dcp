"""Round-trip latency benchmark for DCP transports.

Measures end-to-end ``bridge.call()`` latency — the full path of encode →
wire-out → device decode → handler → reply wire → host decode — and reports
median / p50 / p90 / p99 / IQR over N samples.

This is the measurement source for Figure 5 of the paper. Results are
written to ``docs/paper/figures/latency_data.json``; ``make_figures.py``
reads that file so the plotted figure is always derived from a recorded,
version-controlled measurement rather than hand-typed numbers.

Usage::

    # in-process loopback baseline (no hardware) — pure protocol overhead
    python tools/bench_latency.py --loopback

    # real device over UART
    python tools/bench_latency.py --serial COM6 --label "ESP32-S3 (native USB)"
    python tools/bench_latency.py --serial COM5 --label "ESP32-WROOM-32 (CH340)"

    # more samples, custom warmup
    python tools/bench_latency.py --serial COM6 --count 1000 --warmup 50

Each run updates one keyed entry in the JSON file; other entries are
preserved, so you can build up the full per-transport dataset across
several invocations.
"""
from __future__ import annotations

import argparse
import asyncio
import json
import statistics
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Force UTF-8 so the output renders on Windows consoles.
try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

from dcp.bridge import Bridge
from dcp.manifest import Manifest
from dcp.simulator import GenericSimulator
from dcp.transports.loopback import LoopbackTransport
from dcp.transports.uart import UartTransport

MANIFEST = ROOT / "examples" / "lamp_manifest.yaml"
JSON_OUT = ROOT / "docs" / "paper" / "figures" / "latency_data.json"

# The intent benchmarked. set_brightness is the canonical "typical call":
# one float parameter, idempotent, no return payload — representative of
# the common case and identical to the frame measured for the wire-size
# figure (19 bytes on the wire).
BENCH_INTENT = "set_brightness"
BENCH_PARAMS = {"level": 50}


# --------------------------------------------------------------------------

async def _bench(bridge: Bridge, count: int, warmup: int) -> list[float]:
    """Issue `warmup` discarded calls, then `count` timed calls. Returns ms."""
    for _ in range(warmup):
        r = await bridge.call(BENCH_INTENT, BENCH_PARAMS)
        if r.status != "ok":
            raise RuntimeError(f"warmup call returned status={r.status}")

    samples: list[float] = []
    for _ in range(count):
        t0 = time.perf_counter()
        r = await bridge.call(BENCH_INTENT, BENCH_PARAMS)
        elapsed_ms = (time.perf_counter() - t0) * 1000.0
        if r.status != "ok":
            raise RuntimeError(f"benchmark call returned status={r.status}")
        samples.append(elapsed_ms)
    return samples


def _stats(samples: list[float]) -> dict:
    """Summary statistics. Percentiles use the nearest-rank method."""
    s = sorted(samples)
    n = len(s)

    def pct(p: float) -> float:
        rank = max(1, min(n, int(round(p / 100.0 * n))))
        return s[rank - 1]

    q1, q3 = pct(25), pct(75)
    return {
        "n": n,
        "min": round(s[0], 4),
        "max": round(s[-1], 4),
        "mean": round(statistics.fmean(s), 4),
        "median": round(statistics.median(s), 4),
        "p50": round(pct(50), 4),
        "p90": round(pct(90), 4),
        "p99": round(pct(99), 4),
        "stdev": round(statistics.pstdev(s), 4),
        "q1": round(q1, 4),
        "q3": round(q3, 4),
        "iqr": round(q3 - q1, 4),
    }


# --------------------------------------------------------------------------

async def run_loopback(count: int, warmup: int) -> dict:
    manifest = Manifest.load(MANIFEST)
    host_tr, device_tr = LoopbackTransport.pair()
    sim = GenericSimulator(manifest, device_tr)
    sim_task = asyncio.create_task(sim.run(), name="bench-sim")

    bridge = Bridge(manifest, host_tr,
                    granted_capabilities={"lamp.write", "lamp.read"})
    await bridge.start()
    try:
        samples = await _bench(bridge, count, warmup)
    finally:
        await bridge.stop()
        sim_task.cancel()
        try:
            await sim_task
        except (asyncio.CancelledError, Exception):
            pass
    return _stats(samples)


async def run_uart(port: str, baud: int, count: int, warmup: int) -> dict:
    manifest = Manifest.load(MANIFEST)
    uart = UartTransport(port, baud=baud)
    await uart.open()
    bridge = Bridge(manifest, uart,
                    granted_capabilities={"lamp.write", "lamp.read"},
                    timeout=3.0)
    await bridge.start()
    try:
        samples = await _bench(bridge, count, warmup)
    finally:
        await bridge.stop()
    return _stats(samples)


# --------------------------------------------------------------------------

def _print_report(key: str, label: str, st: dict) -> None:
    print(f"\n=== {label} ===")
    print(f"  samples     {st['n']}")
    print(f"  min / max   {st['min']:.2f} / {st['max']:.2f} ms")
    print(f"  mean        {st['mean']:.2f} ms")
    print(f"  median      {st['median']:.2f} ms")
    print(f"  p90 / p99   {st['p90']:.2f} / {st['p99']:.2f} ms")
    print(f"  IQR (q1-q3) {st['iqr']:.2f} ms  ({st['q1']:.2f} - {st['q3']:.2f})")
    print(f"  stdev       {st['stdev']:.2f} ms")
    print(f"  -> written to latency_data.json under key '{key}'")


def _save(key: str, label: str, st: dict) -> None:
    data: dict = {}
    if JSON_OUT.exists():
        try:
            data = json.loads(JSON_OUT.read_text(encoding="utf-8"))
        except Exception:
            data = {}
    entry = dict(st)
    entry["label"] = label
    entry["intent"] = BENCH_INTENT
    entry["measured_at"] = time.strftime("%Y-%m-%d")
    data[key] = entry
    JSON_OUT.parent.mkdir(parents=True, exist_ok=True)
    JSON_OUT.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n",
                        encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--loopback", action="store_true",
                      help="in-process loopback transport (no hardware)")
    mode.add_argument("--serial", metavar="PORT",
                      help="UART transport on the given serial port")
    ap.add_argument("--baud", type=int, default=115200,
                    help="baud rate for --serial (default 115200)")
    ap.add_argument("--count", type=int, default=500,
                    help="number of timed samples (default 500)")
    ap.add_argument("--warmup", type=int, default=20,
                    help="number of discarded warmup calls (default 20)")
    ap.add_argument("--label",
                    help="human label stored alongside the result")
    ap.add_argument("--key",
                    help="JSON key for this result (default derived from mode)")
    args = ap.parse_args()

    if args.loopback:
        key = args.key or "loopback"
        label = args.label or "DCP loopback (in-process)"
        print(f"benchmarking {label}: {args.count} samples, {args.warmup} warmup")
        st = asyncio.run(run_loopback(args.count, args.warmup))
    else:
        key = args.key or f"uart_{args.serial.lower()}"
        label = args.label or f"DCP UART {args.baud} ({args.serial})"
        print(f"benchmarking {label}: {args.count} samples, {args.warmup} warmup")
        st = asyncio.run(run_uart(args.serial, args.baud, args.count, args.warmup))

    _print_report(key, label, st)
    _save(key, label, st)
    return 0


if __name__ == "__main__":
    sys.exit(main())
