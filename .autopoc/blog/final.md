# DCP on OpenShift: Bridging LLM Agents to IoT Devices via MCP

## TL;DR

We containerized and deployed the Device Context Protocol (DCP) Python SDK on
OpenShift using a UBI 9 base image. DCP provides a compact wire protocol for
LLM agents to safely control physical devices -- from smart lamps to motor
controllers -- through an MCP bridge server. All 104 unit tests and 4 PoC
scenarios passed in the OpenShift environment, proving the SDK works seamlessly
in enterprise container platforms.

## What is DCP?

The [Device Context Protocol](https://github.com/device-context-protocol/dcp)
is a protocol that lets LLM agents safely control physical devices, down to
dollar-class microcontrollers. While MCP (Model Context Protocol) excels at
connecting LLMs to SaaS tools via JSON-RPC, it assumes too much for
resource-constrained hardware -- 32 KB of RAM cannot handle WebSocket
connections and runtime tool discovery.

DCP solves this by keeping MCP's mental model (manifest + tool calls) but
compiling to a compact CBOR wire format with sub-50-byte frames. A reference
Bridge translates DCP to MCP, so any MCP-compatible LLM host works out of the
box.

### Key Design Principles

- **Intent, not register:** `set_brightness(50%)`, not `write_pwm(pin=5, duty=128)`
- **Units in the protocol:** Every number declares a unit -- no ambiguity
- **Safety lives in the Bridge:** HMAC capability tokens, range/type enforcement
- **Transport-agnostic:** UART, BLE, MQTT, USB-CDC, WebSocket -- one frame format

## Why Run DCP on OpenShift?

DCP's MCP bridge server (`dcp serve`) is the natural deployment target for
cloud-hosted LLM agent workflows. When an MCP host like Claude Desktop or an
IDE assistant needs to control physical devices, the Bridge becomes a
server-side component that:

1. Enforces safety checks before any command reaches hardware
2. Manages capability tokens for multi-tenant access
3. Translates between the LLM's MCP calls and the device's DCP wire frames

Running this on OpenShift brings enterprise-grade reliability, RBAC, and
observability to IoT device control -- critical for production deployments.

## The PoC Pipeline

We ran DCP through our automated 11-phase PoC pipeline:

### Containerization

DCP is a pure Python project distributed as `pydcp` on PyPI. We built it on a
`ubi9/python-312` base image:

```dockerfile
FROM registry.access.redhat.com/ubi9/python-312

WORKDIR /opt/app-root/src

COPY pyproject.toml README.md ./
COPY src/ src/

RUN pip install --no-cache-dir ".[mcp,dev]"

COPY examples/ examples/
COPY tests/ tests/

USER 0
RUN chgrp -R 0 /opt/app-root && chmod -R g=u /opt/app-root

USER 1001

ENTRYPOINT ["dcp"]
CMD ["--help"]
```

One lesson learned: UBI Python images run as UID 1001 by default, so files
copied via `COPY` inherit that ownership. The `chgrp -R 0` command needs `USER 0`
first, then switch back to `USER 1001` for OpenShift's arbitrary UID support.

### Build and Deploy

The image was built using OpenShift's internal build system (`oc new-build --binary`)
and pushed to the internal registry. Since DCP is a CLI/SDK tool (no HTTP port),
we deployed 4 Kubernetes Jobs, one per test scenario.

### Test Results

| Scenario | Result | What It Proves |
|---|---|---|
| Manifest Inspect | PASS | CLI parses YAML manifests correctly in container |
| Token Keygen | PASS | HMAC-SHA256 crypto operations work on UBI |
| Lamp Demo | PASS | Full Bridge + Simulator loop: call, read, dry-run, safety enforcement |
| Test Suite | PASS | 104/108 tests pass (4 skipped: BLE/MQTT hardware deps) |

The lamp demo is particularly compelling -- it exercises the entire DCP flow
in a single process:

```
Claude --> MCP --> dcp serve --> Bridge --> Loopback --> GenericSimulator
```

The Bridge validates capability tokens, enforces parameter ranges, and supports
dry-run (predict result without side effects). All of this worked identically
in the OpenShift container as on a developer laptop.

## What We Learned

### MCP Stdio Transport Limitation

DCP's MCP server uses stdio transport (stdin/stdout), designed for subprocess
spawning by MCP hosts. This means it cannot be deployed as a Kubernetes
Deployment with a Service -- there is no HTTP endpoint to proxy. For production
OpenShift deployments, adding an HTTP or SSE transport would enable standard
Kubernetes networking.

### Safety Architecture Works Anywhere

The most impressive aspect of DCP is that safety enforcement is deterministic
and runs entirely in the Bridge process. No external service, no GPU, no model
inference. This means the safety guarantees are identical whether running on a
developer laptop, in a container, or on bare metal. The container boundary
adds an extra layer of isolation without affecting the protocol.

### Minimal Resource Footprint

DCP's entire test suite runs in 0.28 seconds on a small (256Mi RAM) pod.
The container image includes the MCP SDK, CBOR2, and PyYAML -- no heavyweight
ML frameworks. This makes it practical for edge and resource-constrained
OpenShift deployments.

## Relevance to Red Hat AI

DCP maps to the **Agentic AI** strategy area. As enterprises build LLM-driven
automation that extends beyond SaaS APIs into physical operations (manufacturing,
energy, smart buildings), protocols like DCP become the "last mile" connection.

The MCP bridge pattern demonstrated here is directly applicable to:

- **AI Hub / GenAI Studio:** DCP intents could appear as tools in an agent
  orchestration UI
- **Llama Stack agents:** DCP bridge as a tool provider for physical device
  control
- **TrustyAI:** Capability tokens and range enforcement provide built-in
  guardrails for hardware safety

## Try It Yourself

```bash
pip install "pydcp[mcp]"
dcp inspect examples/lamp_manifest.yaml
dcp serve examples/lamp_manifest.yaml --simulator
```

Then point any MCP host at the `dcp serve` process and ask the LLM to
"set the lamp to 60% brightness."

---

*This PoC was run automatically by the AutoPoC pipeline on OpenShift AI.
Source: [device-context-protocol/dcp](https://github.com/device-context-protocol/dcp)
| Fork: [aicatalyst-team/dcp](https://github.com/aicatalyst-team/dcp)*
