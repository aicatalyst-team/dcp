# Paper figures

All paper figures are generated programmatically by `make_figures.py`.
This keeps them under version control as source, reproducible, and easy to
update when the measurement numbers move from synthetic to measured.

## Regenerate

```powershell
python -m pip install matplotlib numpy   # only deps
python make_figures.py
```

Outputs both PDF (for the LaTeX `\includegraphics`) and PNG (for the
README / web preview) into this directory.

## What each figure shows

| File | Figure | Purpose |
|---|---|---|
| `arch.pdf` | Fig. 1 | High-level architecture, expanded Bridge components |
| `wire_format.pdf` | Fig. 2 | DCP frame byte layout + wire-size vs other protocols |
| `footprint.pdf` | Fig. 3 | Flash & RAM comparison: DCP vs IoT-MCP vs raw MCP vs Matter |
| `hallucination.pdf` | Fig. 4 | What % of malformed/adversarial LLM calls each protocol catches |
| `latency.pdf` | Fig. 5 | End-to-end latency broken down per transport |

## What's synthetic vs measured

- **Fig. 1 (arch), Fig. 2 (wire format)** — exact. The diagram and the
  byte counts are derived from the protocol itself.
- **Fig. 5 (latency)** — **measured.** `fig_latency()` reads
  `latency_data.json`, produced by `tools/bench_latency.py`: 1000 timed
  round-trips per transport, median + IQR. Currently covers loopback,
  ESP32-WROOM-32 (CH340), and ESP32-S3 (native USB).
- **Fig. 3 (footprint)** — partially synthetic. The DCP bar is a design
  target; the IoT-MCP / Direct-MCP / Matter bars are cited or typical.
  Needs a rework: the DCP layer is now measured (~14 KB over an empty
  sketch) and the figure should distinguish measured-DCP from cited
  baselines cleanly, including the flash-vs-RAM axis.
- **Fig. 4 (hallucination)** — synthetic, and labelled as such in the
  figure footnote. Making it real needs an LLM adversarial-call
  benchmark (≈1000 generated calls per baseline, with a defined
  attack-generation procedure) — this is the v0.4 paper campaign.

To re-measure latency: connect the board, run
`python tools/bench_latency.py --serial <PORT> --label "..." --key <key>`,
then `python make_figures.py`.

Edit `make_figures.py` directly — each figure is one function. Re-run.
Don't hand-edit the PDFs.
