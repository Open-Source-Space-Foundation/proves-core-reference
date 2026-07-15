##@ Zephyr

# UV runs west with the active virtual environment
WEST ?= $(UV_RUN) west

# UVX runs west without the active virtual environment
WESTX ?= $(UVX) west

ZEPHYR_PATH ?= lib/zephyr-workspace/zephyr
ifeq ($(origin SDK_VERSION), undefined)
SDK_VERSION := $(strip $(shell test -f "$(ZEPHYR_PATH)/SDK_VERSION" && cat "$(ZEPHYR_PATH)/SDK_VERSION"))
endif
ZEPHYR_SDK_PATH ?= $(HOME)/zephyr-sdk-$(SDK_VERSION)

.PHONY: zephyr
zephyr: zephyr-config zephyr-workspace zephyr-export zephyr-python-deps zephyr-sdk

.PHONY: clean-zephyr
clean-zephyr: clean-zephyr-workspace clean-zephyr-export clean-zephyr-sdk

.PHONY: zephyr-config
zephyr-config: fprime-venv ## Configure west
	@test -f .west/config || { \
		$(WEST) init --local .; \
	}

.PHONY: zephyr-workspace
zephyr-workspace: fprime-venv ## Setup Zephyr bootloader, modules, and tools directories
	$(WESTX) update; \


.PHONY: clean-zephyr-workspace
clean-zephyr-workspace: ## Remove Zephyr bootloader, modules, and tools directories
	git submodule deinit --all -f

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

.PHONY: zephyr-sdk
ZEPHYR_SDK_INSTALL_RETRIES ?= 3
zephyr-sdk: fprime-venv ## Install Zephyr SDK
	@test -d $(ZEPHYR_SDK_PATH)/gnu/arm-zephyr-eabi || { \
		i=1; \
		until $(WEST) sdk install --toolchains arm-zephyr-eabi; do \
			if [ "$$i" -ge $(ZEPHYR_SDK_INSTALL_RETRIES) ]; then \
				echo "❌ Error: Zephyr SDK install failed after $(ZEPHYR_SDK_INSTALL_RETRIES) attempts"; \
				exit 1; \
			fi; \
			echo "⚠ Zephyr SDK install failed (attempt $$i/$(ZEPHYR_SDK_INSTALL_RETRIES))"; \
			if [ -f $(ZEPHYR_SDK_PATH)/setup.sh ]; then \
				echo "  Patching setup.sh to fall back to curl if the bundled wget fails, then retrying in place..."; \
				$(UV_RUN) python3 tools/patch-zephyr-sdk-toolchain-download.py $(ZEPHYR_SDK_PATH)/setup.sh; \
			else \
				echo "  Retrying with a clean SDK dir..."; \
				rm -rf $(ZEPHYR_SDK_PATH); \
			fi; \
			i=$$((i + 1)); \
		done; \
	}

.PHONY: clean-zephyr-sdk
clean-zephyr-sdk: ## Remove Zephyr SDK
	rm -rf $(ZEPHYR_SDK_PATH)

.PHONY: zephyr-python-deps
zephyr-python-deps: fprime-venv ## Install Zephyr Python dependencies
	@UV="$(UV)" $(WEST) uv pip --install -- --prerelease=allow --quiet
