.PHONY: minimize-uv-cache
minimize-uv-cache:
	@$(UV) cache prune --ci

.PHONY: generate-ci
generate-ci:
	@$(UV) run fprime-util generate

.PHONY: build-ci
build-ci:
	@$(UV) run fprime-util build
