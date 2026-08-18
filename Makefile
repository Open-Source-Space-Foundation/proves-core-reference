.PHONY: all
all: submodules fprime-venv zephyr generate-if-needed build

.PHONY: help
help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Dependencies

.PHONY: submodules
submodules: ## Initialize and update git submodules
	@git submodule foreach --recursive 'git checkout -- . && git clean -fd' || true
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
		$(UVX) west update && \
		$(UVX) west zephyr-export && \
		$(UV) run west packages pip --install && \
		$(UV) run west sdk install --toolchains arm-zephyr-eabi && \
		$(UV) pip install --prerelease=allow -r lib/zephyr-workspace/bootloader/mcuboot/zephyr/requirements.txt; \
	}

##@ Development

.PHONY: pre-commit-install
pre-commit-install: uv ## Install pre-commit hooks
	@$(UVX) pre-commit install > /dev/null

.PHONY: fmt
fmt: pre-commit-install ## Lint and format files
	@$(UVX) pre-commit run --all-files

.PHONY: data-budget
data-budget: fprime-venv ## Analyze telemetry data budget (use VERBOSE=1 for detailed output)
	@$(UV_RUN) python3 tools/data_budget.py $(if $(VERBOSE),--verbose,)

.PHONY: data-diagram
data-diagram: fprime-venv ## Generate Mermaid packet wire diagrams as Markdown (use OUTPUT=file.md to save to a file)
	@$(UV_RUN) python3 tools/data_budget.py --diagram $(if $(OUTPUT),--output $(OUTPUT),)

##@ Documentation

.PHONY: docs-sync
docs-sync: ## Sync SDD files from components to docs-site
	@echo "Syncing SDD files to docs-site/components..."
	@mkdir -p docs-site/components/img
	@# Copy ADCS
	@cp PROVESFlightControllerReference/Components/ADCS/docs/sdd.md docs-site/components/ADCS.md
	@# Copy Communication Components
	@cp PROVESFlightControllerReference/Components/AmateurRadio/docs/sdd.md docs-site/components/AmateurRadio.md
	@cp PROVESFlightControllerReference/Components/SBand/docs/sdd.md docs-site/components/SBand.md
	@cp PROVESFlightControllerReference/ComCcsdsUart/docs/sdd.md docs-site/components/ComCcsdsUart.md
	@cp PROVESFlightControllerReference/ComCcsdsSband/docs/sdd.md docs-site/components/ComCcsdsSband.md
	@cp PROVESFlightControllerReference/ComCcsdsLora/docs/sdd.md docs-site/components/ComCcsdsLora.md
	@cp PROVESFlightControllerReference/Components/PayloadCom/docs/sdd.md docs-site/components/PayloadCom.md
	@cp PROVESFlightControllerReference/Components/ComDelay/docs/sdd.md docs-site/components/ComDelay.md
	@# Copy Core Components
	@cp PROVESFlightControllerReference/Components/ModeManager/docs/sdd.md docs-site/components/ModeManager.md
	@cp PROVESFlightControllerReference/Components/StartupManager/docs/sdd.md docs-site/components/StartupManager.md
	@cp PROVESFlightControllerReference/Components/ResetManager/docs/sdd.md docs-site/components/ResetManager.md
	@cp PROVESFlightControllerReference/Components/Watchdog/docs/sdd.md docs-site/components/Watchdog.md
	@cp PROVESFlightControllerReference/Components/BootloaderTrigger/docs/sdd.md docs-site/components/BootloaderTrigger.md
	@cp PROVESFlightControllerReference/Components/DetumbleManager/docs/sdd.md docs-site/components/DetumbleManager.md
	@# Copy Hardware Components
	@cp PROVESFlightControllerReference/Components/AntennaDeployer/docs/sdd.md docs-site/components/AntennaDeployer.md
	@cp PROVESFlightControllerReference/Components/Burnwire/docs/sdd.md docs-site/components/Burnwire.md
	@cp PROVESFlightControllerReference/Components/CameraHandler/docs/sdd.md docs-site/components/CameraHandler.md
	@cp PROVESFlightControllerReference/Components/LoadSwitch/docs/sdd.md docs-site/components/LoadSwitch.md
	@# Copy Sensor Components
	@cp PROVESFlightControllerReference/Components/ImuManager/docs/sdd.md docs-site/components/ImuManager.md
	@cp PROVESFlightControllerReference/Components/PowerMonitor/docs/sdd.md docs-site/components/PowerMonitor.md
	@cp PROVESFlightControllerReference/Components/ThermalManager/docs/sdd.md docs-site/components/ThermalManager.md
	@# Copy Driver Components
	@cp PROVESFlightControllerReference/Components/Drv/Drv2605Manager/docs/sdd.md docs-site/components/Drv2605Manager.md
	@cp PROVESFlightControllerReference/Components/Drv/Ina219Manager/docs/sdd.md docs-site/components/Ina219Manager.md
	@cp PROVESFlightControllerReference/Components/Drv/RtcManager/docs/sdd.md docs-site/components/RtcManager.md
	@cp PROVESFlightControllerReference/Components/Drv/Tmp112Manager/docs/sdd.md docs-site/components/Tmp112Manager.md
	@cp PROVESFlightControllerReference/Components/Drv/Veml6031Manager/docs/sdd.md docs-site/components/Veml6031Manager.md
	@# Copy Storage Components
	@cp PROVESFlightControllerReference/Components/FlashWorker/docs/sdd.md docs-site/components/FlashWorker.md
	@cp PROVESFlightControllerReference/Components/FsFormat/docs/sdd.md docs-site/components/FsFormat.md
	@cp PROVESFlightControllerReference/Components/FsSpace/docs/sdd.md docs-site/components/FsSpace.md
	@cp PROVESFlightControllerReference/Components/NullPrmDb/docs/sdd.md docs-site/components/NullPrmDb.md
	@# Copy Security Components
	@cp PROVESFlightControllerReference/Components/TcSecurityDeframer/docs/sdd.md docs-site/components/TcSecurityDeframer.md
	@cp PROVESFlightControllerReference/Components/ProvesRouter/docs/sdd.md docs-site/components/ProvesRouter.md
	@# Copy images
	@find PROVESFlightControllerReference -path "*/docs/img/*" -type f -exec cp {} docs-site/components/img/ \; 2>/dev/null || true
	@echo "✓ Synced 32 component SDDs and images"

.PHONY: docs-serve
docs-serve: uv ## Serve MkDocs documentation site locally
	@echo "Starting MkDocs server at http://127.0.0.1:8000"
	@$(UVX) --from mkdocs-material mkdocs serve

.PHONY: docs-build
docs-build: uv ## Build MkDocs documentation site
	@$(UVX) --from mkdocs-material mkdocs build

.PHONY: generate
generate: submodules fprime-venv zephyr generate-auth-key keys/proves.pem ## Generate FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util generate --force

.PHONY: generate-if-needed
BUILD_DIR ?= $(shell pwd)/build-fprime-automatic-zephyr
generate-if-needed:
	@test -d $(BUILD_DIR) || $(MAKE) generate

.PHONY: build
build: submodules zephyr fprime-venv generate-if-needed ## Build FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util build
	./tools/bin/make-loadable-image ./build-artifacts/zephyr.signed.bin bootable.uf2
	mv ./build-artifacts/zephyr.signed.hex bootable.signed.hex

.PHONY: check-console-disabled
ZEPHYR_CONFIG ?= $(BUILD_DIR)/zephyr/.config
check-console-disabled: uv ## Fail if the Zephyr UART console is enabled (it corrupts the F' downlink); run after 'make build'
	@$(UV_RUN) python3 scripts/check_console_disabled.py "$(ZEPHYR_CONFIG)"

##@ Authentication Keys

AUTH_DEFAULT_KEY_HEADER ?= PROVESFlightControllerReference/Components/TcSecurityDeframer/AuthDefaultKey.h
AUTH_KEY_TEMPLATE ?= scripts/generate_auth_default_key.h

.PHONY: generate-auth-key
generate-auth-key: ## Generate AuthDefaultKey.h with a random HMAC key
	@if [ -f "$(AUTH_DEFAULT_KEY_HEADER)" ]; then \
		echo "$(AUTH_DEFAULT_KEY_HEADER) already exists. Skipping generation."; \
	else \
		echo "Generating $(AUTH_DEFAULT_KEY_HEADER) with random key..."; \
		$(UV_RUN) python3 scripts/generate_auth_key_header.py --output $(AUTH_DEFAULT_KEY_HEADER) --template $(AUTH_KEY_TEMPLATE); \
	fi
	@echo "Generated $(AUTH_DEFAULT_KEY_HEADER)"

keys/proves.pem:
	@mkdir -p keys
	@cp lib/zephyr-workspace/bootloader/mcuboot/root-rsa-2048.pem keys/proves.pem

SYSBUILD_PATH ?= $(shell pwd)/lib/zephyr-workspace/zephyr/samples/sysbuild/with_mcuboot
.PHONY: build-mcuboot
build-mcuboot: submodules zephyr fprime-venv
	@cp $(shell pwd)/bootloader/sysbuild.conf $(SYSBUILD_PATH)/sysbuild.conf

	$(UV_RUN) $(shell pwd)/tools/bin/build-with-proves $(SYSBUILD_PATH) --sysbuild
	mv $(shell pwd)/build/with_mcuboot/zephyr/zephyr.uf2 $(shell pwd)/mcuboot.uf2
	mv $(shell pwd)/build/mcuboot/zephyr/zephyr.elf $(shell pwd)/mcuboot.elf

##@ Debugging / OpenOCD

OPENOCD_DIR ?= $(shell pwd)/tools/openocd
OPENOCD_REPO ?= https://github.com/raspberrypi/openocd.git
OPENOCD_REF ?= acff23f
OPENOCD_BIN ?= $(OPENOCD_DIR)/src/openocd
OPENOCD_JOBS ?= 4
OPENOCD_FLASH_SPEED ?= 5000
OPENOCD_COMMON_FLAGS ?= -s $(OPENOCD_DIR)/tcl -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed $(OPENOCD_FLASH_SPEED)"

$(OPENOCD_DIR)/.built:
	@if [ ! -d "$(OPENOCD_DIR)" ]; then \
		git clone "$(OPENOCD_REPO)" "$(OPENOCD_DIR)"; \
	fi
	@cd "$(OPENOCD_DIR)" && \
		git checkout "$(OPENOCD_REF)" || { echo "Failed to checkout $(OPENOCD_REF)"; exit 1; } && \
		./bootstrap && \
		./configure --disable-werror --enable-cmsis-dap --enable-cmsis-dap-v2 && \
		$(MAKE) -j$(OPENOCD_JOBS)
	@touch "$(OPENOCD_DIR)/.built"

.PHONY: debug
debug: $(OPENOCD_DIR)/.built ## Run OpenOCD against the debug probe and stream board debug output
	@"$(OPENOCD_BIN)" $(OPENOCD_COMMON_FLAGS)

.PHONY: debug-install
debug-install: $(OPENOCD_DIR)/.built ## Flash a file via SWD with OpenOCD. Usage: make debug-install <filename>
	@TARGET_FILE="$(firstword $(filter-out $@,$(MAKECMDGOALS)))"; \
	if [ -z "$$TARGET_FILE" ]; then \
		echo "Usage: make debug-install <filename>"; \
		exit 1; \
	fi; \
	if [ ! -f "$$TARGET_FILE" ]; then \
		echo "File not found: $$TARGET_FILE"; \
		exit 1; \
	fi; \
	"$(OPENOCD_BIN)" $(OPENOCD_COMMON_FLAGS) -c "program $$TARGET_FILE verify reset exit"

test-unit: ## Run unit tests
	cmake -S PROVESFlightControllerReference/test/unit-tests -B build-gtest -DBUILD_TESTING=ON
	cmake --build build-gtest
	ctest --test-dir build-gtest

FILTER ?= not sync_sequence_number and not format_filesystem

.PHONY: test-integration
test-integration: uv ## Run integration tests (set TEST=<name|file.py> or pass test targets)
	@DEPLOY="build-artifacts/zephyr/fprime-zephyr-deployment"; \
	TARGETS=""; \
	if [ -n "$(TEST)" ]; then \
		case "$(TEST)" in \
			*.py) TARGETS="PROVESFlightControllerReference/test/int/$(TEST)" ;; \
			*) TARGETS="PROVESFlightControllerReference/test/int/$(TEST).py" ;; \
		esac; \
		[ -e "$$TARGETS" ] || { echo "Specified test file $$TARGETS not found"; exit 1; }; \
	elif [ -n "$(filter-out $@,$(MAKECMDGOALS))" ]; then \
		for test in $(filter-out $@,$(MAKECMDGOALS)); do \
			case "$$test" in \
				*.py) TARGETS="$$TARGETS PROVESFlightControllerReference/test/int/$$test" ;; \
				*) TARGETS="$$TARGETS PROVESFlightControllerReference/test/int/$${test}_test.py" ;; \
			esac; \
		done; \
	else \
		TARGETS="PROVESFlightControllerReference/test/int"; \
	fi; \
	echo "Running integration tests: $$TARGETS"; \
	$(UV_RUN) pytest $$TARGETS --deployment $$DEPLOY -m "$(FILTER)" $(PYTEST_ARGS)

# Allow test names to be passed as targets without Make trying to execute them
%:
	@:

.PHONY: sync-sequence-number
sync-sequence-number: uv ## Synchronize GDS/flight sequence number
	@echo "Synchronizing sequence number"
	@$(UV_RUN) pytest PROVESFlightControllerReference/test/int/sync_sequence_number_test.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment

.PHONY: test-interactive
test-interactive: fprime-venv ## Run interactive test selection (set ARGS for CLI mode, e.g., ARGS="--all --cycles 10")
	@$(UV_RUN) python PROVESFlightControllerReference/test/run_interactive_tests.py $(ARGS)

.PHONY: clean
clean: ## Remove all gitignored files
	git clean -dfX

##@ YAMCS

YAMCS_INPUT_DIR ?= ../yamcs-stack/server/inputs/proves

.PHONY: yamcs-export
yamcs-export: ## Export the dictionary and matching HMAC key for yamcs-stack/server
	@python3 scripts/export_yamcs_bundle.py \
	  --dictionary-root build-artifacts \
	  --auth-header $(AUTH_DEFAULT_KEY_HEADER) \
	  --output-dir $(YAMCS_INPUT_DIR)

.PHONY: test-export-bundle
test-export-bundle: ## Test the standalone Yamcs bundle exporter
	@python3 -m unittest scripts.tests.test_export_yamcs_bundle

##@ Operations

GDS_COMMAND ?= $(UV_RUN) fprime-gds

ARTIFACT_DIR ?= $(shell pwd)/build-artifacts

.PHONY: sequence
sequence: fprime-venv ## Compile a sequence file (usage: make sequence SEQ=startup)
	@if [ -z "$(SEQ)" ]; then \
		echo "Error: SEQ variable not set. Usage: make sequence SEQ=startup"; \
		exit 1; \
	fi
	@echo "Compiling sequence: $(SEQ).seq"
	@$(UV_RUN) fprime-seqgen sequences/$(SEQ).seq -d $(ARTIFACT_DIR)/zephyr/fprime-zephyr-deployment

.PHONY: gds
gds: ## Run FPrime GDS
	@echo "Running FPrime GDS..."
	@if [ -n "$(UART_DEVICE)" ]; then \
		echo "Using UART_DEVICE=$(UART_DEVICE)"; \
		$(GDS_COMMAND) --uart-device $(UART_DEVICE); \
	fi
	$(GDS_COMMAND)

.PHONY: delete-shadow-gds
delete-shadow-gds:
	@echo "Deleting shadow GDS..."
	@$(UV_RUN) pkill -9 -f fprime_gds
	@$(UV_RUN) pkill -9 -f fprime-gds

.PHONY: gds-integration
gds-integration: framer-plugin
	@$(GDS_COMMAND) --gui=none --output-unframed-data --uart-device=$(if $(UART_DEVICE),$(UART_DEVICE),/dev/ttyBOARD)

.PHONY: DoL_test
DOL_SERIAL_PORT ?= /dev/ttyACM0
DoL_test: ## Run the day-in-the-life tests (DOL_SERIAL_PORT=/dev/ttyXXX)
	@echo "make sure passthrough GDS is running"
	@DOL_SERIAL_PORT="$(DOL_SERIAL_PORT)" $(UV_RUN) pytest PROVESFlightControllerReference/test/day-in-the-life/test_day_in_the_life.py PROVESFlightControllerReference/test/day-in-the-life/test_edge_cases_day_in_the_life.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment

.PHONY: framer-plugin
framer-plugin: fprime-venv ## Build framer plugin
	@echo "Framer plugin built and installed in virtual environment."
	@# Use `uv pip` (installs into $(VIRTUAL_ENV)) rather than `uv run` inside
	@# Framing/: `uv run` treats Framing as its own uv project and syncs its
	@# lockfile into the shared venv, uninstalling/reinstalling dozens of
	@# packages. When gds-integration runs in the background this churn races
	@# any concurrently starting pytest and breaks its imports mid-flight.
	@ $(UV) pip install -e Framing

.PHONY: copy-secrets
copy-secrets:
	@if [ -z "$(SECRETS_DIR)" ]; then \
		echo "Error: Must pass valid secrets dir. Usage: make copy-secrets SECRETS_DIR=dir"; \
		exit 1; \
	fi
	@mkdir -p ./keys/
	@cp $(SECRETS_DIR)/proves.pem ./keys/
	@cp $(SECRETS_DIR)/proves.pub.pem ./keys/
	@cp $(SECRETS_DIR)/AuthDefaultKey.h ./PROVESFlightControllerReference/Components/TcSecurityDeframer/
	@echo "Copied secret files 🤫"

.PHONY: make-ci-spacecraft-id
make-ci-spacecraft-id: ## Generate a unique spacecraft ID for CI builds
	@echo "Generating unique spacecraft ID for CI build..."
	sed -i.bak 's/SpacecraftId = 0x0044/SpacecraftId = 0x0043/' PROVESFlightControllerReference/project/config/ComCfg.fpp && \
	rm PROVESFlightControllerReference/project/config/ComCfg.fpp.bak
	@grep -q 'SpacecraftId = 0x0043' PROVESFlightControllerReference/project/config/ComCfg.fpp || (echo "Failed to set CI spacecraft ID in ComCfg.fpp" && exit 1)

include makelib/build-tools.mk
include makelib/ci.mk
include makelib/zephyr.mk
