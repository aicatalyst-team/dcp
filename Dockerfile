FROM registry.access.redhat.com/ubi9/python-312

WORKDIR /opt/app-root/src

# Copy dependency file first for layer caching
COPY pyproject.toml README.md ./
COPY src/ src/

# Install the package with mcp and dev extras (for pytest)
# Skip serial/mqtt/ble extras as they need hardware transports
RUN pip install --no-cache-dir ".[mcp,dev]"

# Copy examples and tests for PoC validation
COPY examples/ examples/
COPY tests/ tests/

# OpenShift compatibility
RUN chgrp -R 0 /opt/app-root && chmod -R g=u /opt/app-root

USER 1001

ENTRYPOINT ["dcp"]
CMD ["--help"]
