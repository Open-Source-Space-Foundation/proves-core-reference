.PHONY: all
all: submodules fprime-venv zephyr-setup generate-if-needed build

.PHONY: help
help: ## Display this help.
	@awk 'BEGIN {FS = ":.*##"; printf "\nUsage:\n  make \033[36m<target>\033[0m\n"} /^[a-zA-Z_0-9-]+:.*?##/ { printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2 } /^##@/ { printf "\n\033[1m%s\033[0m\n", substr($$0, 5) } ' $(MAKEFILE_LIST)

##@ Dependencies

.PHONY: submodules
submodules: ## Initialize and update git submodules
	git submodule update --init --recursive

export VIRTUAL_ENV ?= $(shell pwd)/fprime-venv
fprime-venv: ## Create a virtual environment
		@$(MAKE) uv
		@$(UV) venv fprime-venv
		@$(UV) pip install --requirement pyproject.toml

##@ Development

.PHONY: pre-commit-install
pre-commit-install: uv ## Install pre-commit hooks
	@$(UVX) pre-commit install > /dev/null

.PHONY: fmt
fmt: pre-commit-install ## Lint and format files
	@$(UVX) pre-commit run --all-files

.PHONY: generate
generate: submodules fprime-venv zephyr-setup ## Generate FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util generate --force

.PHONY: generate-if-needed
BUILD_DIR ?= $(shell pwd)/build-fprime-automatic-zephyr
generate-if-needed:
	@test -d $(BUILD_DIR) || $(MAKE) generate

.PHONY: build
build: submodules zephyr-setup fprime-venv generate-if-needed ## Build FPrime-Zephyr Proves Core Reference
	@$(UV_RUN) fprime-util build

.PHONY: test-integration
test-integration:
	@$(UV_RUN) pytest FprimeZephyrReference/test/int --deployment build-artifacts/zephyr/fprime-zephyr-deployment

.PHONY: clean
clean: ## Remove all gitignored files
	git clean -dfX

##@ Operations

.PHONY: gds
ARTIFACT_DIR ?= $(shell pwd)/build-artifacts
gds: ## Run FPrime GDS
	@$(UV_RUN) fprime-gds -n --dictionary $(ARTIFACT_DIR)/zephyr/fprime-zephyr-deployment/dict/ReferenceDeploymentTopologyDictionary.json --communication-selection uart --uart-baud 115200 --output-unframed-data

include lib/makelib/build-tools.mk
include lib/makelib/ci.mk
include lib/makelib/zephyr.mk
