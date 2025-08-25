.PHONY: all
all: submodules fprime-venv zephyr-setup generate-if-needed build

.PHONY: help
help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Dependencies

.PHONY: submodules
submodules: ## Initialize and update git submodules
	@echo "Initializing and updating git submodules..."
	git submodule update --init --recursive

export VIRTUAL_ENV ?= $(shell pwd)/fprime-venv
fprime-venv: uv ## Create a virtual environment
	@test -s $(VIRTUAL_ENV) || { \
		echo "Creating virtual environment..."; \
		$(UV) venv fprime-venv; \
		$(UV) pip install --requirement requirements.txt; \
	}

.PHONY: zephyr-setup
zephyr-setup: uv ## Set up Zephyr environment
	@test -s lib/zephyr-workspace/tools/edtt/.gitignore || { \
		echo "Setting up Zephyr environment..."; \
		cd lib/zephyr-workspace && \
			$(UVX) west update && \
			$(UVX) west zephyr-export && \
			$(UV) run west packages pip --install && \
			$(UV) run west sdk install; \
	}

##@ Development

.PHONY: pre-commit-install
pre-commit-install: uv
	@echo "Installing pre-commit hooks..."
	@$(UVX) pre-commit install > /dev/null

.PHONY: fmt
fmt: pre-commit-install ## Lint and format files
	$(UVX) pre-commit run --all-files

.PHONY: generate
generate: submodules fprime-venv zephyr-setup ## Generate FPrime-Zephyr Proves Core Reference
	@echo "Generating FPrime-Zephyr Proves Core Reference..."
	$(UV) run fprime-util generate --force

.PHONY: generate-if-needed
BUILD_DIR ?= $(shell pwd)/build-fprime-automatic-zephyr
generate-if-needed:
	@test -s $(BUILD_DIR) || $(MAKE) generate

.PHONY: build
build: generate-if-needed ## Build FPrime-Zephyr Proves Core Reference
	@echo "Building FPrime code..."
	@$(UV) run fprime-util build

.PHONY: list-tty
list-tty: arduino-cli ## List available TTY ports
	@echo "TTY ports:"
	@$(ARDUINO_CLI) board list | grep "USB" | awk '{print $$1}'

.PHONY: install
UF2 ?= $(BUILD_DIR)/zephyr/zephyr.uf2
install: arduino-cli build ## Install the zephyr firmware onto a connected PROVES Kit, requires BOARD_DIR=[path-to-your-board]
	@$(ARDUINO_CLI) config init || true
	@$(ARDUINO_CLI) config add board_manager.additional_urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
	@$(ARDUINO_CLI) core install rp2040:rp2040@4.1.1
	@$(ARDUINO_CLI) upload -v -b 115200 --fqbn rp2040:rp2040:rpipico -p $(BOARD_DIR) -i $(UF2)

.PHONY: clean
clean: ## Remove all gitignored files
	git clean -dfX
	rm -rf lib/zephyr-workspace/bootloader lib/zephyr-workspace/modules lib/zephyr-workspace/tools

##@ Operations

.PHONY: gds
gds: ## Run FPrime GDS
	@echo "Running FPrime GDS..."
	@$(UV) run fprime-gds -n --dictionary $(BUILD_DIR)/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-baud 115200 --output-unframed-data

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

ARDUINO_CLI ?= $(BIN_DIR)/arduino-cli
.PHONY: arduino-cli
arduino-cli: $(ARDUINO_CLI) ## Download arduino-cli
$(ARDUINO_CLI): $(BIN_DIR)
	@test -s $(ARDUINO_CLI) || curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=$(BIN_DIR) sh > /dev/null
