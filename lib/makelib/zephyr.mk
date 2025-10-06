##@ Zephyr

ZEPHYR_WORKSPACE ?= lib/zephyr-workspace
ZEPHYR_PATH ?= $(ZEPHYR_WORKSPACE)/zephyr

# UV runs west with the active virtual environment
WEST ?= cd $(ZEPHYR_WORKSPACE) && $(UV_RUN) west

# UVX runs west without the active virtual environment
WESTX ?= cd $(ZEPHYR_WORKSPACE) && $(UVX) west

zephyr-setup: zephyr-workspace zephyr-export zephyr-sdk zephyr-python-deps

.PHONY: zephyr-workspace
zephyr-workspace: fprime-venv ## Setup Zephyr bootloader, modules, and tools directories
	@test -d $(ZEPHYR_WORKSPACE)/bootloader || \
	test -d $(ZEPHYR_WORKSPACE)/modules || \
	test -d $(ZEPHYR_WORKSPACE)/tools || { \
		$(WESTX) update; \
	}

.PHONY: clean-zephyr-workspace
clean-zephyr-workspace: ## Remove Zephyr bootloader, modules, and tools directories
	rm -rf $(ZEPHYR_WORKSPACE)/bootloader \
		$(ZEPHYR_WORKSPACE)/modules \
		$(ZEPHYR_WORKSPACE)/tools

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

SDK_VERSION ?= $(shell cat $(ZEPHYR_WORKSPACE)/zephyr/SDK_VERSION)
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
	@test -s $(VIRTUAL_ENV)/zephyr-deps.txt || { \
		$(WEST) packages pip > $(VIRTUAL_ENV)/zephyr-deps.txt; \
		$(UV) pip install --requirement $(VIRTUAL_ENV)/zephyr-deps.txt; \
	}
