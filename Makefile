.PHONY: all
all: submodules fprime-venv zephyr-setup generate-if-needed build

.PHONY: help
help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Dependencies
# Note: Zephyr setup uses minimal modules and ARM-only toolchain for RP2040/RP2350
# This saves ~3-4 GB compared to full installation. See docs/additional-resources/west-manifest-setup.md
# Hello World!

.PHONY: submodules
submodules: ## Initialize and update git submodules
	@echo "Initializing and updating git submodules..."
	git submodule update --init --recursive

export VIRTUAL_ENV ?= $(shell pwd)/fprime-venv
fprime-venv: ## Create a virtual environment
		@$(MAKE) uv
		@echo "Creating virtual environment..."
		@$(UV) venv fprime-venv
		@$(UV) pip install --requirement requirements.txt

.PHONY: zephyr-setup
zephyr-setup: fprime-venv ## Set up Zephyr environment (minimal RP2040/RP2350 only)
	@test -d lib/zephyr-workspace/modules/hal/rpi_pico || test -d ../lib/zephyr-workspace/modules/hal/rpi_pico || { \
		echo "Setting up minimal Zephyr environment (RP2040/RP2350 only)..."; \
		echo "  - Using minimal module set (~80% disk space reduction)"; \
		echo "  - Installing ARM toolchain only (~92% SDK reduction)"; \
		if [ ! -f .west/config ] && [ ! -f ../.west/config ]; then \
			echo "  - Initializing West workspace..."; \
			$(UVX) west init -l .; \
		fi && \
		$(UVX) west update && \
		$(UVX) west zephyr-export && \
		$(UV) run west packages pip --install && \
		$(UV) run west sdk install --toolchains arm-zephyr-eabi; \
	}

.PHONY: zephyr-setup-full
zephyr-setup-full: fprime-venv ## Set up Zephyr with ALL modules and toolchains (not recommended)
	@echo "Setting up FULL Zephyr environment (all modules and toolchains)..."
	@echo "Warning: This will download 4-6 GB of data"
	@read -p "Continue? [y/N] " -n 1 -r; \
	echo; \
	if [[ $$REPLY =~ ^[Yy]$$ ]]; then \
		$(UVX) west config manifest.path lib/zephyr-workspace/zephyr && \
		$(UVX) west update && \
		$(UVX) west zephyr-export && \
		$(UV) run west packages pip --install && \
		$(UV) run west sdk install; \
	else \
		echo "Cancelled. Use 'make zephyr-setup' for minimal installation."; \
	fi

##@ Development

.PHONY: pre-commit-install
pre-commit-install: uv ## Install pre-commit hooks
	@$(UVX) pre-commit install > /dev/null

.PHONY: fmt
fmt: pre-commit-install ## Lint and format files
	@$(UVX) pre-commit run --all-files

.PHONY: generate
generate: submodules fprime-venv zephyr-setup ## Generate FPrime-Zephyr Proves Core Reference
	@echo "Generating FPrime-Zephyr Proves Core Reference..."
	@$(UV) run fprime-util generate --force

.PHONY: generate-if-needed
BUILD_DIR ?= $(shell pwd)/build-fprime-automatic-zephyr
generate-if-needed:
	@test -s $(BUILD_DIR) || $(MAKE) generate

.PHONY: generate-lite
generate-lite: fprime-venv ## Generate FPrime-Zephyr Proves Core Reference (lite)
	@echo "Generating..."
	@$(UV) run fprime-util generate --force

.PHONY: build
build: submodules zephyr-setup fprime-venv generate-if-needed ## Build FPrime-Zephyr Proves Core Reference
	@echo "Building..."
	@$(UV) run fprime-util build

.PHONY: build-lite
build-lite: fprime-venv generate-lite ## Build FPrime-Zephyr Proves Core Reference (lite)
	@echo "Building..."
	@$(UV) run fprime-util build

.PHONY: clean
clean: ## Remove all gitignored files
	git clean -dfX

.PHONY: clean-zephyr
clean-zephyr: ## Remove all Zephyr build files
	rm -rf lib/zephyr-workspace/bootloader lib/zephyr-workspace/modules lib/zephyr-workspace/tools

.PHONY: clean-zephyr-sdk
clean-zephyr-sdk: ## Remove Zephyr SDK (reinstall with 'make zephyr-setup')
	@echo "Removing Zephyr SDK..."
	rm -rf ~/zephyr-sdk-*
	@echo "Run 'make zephyr-setup' to reinstall with minimal ARM-only toolchain"

##@ Operations

.PHONY: gds
ARTIFACT_DIR ?= $(shell pwd)/build-artifacts
gds: ## Run FPrime GDS
	@echo "Running FPrime GDS..."
	@$(UV) run fprime-gds -n --dictionary $(ARTIFACT_DIR)/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-baud 115200 --output-unframed-data

##@ Build Tools
BIN_DIR ?= $(shell pwd)/bin
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

### Tool Versions
UV_VERSION ?= 0.8.13

UV_DIR ?= $(BIN_DIR)/uv-$(UV_VERSION)
UV ?= $(UV_DIR)/uv
UVX ?= $(UV_DIR)/uvx
.PHONY: uv
uv: $(UV) ## Download uv
$(UV): $(BIN_DIR)
	@test -s $(UV) || { mkdir -p $(UV_DIR); curl -LsSf https://astral.sh/uv/$(UV_VERSION)/install.sh | UV_INSTALL_DIR=$(UV_DIR) sh > /dev/null; }
