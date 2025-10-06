.PHONY: minimize-uv-cache
minimize-uv-cache:
	@$(UV) cache prune --ci

.PHONY: generate-ci
generate-ci:
	@$(UV_RUN) fprime-util generate -f

.PHONY: build-ci
build-ci:
	@$(UV_RUN) fprime-util build
