PYSQUARED_VERSION ?= v2.0.0-alpha-25w40a
PYSQUARED ?= git+https://github.com/proveskit/pysquared@$(PYSQUARED_VERSION)\#subdirectory=circuitpython-workspaces/flight-software
BOARD_MOUNT_POINT ?=

.PHONY: help
help: ## Display this help
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Installation

.PHONY: install
install: ## Install passthrough code to board. Usage: make install sband BOARD_MOUNT_POINT=/path/to/board OR make install lora BOARD_MOUNT_POINT=/path/to/board
	@if [ "$(filter sband lora,$(MAKECMDGOALS))" = "" ]; then \
		echo "Error: Please specify 'sband' or 'lora'"; \
		echo "Usage: make install sband BOARD_MOUNT_POINT=/path/to/board"; \
		echo "   OR: make install lora BOARD_MOUNT_POINT=/path/to/board"; \
		exit 1; \
	fi

.PHONY: sband
sband: check-board download-libraries ## Install S-band passthrough to board
	@echo "Installing S-band passthrough to $(BOARD_MOUNT_POINT)..."
	@rm -f $(BOARD_MOUNT_POINT)/code.py > /dev/null 2>&1 || true
	@rsync -avh config.json boot.py lib --exclude=".*" --exclude="__pycache__" $(BOARD_MOUNT_POINT)/ --delete --times --checksum
	@rsync -avh code-sband.py $(BOARD_MOUNT_POINT)/code.py --times --checksum
	@echo "S-band passthrough installed successfully!"

.PHONY: lora
lora: check-board download-libraries ## Install LoRa passthrough to board
	@echo "Installing LoRa passthrough to $(BOARD_MOUNT_POINT)..."
	@rm -f $(BOARD_MOUNT_POINT)/code.py > /dev/null 2>&1 || true
	@rsync -avh config.json boot.py lib --exclude=".*" --exclude="__pycache__" $(BOARD_MOUNT_POINT)/ --delete --times --checksum
	@rsync -avh code-lora.py $(BOARD_MOUNT_POINT)/code.py --times --checksum
	@echo "LoRa passthrough installed successfully!"

.PHONY: check-board
check-board:
	@if [ -z "$(BOARD_MOUNT_POINT)" ]; then \
		echo "Error: BOARD_MOUNT_POINT is not set"; \
		echo "Usage: make install sband BOARD_MOUNT_POINT=/path/to/board"; \
		echo "   OR: make install lora BOARD_MOUNT_POINT=/path/to/board"; \
		exit 1; \
	fi
	@if [ ! -d "$(BOARD_MOUNT_POINT)" ]; then \
		echo "Error: Board not found at $(BOARD_MOUNT_POINT)"; \
		echo "Please ensure your board is connected and mounted."; \
		exit 1; \
	fi

##@ Library Management

.PHONY: download-libraries
download-libraries: uv ## Download CircuitPython libraries needed for passthrough scripts
	@echo "Downloading libraries..."
	@mkdir -p lib
	@$(UV) pip install --requirement requirements.txt --target lib --no-deps --upgrade --quiet
	@$(UV) pip --no-cache install $(PYSQUARED) --target lib --no-deps --upgrade --quiet
	@rm -rf lib/*.dist-info
	@rm -rf lib/.lock
	@echo "Libraries downloaded successfully!"

CIRCUIT_PYTHON ?= ./firmware.uf2
BOARD ?=

.PHONY: circuit-python
circuit-python: ## Download Circuit Python firmware. Usage: make circuit-python BOARD={v5a|v5b|v5c|v5d}
	@if [ -z "$(BOARD)" ]; then \
		echo "Error: BOARD is not set"; \
		echo "Usage: make circuit-python BOARD={v5a|v5b|v5c|v5d}"; \
		exit 1; \
	fi
	@case "$(BOARD)" in \
		v5a|v5b|v5c|v5d) ;; \
		*) echo "Error: Invalid board '$(BOARD)'. Must be one of: v5a, v5b, v5c, v5d"; exit 1;; \
	esac
	@echo "Downloading Circuit Python firmware for board $(BOARD)..."
	@curl -o $(CIRCUIT_PYTHON) -fsSL https://raw.githubusercontent.com/proveskit/flight_controller_board/main/Firmware/FC_FIRM_$(BOARD)_V1.uf2
	@echo "Firmware downloaded successfully to $(CIRCUIT_PYTHON)"

.PHONY: clean
clean: ## Remove downloaded libraries
	@echo "Cleaning libraries..."
	@rm -rf lib
	@echo "Clean complete!"

##@ Build Tools

TOOLS_DIR ?= tools
$(TOOLS_DIR):
	@mkdir -p $(TOOLS_DIR)

### Tool Versions
UV_VERSION ?= 0.7.13

UV_DIR ?= $(TOOLS_DIR)/uv-$(UV_VERSION)
UV ?= $(UV_DIR)/uv
UVX ?= $(UV_DIR)/uvx
.PHONY: uv
uv: $(UV) ## Download uv
$(UV): $(TOOLS_DIR)
	@test -s $(UV) || { mkdir -p $(UV_DIR); curl -LsSf https://astral.sh/uv/$(UV_VERSION)/install.sh | UV_INSTALL_DIR=$(UV_DIR) sh > /dev/null; }
