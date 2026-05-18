"""Generate the OG image (1200x630) and a slightly nicer logo SVG.

Run from anywhere:
    python docs/site/scripts/og.py

Outputs to docs/site/public/: og.png, logo.svg, favicon.svg (overwrites
the placeholder). After running, re-build the site (`npm run build`) so
dist/ picks up the new assets.
"""
from __future__ import annotations

from pathlib import Path

HERE = Path(__file__).resolve().parent.parent / "public"   # write into public/


# ---------------------------------------------------------------------------
# OG image — matplotlib, no external assets.

def make_og():
    import matplotlib.pyplot as plt
    from matplotlib.patches import FancyBboxPatch

    fig = plt.figure(figsize=(12, 6.3), dpi=100)
    ax  = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 120); ax.set_ylim(0, 63); ax.axis("off")

    # background — subtle navy radial vibe via two layered rectangles
    ax.add_patch(plt.Rectangle((0, 0), 120, 63, facecolor="#0f1d2e"))
    ax.add_patch(plt.Rectangle((0, 0), 120, 63, facecolor="#1f4e79", alpha=0.18))

    # mark on the left
    ax.add_patch(FancyBboxPatch((6, 38), 18, 18,
                                boxstyle="round,pad=0.5,rounding_size=2",
                                edgecolor="none", facecolor="#1f4e79"))
    ax.text(15, 47, "D", color="white", fontsize=72, weight="bold",
            ha="center", va="center", family="serif")

    # title
    ax.text(30, 51.5, "DCP", color="white", fontsize=64, weight="bold",
            ha="left", va="center", family="serif")
    ax.text(30, 41, "Device Context Protocol",
            color="#9fbedb", fontsize=22, weight="regular",
            ha="left", va="center", family="serif")

    # tagline
    ax.text(6, 26, "The protocol that lets LLM agents safely",
            color="white", fontsize=22, ha="left", va="center", family="serif")
    ax.text(6, 21, "control physical devices.",
            color="white", fontsize=22, ha="left", va="center", family="serif")

    # spec line
    ax.text(6, 12, "Sub-50-byte frames  ·  <16 KB MCU  ·  capability-scoped",
            color="#9fbedb", fontsize=14, ha="left", va="center",
            family="monospace", style="italic")
    ax.text(6, 6,  "github.com/device-context-protocol/dcp",
            color="#9fbedb", fontsize=12, ha="left", va="center",
            family="monospace")

    fig.savefig(HERE / "og.png", facecolor="#0f1d2e", dpi=100)
    plt.close(fig)
    print(f"wrote {HERE / 'og.png'}")


# ---------------------------------------------------------------------------
# Logo SVG — slightly more meaningful than a bare "D".

LOGO_SVG = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" fill="none">
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0%" stop-color="#1f4e79"/>
      <stop offset="100%" stop-color="#2d6ba5"/>
    </linearGradient>
  </defs>
  <!-- rounded square brand mark -->
  <rect width="64" height="64" rx="14" fill="url(#g)"/>
  <!-- "D" letterform -->
  <path d="M 18 14 L 18 50 L 32 50 C 42 50 50 42 50 32 C 50 22 42 14 32 14 Z M 26 22 L 32 22 C 38 22 42 26 42 32 C 42 38 38 42 32 42 L 26 42 Z"
        fill="white"/>
  <!-- tiny pin row below to evoke "device" — three little dots -->
  <circle cx="22" cy="56" r="1.6" fill="white" fill-opacity="0.55"/>
  <circle cx="32" cy="56" r="1.6" fill="white" fill-opacity="0.55"/>
  <circle cx="42" cy="56" r="1.6" fill="white" fill-opacity="0.55"/>
</svg>
'''


def make_logo():
    (HERE / "logo.svg").write_text(LOGO_SVG, encoding="utf-8")
    (HERE / "favicon.svg").write_text(LOGO_SVG, encoding="utf-8")
    print(f"wrote {HERE / 'logo.svg'}, {HERE / 'favicon.svg'}")


if __name__ == "__main__":
    make_og()
    make_logo()
