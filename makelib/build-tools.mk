##@ Build Tools

.PHONY: download-bin
download-bin: uv

TOOLS_DIR ?= $(shell pwd)/bin
$(TOOLS_DIR):
	mkdir -p $(TOOLS_DIR)

### Tool Versions
UV_VERSION ?= 0.8.13

### Python version used for tooling environments. Single source of truth is .python-version, so
### that tools do not silently build against whichever interpreter happens to be on the machine.
PYTHON_VERSION ?= $(shell cat .python-version)

### uv & uvx
UV_DIR ?= $(TOOLS_DIR)/uv-$(UV_VERSION)
UV ?= $(UV_DIR)/uv
UVX ?= $(UV_DIR)/uvx
### Pin the interpreter for uvx-run tooling. pre-commit builds its hook environments with whatever
### Python runs it, so without this a machine defaulting to an older interpreter builds hook
### environments that cannot parse project sources written for .python-version. uv downloads the
### pinned interpreter if it is missing.
UVX_PINNED ?= $(UVX) --python $(PYTHON_VERSION)
.PHONY: uv
uv: $(UV) ## Download uv
$(UV): $(TOOLS_DIR)
	@test -s $(UV) || { mkdir -p $(UV_DIR); curl -LsSf https://astral.sh/uv/$(UV_VERSION)/install.sh | UV_UNMANAGED_INSTALL=$(UV_DIR) sh > /dev/null; }

UV_RUN ?= $(UV) run --active
