.PHONY: all
all: submodules fprime-venv zephyr generate-if-needed build

.PHONY: help
help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Dependencies

.PHONY: submodules
submodules: ## Initialize and update git submodules
	@git submodule update --init --recursive

export VIRTUAL_ENV ?= $(shell pwd)/fprime-venv
.PHONY: fprime-venv
fprime-venv: uv ## Create a virtual environment
	@$(UV) venv fprime-venv --allow-existing
	@$(UV) pip install --prerelease=allow --requirement requirements.txt

.PHONY: zephyr-setup
zephyr-setup: fprime-venv ## Set up Zephyr environment
	@test -d lib/zephyr-workspace/modules/hal/rpi_pico || test -d ../lib/zephyr-workspace/modules/hal/rpi_pico || { \
		echo "Setting up Zephyr environment..."; \
		rm -rf ../.west/ && \
		$(UVX) west init --local . && \
		$(UVX) west update && \
		$(UVX) west zephyr-export && \
		$(UV) run west packages pip --install && \
		$(UV) run west sdk install --toolchains arm-zephyr-eabi; \
	}

##@ Development

.PHONY: pre-commit-install
pre-commit-install: uv ## Install pre-commit hooks
	@$(UVX) pre-commit install > /dev/null

.PHONY: fmt
fmt: pre-commit-install ## Lint and format files
	@$(UVX) pre-commit run --all-files

.PHONY: generate
generate: submodules fprime-venv zephyr ## Generate FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util generate --force

.PHONY: generate-if-needed
BUILD_DIR ?= $(shell pwd)/build-fprime-automatic-zephyr
generate-if-needed:
	@test -d $(BUILD_DIR) || $(MAKE) generate

.PHONY: build
build: submodules zephyr fprime-venv generate-if-needed ## Build FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util build

.PHONY: test-integration
test-integration: uv
	@$(UV_RUN) pytest FprimeZephyrReference/test/int --deployment build-artifacts/zephyr/fprime-zephyr-deployment

.PHONY: bootloader
bootloader: uv
	@if [ -d "/Volumes/RP2350" ] || [ -d "/Volumes/RPI-RP2" ] || ls /media/*/RP2350 2>/dev/null || ls /media/*/RPI-RP2 2>/dev/null; then \
		echo "RP2350 already in bootloader mode - skipping trigger"; \
	else \
		echo "RP2350 not in bootloader mode - triggering bootloader"; \
		$(UV_RUN) pytest FprimeZephyrReference/test/bootloader_trigger.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment; \
	fi

.PHONY: clean
clean: ## Remove all gitignored files
	git clean -dfX

##@ Operations

GDS_COMMAND ?= $(UV_RUN) fprime-gds -n --dictionary $(ARTIFACT_DIR)/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-baud 115200 --output-unframed-data
ARTIFACT_DIR ?= $(shell pwd)/build-artifacts

.PHONY: gds
gds: ## Run FPrime GDS
	@echo "Running FPrime GDS..."
	@$(GDS_COMMAND)

.PHONY: gds-integration
gds-integration:
	@$(GDS_COMMAND) --gui=none

include lib/makelib/build-tools.mk
include lib/makelib/ci.mk
include lib/makelib/zephyr.mk
