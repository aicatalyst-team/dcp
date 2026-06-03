#!/usr/bin/env python3
"""AutoPoC Test Script for DCP (Device Context Protocol)

Tests CLI-based scenarios by examining Kubernetes Job outputs.
Uses only Python stdlib.
"""
import json
import os
import subprocess
import sys
import time

NAMESPACE = os.environ.get("POC_NAMESPACE", "autopoc-test-builds")
results = []


def run_cmd(cmd: list[str], timeout: int = 30) -> tuple[int, str, str]:
    """Run a command and return (exit_code, stdout, stderr)."""
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "Command timed out"
    except Exception as e:
        return -1, "", str(e)


def test_job_completed(job_name: str) -> tuple[bool, str]:
    """Check if a Kubernetes Job completed successfully."""
    rc, out, err = run_cmd([
        "kubectl", "get", f"job/{job_name}", "-n", NAMESPACE,
        "-o", "jsonpath={.status.succeeded}"
    ])
    if rc != 0:
        return False, f"kubectl failed: {err}"
    return out.strip() == "1", f"succeeded={out.strip()}"


def get_job_logs(job_name: str) -> str:
    """Get logs from a Kubernetes Job."""
    rc, out, err = run_cmd([
        "kubectl", "logs", f"job/{job_name}", "-n", NAMESPACE
    ], timeout=30)
    if rc != 0:
        return f"ERROR: {err}"
    return out


def test_scenario(name: str, description: str, job_name: str,
                  expected_strings: list[str] | None = None,
                  not_expected_strings: list[str] | None = None):
    """Test a CLI scenario by checking Job completion and log content."""
    start = time.time()

    # Check job completed
    completed, status_msg = test_job_completed(job_name)
    if not completed:
        results.append({
            "scenario_name": name,
            "status": "fail",
            "output": status_msg,
            "error_message": f"Job {job_name} did not complete successfully",
            "duration_seconds": round(time.time() - start, 2)
        })
        return

    # Get logs
    logs = get_job_logs(job_name)

    # Check expected strings
    missing = []
    if expected_strings:
        for s in expected_strings:
            if s not in logs:
                missing.append(s)

    # Check not-expected strings
    found_bad = []
    if not_expected_strings:
        for s in not_expected_strings:
            if s in logs:
                found_bad.append(s)

    if missing:
        results.append({
            "scenario_name": name,
            "status": "fail",
            "output": logs[:2000],
            "error_message": f"Missing expected strings: {missing}",
            "duration_seconds": round(time.time() - start, 2)
        })
    elif found_bad:
        results.append({
            "scenario_name": name,
            "status": "fail",
            "output": logs[:2000],
            "error_message": f"Found unexpected strings: {found_bad}",
            "duration_seconds": round(time.time() - start, 2)
        })
    else:
        results.append({
            "scenario_name": name,
            "status": "pass",
            "output": logs[:2000],
            "error_message": None,
            "duration_seconds": round(time.time() - start, 2)
        })


# === SCENARIOS ===

# Scenario 1: manifest-inspect
test_scenario(
    name="manifest-inspect",
    description="Run dcp inspect to parse and display lamp manifest",
    job_name="dcp-manifest-inspect",
    expected_strings=["lamp-kitchen-01", "set_brightness", "intents: 4", "events: 1"]
)

# Scenario 2: token-keygen
test_scenario(
    name="token-keygen",
    description="Generate random HMAC secret key",
    job_name="dcp-token-keygen",
    expected_strings=[]  # Just check it completed; output is a random hex string
)
# Additional check: verify output is a 64-char hex string
logs = get_job_logs("dcp-token-keygen")
token_line = logs.strip()
if len(token_line) == 64 and all(c in "0123456789abcdef" for c in token_line):
    # Already passed from job completion; update output
    for r in results:
        if r["scenario_name"] == "token-keygen":
            r["output"] = f"Generated valid 256-bit hex key: {token_line[:16]}..."
else:
    for r in results:
        if r["scenario_name"] == "token-keygen":
            r["status"] = "fail"
            r["error_message"] = f"Expected 64-char hex, got: {repr(token_line[:100])}"

# Scenario 3: lamp-demo
test_scenario(
    name="lamp-demo",
    description="Run Bridge + Simulator end-to-end demo",
    job_name="dcp-lamp-demo",
    expected_strings=[
        "valid call",
        "read it back",
        "dry-run",
        "out-of-range",
        "missing capability",
        "status='ok'",
        "status='range'",
        "status='capability_required'"
    ]
)

# Scenario 4: test-suite
test_scenario(
    name="test-suite",
    description="Run pytest test suite (104 tests)",
    job_name="dcp-test-suite",
    expected_strings=["passed"],
    not_expected_strings=["FAILED", "ERROR"]
)

# === END SCENARIOS ===

print(json.dumps({"results": results}, indent=2))
sys.exit(1 if any(r["status"] in ("fail", "error") for r in results) else 0)
