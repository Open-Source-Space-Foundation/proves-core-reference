.PHONY: all
all: submodules fprime-venv zephyr generate-if-needed build

.PHONY: help
help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Dependencies

.PHONY: submodules
submodules: ## Initialize and update git submodules
	@git submodule update --init --recursive
	@echo "Applying fprime-gds version patch..."
	@cd lib/fprime && \
		if git apply --check ../../patches/fprime-gds-version.patch 2>/dev/null; then \
			git apply ../../patches/fprime-gds-version.patch && \
			echo "✓ Applied fprime-gds version patch"; \
		elif git apply --reverse --check ../../patches/fprime-gds-version.patch 2>/dev/null; then \
			echo "⚠ Patch already applied"; \
		else \
			echo "❌ Error: Unable to apply patch. Run 'cd lib/fprime && git status' to check."; \
			exit 1; \
		fi

export VIRTUAL_ENV ?= $(shell pwd)/fprime-venv
.PHONY: fprime-venv
FPRIME_YAMCS_MAIN ?= $(shell pwd)/fprime-venv/lib/python*/site-packages/fprime_yamcs/__main__.py
fprime-venv: uv ## Create a virtual environment
	@$(UV) venv fprime-venv --allow-existing
	@$(UV) pip install --prerelease=allow --requirement requirements.txt --overrides yamcs/pyyaml-override.txt
	@echo "Applying fprime-yamcs noapp/path patch..."
	@TARGET=$$(ls $(FPRIME_YAMCS_MAIN) 2>/dev/null | head -1); \
	  if [ -z "$$TARGET" ]; then echo "⚠ fprime-yamcs not found, skipping patch"; exit 0; fi; \
	  PATCH_DIR=$$(dirname $$TARGET); \
	  if grep -q 'venv_bin = str(Path(sys.executable).parent)' $$TARGET; then \
	    echo "⚠ fprime-yamcs patch already applied"; \
	  elif patch --dry-run -p2 -d $$PATCH_DIR < patches/fprime-yamcs-noapp-path.patch > /dev/null 2>&1; then \
	    patch -p2 -d $$PATCH_DIR < patches/fprime-yamcs-noapp-path.patch && echo "✓ Applied fprime-yamcs patch"; \
	  else \
	    echo "❌ Error: Unable to apply fprime-yamcs patch. Run 'ls $$TARGET' to check."; exit 1; \
	  fi
	@echo "Applying fprime-yamcs-events CPU fix..."
	@EVENTS_PROC=$$(ls $(shell pwd)/fprime-venv/lib/python*/site-packages/fprime_yamcs/events/processor.py 2>/dev/null | head -1); \
	  if [ -z "$$EVENTS_PROC" ]; then echo "⚠ events processor not found, skipping"; exit 0; fi; \
	  $(VIRTUAL_ENV)/bin/python tools/apply-events-cpu-fix.py "$$EVENTS_PROC"


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
	@cp PROVESFlightControllerReference/Components/Authenticate/docs/sdd.md docs-site/components/Authenticate.md
	@cp PROVESFlightControllerReference/Components/AuthenticationRouter/docs/sdd.md docs-site/components/AuthenticationRouter.md
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

##@ Authentication Keys

AUTH_DEFAULT_KEY_HEADER ?= PROVESFlightControllerReference/Components/Authenticate/AuthDefaultKey.h
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

test-unit: ## Run unit tests
	cmake -S PROVESFlightControllerReference/test/unit-tests -B build-gtest -DBUILD_TESTING=ON
	cmake --build build-gtest
	ctest --test-dir build-gtest

.PHONY: test-integration
test-integration: uv ## Run integration tests (set TEST=<name|file.py> or pass test targets)
	@DEPLOY="build-artifacts/zephyr/fprime-zephyr-deployment"; \
	TARGETS=""; \
	FILTER="not flaky"; \
	if [ -n "$(TEST)" ]; then \
		case "$(TEST)" in \
			*.py) TARGETS="PROVESFlightControllerReference/test/int/$(TEST)" ;; \
			*) TARGETS="PROVESFlightControllerReference/test/int/$(TEST).py" ;; \
		esac; \
		[ -e "$$TARGETS" ] || { echo "Specified test file $$TARGETS not found"; exit 1; }; \
	elif [ -n "$(filter-out $@,$(MAKECMDGOALS))" ]; then \
		FILTER=""; \
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
	$(UV_RUN) pytest $$TARGETS --deployment $$DEPLOY -m "$$FILTER"

# Allow test names to be passed as targets without Make trying to execute them
%:
	@:

.PHONY: test-interactive
test-interactive: fprime-venv ## Run interactive test selection (set ARGS for CLI mode, e.g., ARGS="--all --cycles 10")
	@$(UV_RUN) python PROVESFlightControllerReference/test/run_interactive_tests.py $(ARGS)

.PHONY: bootloader
bootloader: uv
	@if picotool info ; then \
		echo "RP2350 already in bootloader mode - skipping trigger"; \
	else \
		echo "RP2350 not in bootloader mode - triggering bootloader"; \
		$(UV_RUN) pytest PROVESFlightControllerReference/test/bootloader_trigger.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment; \
	fi

.PHONY: sync-sequence-number
sync-sequence-number: fprime-venv ## Synchronize sequence number between GDS and flight software
	@echo "Synchronizing sequence number; ensure you have the GDS open."
	$(UV_RUN) pytest PROVESFlightControllerReference/test/sync_sequence_number.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment

.PHONY: clean
clean: ## Remove all gitignored files
	git clean -dfX

##@ YAMCS

.PHONY: yamcs-dict
yamcs-dict: fprime-venv ## Generate XTCE dictionary for YAMCS (requires build-artifacts; run 'make build' first)
	@mkdir -p yamcs/yamcs-data/mdb
	@DICT=$$(find build-artifacts -name "*TopologyDictionary.json" | head -1); \
	  if [ -z "$$DICT" ]; then echo "Error: run 'make build' first"; exit 1; fi; \
	  echo "Generating XTCE from $$DICT"; \
	  $(UV_RUN) fprime-to-xtce "$$DICT" -o yamcs/yamcs-data/mdb/fprime.xtce.xml
	@echo "XTCE dictionary at yamcs/yamcs-data/mdb/fprime.xtce.xml"

.PHONY: yamcs-stop
yamcs-stop: ## Stop all YAMCS-related processes (YAMCS server, events bridge, adapter)
	@echo "Stopping YAMCS processes..."
	@pkill -f 'proves_adapter.py' 2>/dev/null && echo "  stopped serial adapter" || true
	@pkill -f 'fprime-yamcs-events' 2>/dev/null && echo "  stopped fprime-yamcs-events" || true
	@pkill -f 'fprime_yamcs' 2>/dev/null && echo "  stopped fprime-yamcs wrapper" || true
	@pkill -f 'mvn.*yamcs' 2>/dev/null && echo "  stopped Maven yamcs runner" || true
	@pkill -f 'org.yamcs.YamcsServer' 2>/dev/null && echo "  stopped YAMCS server" || true
	@i=0; while pgrep -f 'org.yamcs.YamcsServer' > /dev/null 2>&1; do \
	  sleep 0.5; i=$$((i+1)); if [ $$i -ge 10 ]; then \
	    echo "  Warning: YAMCS server still running after 5s, forcing..."; \
	    pkill -9 -f 'org.yamcs.YamcsServer' 2>/dev/null; break; fi; done
	@echo "Done."

.PHONY: yamcs
yamcs: fprime-venv yamcs-dict ## Run YAMCS with serial adapter (Use Case 1: UART_DEVICE=/dev/ttyXXX)
	@if [ -z "$(UART_DEVICE)" ]; then echo "Error: set UART_DEVICE=/dev/ttyXXX"; exit 1; fi
	@$(MAKE) yamcs-stop
	@echo "Starting YAMCS (requires Java 11+)..."
	@mkdir -p $(shell pwd)/yamcs/yamcs-runtime
	FPRIME_GDS_CONFIG_PATH=$(shell pwd)/yamcs/fprime-gds.yml \
	$(UV_RUN) fprime-yamcs \
	    -d $(shell pwd)/build-artifacts/zephyr/fprime-zephyr-deployment \
	    --no-app \
	    --communication-selection none \
	    --no-convert-dictionary \
	    --yamcs-config-dir $(shell pwd)/yamcs/yamcs-data \
	    --yamcs-data-dir $(shell pwd)/yamcs/yamcs-runtime &
	@sleep 5
	@echo "Starting fprime-yamcs-events bridge..."
	$(UV_RUN) fprime-yamcs-events --dictionary $(shell pwd)/build-artifacts/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json &
	@echo "Starting serial adapter on $(UART_DEVICE)..."
	$(VIRTUAL_ENV)/bin/python tools/yamcs/proves_adapter.py \
	    --mode serial \
	    --uart-device $(UART_DEVICE) \
	    --uart-baud 115200

.PHONY: yamcs-server
yamcs-server: yamcs-dict ## Start YAMCS server via Docker (Use Case 2: remote deployment)
	docker compose -f yamcs/docker-compose.yml up

.PHONY: yamcs-adapter-tcp
yamcs-adapter-tcp: fprime-venv ## Start TCP adapter for bent-pipe (GS_HOST=, GS_PORT=, YAMCS_HOST=)
	$(VIRTUAL_ENV)/bin/python tools/yamcs/proves_adapter.py \
	    --mode tcp \
	    --tcp-host $(GS_HOST) \
	    --tcp-port $(GS_PORT) \
	    --yamcs-host $(YAMCS_HOST)

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
	@$(GDS_COMMAND) --gui=none --uart-device=/dev/ttyBOARD

.PHONY: DoL_test
DoL_test:
	@echo "make sure passthrough GDS is running"
	@$(UV_RUN) pytest test/test_day_in_the_life.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment

.PHONY: framer-plugin
framer-plugin: fprime-venv ## Build framer plugin
	@echo "Framer plugin built and installed in virtual environment."
	@ cd Framing && $(UV_RUN) pip install -e .

.PHONY: copy-secrets
copy-secrets:
	@if [ -z "$(SECRETS_DIR)" ]; then \
		echo "Error: Must pass valid secrets dir. Usage: make copy-secrets SECRETS_DIR=dir"; \
		exit 1; \
	fi
	@mkdir -p ./keys/
	@cp $(SECRETS_DIR)/proves.pem ./keys/
	@cp $(SECRETS_DIR)/proves.pub.pem ./keys/
	@cp $(SECRETS_DIR)/AuthDefaultKey.h ./PROVESFlightControllerReference/Components/Authenticate/
	@echo "Copied secret files 🤫"

.PHONY: make-ci-spacecraft-id
make-ci-spacecraft-id: ## Generate a unique spacecraft ID for CI builds
	@echo "Generating unique spacecraft ID for CI build..."
	sed -i.bak 's/SpacecraftId = 0x0044/SpacecraftId = 0x0043/' PROVESFlightControllerReference/project/config/ComCfg.fpp && \
	rm PROVESFlightControllerReference/project/config/ComCfg.fpp.bak
	@grep -q 'SpacecraftId = 0x0043' PROVESFlightControllerReference/project/config/ComCfg.fpp || (echo "Failed to set CI spacecraft ID in ComCfg.fpp" && exit 1)

include lib/makelib/build-tools.mk
include lib/makelib/ci.mk
include lib/makelib/zephyr.mk
