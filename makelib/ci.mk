.PHONY: minimize-uv-cache
minimize-uv-cache:
	@$(UV) cache prune --ci
