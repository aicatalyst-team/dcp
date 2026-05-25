"""LVGL-visible DCP demo — drives the smart_panel firmware's on-screen widgets.

Sends display_text + set_color + clear_screen + set_backlight intents over
DCP/UART and prints status. Watch the T-Panel screen: it should show text
changes line-by-line and a color rectangle at the bottom.

Usage:
    python tools/demo_lvgl_via_dcp.py [COM_PORT]
"""
from __future__ import annotations

import asyncio
import sys
from pathlib import Path

try:
    sys.stdout.reconfigure(encoding="utf-8")
except Exception:
    pass

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "src"))

from dcp.bridge import Bridge
from dcp.manifest import Manifest
from dcp.transports.uart import UartTransport


async def main(port: str) -> int:
    print(f"\n=== DCP -> LVGL demo against {port} ===\n")
    manifest = Manifest.load(ROOT / "examples" / "smart_panel_manifest.yaml")
    print(f"manifest: {manifest.device_id}  ({len(manifest.intents)} intents)")

    uart = UartTransport(port, baud=115200)
    await uart.open()
    bridge = Bridge(
        manifest,
        uart,
        granted_capabilities={"panel.write", "panel.read"},
        timeout=3.0,
    )
    await bridge.start()
    print("bridge up\n")

    async def call(label, intent, params=None, dry=False):
        try:
            r = await bridge.call(intent, params, dry_run=dry)
            mark = "[OK]" if r.status == "ok" else f"[{r.status}]"
            print(f"  {mark:8s} {label:30s} -> {r.data}")
        except Exception as e:
            print(f"  [EXC]    {label:30s} -> {e!r}")
        await asyncio.sleep(0.6)   # let viewer see each step on screen

    # Sequence designed to be visible on the panel:
    await call("backlight 100%",     "set_backlight", {"level": 100})
    await call("text line 0 hello",  "display_text",  {"line": 0, "text": "Hello from DCP"})
    await call("text line 1 model",  "display_text",  {"line": 1, "text": "model: T-Panel S3"})
    await call("text line 2 latency","display_text",  {"line": 2, "text": "median 15.6 ms"})
    await call("color RED",          "set_color",     {"r": 255, "g": 0, "b": 0})
    await asyncio.sleep(1.2)
    await call("color GREEN",        "set_color",     {"r": 0, "g": 200, "b": 0})
    await asyncio.sleep(1.2)
    await call("color BLUE",         "set_color",     {"r": 0, "g": 80, "b": 255})
    await asyncio.sleep(1.2)
    await call("text line 3 ok",    "display_text",  {"line": 3, "text": "DCP+LVGL works"})
    await asyncio.sleep(2.0)
    await call("dry-run set_color",  "set_color",     {"r": 128, "g": 128, "b": 128}, dry=True)
    await call("clear_screen",       "clear_screen")
    await asyncio.sleep(1.0)
    await call("text line 0 done",   "display_text",  {"line": 0, "text": "demo complete"})

    await bridge.stop()
    print("\n=== done ===")
    return 0


if __name__ == "__main__":
    port = sys.argv[1] if len(sys.argv) > 1 else "COM6"
    sys.exit(asyncio.run(main(port)))
