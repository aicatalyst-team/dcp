# PoC Plan: DCP (Device Context Protocol)

## Project Classification

- **Type:** infrastructure (Python SDK/protocol library with CLI and MCP server)
- **Key Technologies:** Python 3.11+, CBOR (cbor2), PyYAML, MCP SDK, asyncio
- **ODH Relevance:** Demonstrates MCP tool connectivity for agentic AI workflows.
  DCP bridges LLM agents to IoT devices via MCP, directly relevant to
  OpenShift AI's agentic AI strategy with MCP support.

## PoC Objectives

1. Prove that the DCP Python SDK installs and runs correctly in a UBI-based
   container on OpenShift
2. Validate the `dcp inspect` CLI command parses manifests correctly
3. Validate the `dcp token keygen` command generates HMAC keys
4. Run the `lamp_demo.py` example to exercise the Bridge + Simulator
   loopback transport end-to-end
5. Run the project's test suite (`pytest`) to confirm all 88 tests pass
   in the containerized environment

## Infrastructure Requirements

- **Resource Profile:** small (256Mi RAM, 250m CPU)
- **GPU Required:** No
- **Persistent Storage:** None
- **Sidecar Containers:** None

## Deployment Model

- **deployment_model:** job
- **listens_on_port:** false
- **long_running:** false

DCP's MCP server uses stdio transport (stdin/stdout), not HTTP. It is
designed to be launched by an MCP host (like Claude Desktop) as a subprocess.
There is no HTTP endpoint to test. The correct PoC approach is to run CLI
commands and the demo script as Kubernetes Jobs.

## Test Scenarios

### Scenario 1: manifest-inspect

- **Description:** Run `dcp inspect examples/lamp_manifest.yaml` to verify
  the CLI parses and displays a manifest summary
- **Type:** cli
- **Input:** `dcp inspect examples/lamp_manifest.yaml`
- **Expected:** Exit code 0, output contains "lamp-kitchen-01" and "set_brightness"
- **Timeout:** 30 seconds

### Scenario 2: token-keygen

- **Description:** Run `dcp token keygen` to generate a random HMAC secret
- **Type:** cli
- **Input:** `dcp token keygen`
- **Expected:** Exit code 0, output is a 64-character hex string
- **Timeout:** 15 seconds

### Scenario 3: lamp-demo

- **Description:** Run `python examples/lamp_demo.py` which exercises the
  full Bridge + Simulator + LoopbackTransport flow with valid calls,
  dry-run, out-of-range rejection, and capability enforcement
- **Type:** cli
- **Input:** `python examples/lamp_demo.py`
- **Expected:** Exit code 0, output contains "valid call", "dry-run",
  "out-of-range", "missing capability"
- **Timeout:** 30 seconds

### Scenario 4: test-suite

- **Description:** Run `pytest tests/ -v --timeout=60` to execute the
  project's full test suite (88 tests) in the container
- **Type:** cli
- **Input:** `pytest tests/ -v`
- **Expected:** Exit code 0, all tests pass
- **Timeout:** 120 seconds

## Dockerfile Considerations

- Base image: `registry.access.redhat.com/ubi9/python-312`
- Install with: `pip install --no-cache-dir ".[mcp,dev]"`
  (serial extra requires pyserial-asyncio which needs no system deps;
  skip BLE as bleak is Linux-only desktop; include dev for pytest)
- No EXPOSE needed (no HTTP port)
- Entry point: `dcp` CLI or `python -m pytest`
- Copy examples/ directory for lamp demo and manifests

## Deployment Considerations

- Deploy as Kubernetes Jobs (not Deployments) since this is a CLI/SDK tool
- Each test scenario maps to a separate Job
- No Service needed (no ports)
- All Jobs run in `autopoc-test-builds` namespace to avoid image pull RBAC issues
