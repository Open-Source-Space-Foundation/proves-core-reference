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

.PHONY: generate
generate: submodules fprime-venv zephyr ## Generate FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util generate --force

.PHONY: generate-if-needed
BUILD_DIR ?= $(shell pwd)/build-fprime-automatic-zephyr
generate-if-needed:
	@test -d $(BUILD_DIR) || $(MAKE) generate

.PHONY: build
build: submodules zephyr fprime-venv generate-if-needed framer-plugin ## Build FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util build
	./tools/bin/make-loadable-image ./build-artifacts/zephyr.signed.bin bootable.uf2

SYSBUILD_PATH ?= $(shell pwd)/lib/zephyr-workspace/zephyr/samples/sysbuild/with_mcuboot
.PHONY: build-mcuboot
build-mcuboot: submodules zephyr fprime-venv
	@cp $(shell pwd)/bootloader/sysbuild.conf $(SYSBUILD_PATH)/sysbuild.conf

	$(UV_RUN) $(shell pwd)/tools/bin/build-with-proves $(SYSBUILD_PATH) --sysbuild
	mv $(shell pwd)/build/with_mcuboot/zephyr/zephyr.uf2 $(shell pwd)/mcuboot.uf2

.PHONY: test-integration
test-integration: uv ## Run integration tests (set TEST=<name|file.py> or pass test targets)
	@DEPLOY="build-artifacts/zephyr/fprime-zephyr-deployment"; \
	TARGETS=""; \
	if [ -n "$(TEST)" ]; then \
		case "$(TEST)" in \
			*.py) TARGETS="FprimeZephyrReference/test/int/$(TEST)" ;; \
			*) TARGETS="FprimeZephyrReference/test/int/$(TEST).py" ;; \
		esac; \
		[ -e "$$TARGETS" ] || { echo "Specified test file $$TARGETS not found"; exit 1; }; \
	elif [ -n "$(filter-out $@,$(MAKECMDGOALS))" ]; then \
		for test in $(filter-out $@,$(MAKECMDGOALS)); do \
			case "$$test" in \
				*.py) TARGETS="$$TARGETS FprimeZephyrReference/test/int/$$test" ;; \
				*) TARGETS="$$TARGETS FprimeZephyrReference/test/int/$${test}_test.py" ;; \
			esac; \
		done; \
	else \
		TARGETS="FprimeZephyrReference/test/int"; \
	fi; \
	echo "Running integration tests: $$TARGETS"; \
	$(UV_RUN) pytest $$TARGETS --deployment $$DEPLOY

# Allow test names to be passed as targets without Make trying to execute them
%:
	@:

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

GDS_COMMAND ?= $(UV_RUN) fprime-gds -n --dictionary $(ARTIFACT_DIR)/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-baud 115200 --output-unframed-data --framing-selection authenticate-space-data-link

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
	@$(GDS_COMMAND)

.PHONY: delete-shadow-gds
delete-shadow-gds:
	@echo "Deleting shadow GDS..."
	@$(UV_RUN) pkill -9 -f fprime_gds
	@$(UV_RUN) pkill -9 -f fprime-gds

.PHONY: gds-integration
gds-integration:
	@$(GDS_COMMAND) --gui=none

.PHONY: DoL_test
DoL_test:
	@echo "make sure passthrough GDS is running"
	@$(UV_RUN) pytest test/test_day_in_the_life.py --deployment build-artifacts/zephyr/fprime-zephyr-deployment

.PHONY: framer-plugin
framer-plugin: fprime-venv ## Build framer plugin
	@echo "Framer plugin built and installed in virtual environment."
	@ cd Framing && $(UV_RUN) pip install -e .

include lib/makelib/build-tools.mk
include lib/makelib/ci.mk
include lib/makelib/zephyr.mk
