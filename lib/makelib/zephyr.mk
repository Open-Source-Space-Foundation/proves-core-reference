##@ Zephyr

# UV runs west with the active virtual environment
WEST ?= $(UV_RUN) west

# UVX runs west without the active virtual environment
WESTX ?= $(UVX) west

.PHONY: zephyr
zephyr: zephyr-config zephyr-workspace zephyr-export zephyr-sdk zephyr-python-deps

.PHONY: clean-zephyr
clean-zephyr: clean-zephyr-config clean-zephyr-workspace clean-zephyr-export clean-zephyr-sdk

.PHONY: zephyr-config
zephyr-config: fprime-venv ## Configure west
	@test -f ../.west/config || { \
		$(WEST) init --local .; \
	}

.PHONY: clean-zephyr-config
clean-zephyr-config: ## Remove west configuration
	rm -rf ../.west

.PHONY: zephyr-workspace
zephyr-workspace: fprime-venv ## Setup Zephyr bootloader, modules, and tools directories
	@test -d ../lib/zephyr-workspace/bootloader || \
	test -d ../lib/zephyr-workspace/modules || \
	test -d ../lib/zephyr-workspace/tools || { \
		$(WESTX) update; \
	}

.PHONY: clean-zephyr-workspace
clean-zephyr-workspace: ## Remove Zephyr bootloader, modules, and tools directories
	rm -rf ../lib

CMAKE_PACKAGES ?= ~/.cmake/packages
.PHONY: zephyr-export
zephyr-export: fprime-venv ## Export Zephyr environment variables
	@test -d $(CMAKE_PACKAGES)/Zephyr/ || \
	test -d $(CMAKE_PACKAGES)/ZephyrUnittest/ || \
	{ \
		$(WESTX) zephyr-export; \
	}

.PHONY: clean-zephyr-export
clean-zephyr-export: ## Remove Zephyr exported files
	rm -rf $(CMAKE_PACKAGES)/Zephyr $(CMAKE_PACKAGES)/ZephyrUnittest/

ZEPHYR_PATH ?= lib/zephyr-workspace/zephyr
SDK_VERSION ?= $(shell cat $(ZEPHYR_PATH)/SDK_VERSION)
ZEPHYR_SDK_PATH ?= ~/zephyr-sdk-$(SDK_VERSION)
.PHONY: zephyr-sdk
zephyr-sdk: fprime-venv ## Install Zephyr SDK
	@test -d $(ZEPHYR_SDK_PATH) || { \
		$(WEST) sdk install --toolchains arm-zephyr-eabi; \
	}

.PHONY: clean-zephyr-sdk
clean-zephyr-sdk: ## Remove Zephyr SDK
	rm -rf $(ZEPHYR_SDK_PATH)

.PHONY: zephyr-python-deps
zephyr-python-deps: fprime-venv ## Install Zephyr Python dependencies
	@$(WEST) uv pip --install -- --prerelease=allow --quiet
