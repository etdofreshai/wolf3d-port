CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g
CPPFLAGS ?= -Iinclude
BUILD_DIR := build
TEST_ASSETS := $(BUILD_DIR)/test_assets
TEST_SDL3 := $(BUILD_DIR)/test_sdl3_smoke
SDL3_PKG_CONFIG ?= pkg-config
SDL3_PREFIX ?= ../../.deps/SDL3
SDL3_PKG_CONFIG_PATH := $(SDL3_PREFIX)/lib/pkgconfig:$(SDL3_PREFIX)/lib64/pkgconfig:$(PKG_CONFIG_PATH)
HAVE_SDL3 := $(shell PKG_CONFIG_PATH="$(SDL3_PKG_CONFIG_PATH)" $(SDL3_PKG_CONFIG) --exists sdl3 && echo yes || echo no)
SDL3_CFLAGS := $(shell PKG_CONFIG_PATH="$(SDL3_PKG_CONFIG_PATH)" $(SDL3_PKG_CONFIG) --cflags sdl3 2>/dev/null)
SDL3_LIBS := $(shell PKG_CONFIG_PATH="$(SDL3_PKG_CONFIG_PATH)" $(SDL3_PKG_CONFIG) --libs sdl3 2>/dev/null)

.PHONY: all test test-sdl3 sdl3-info clean

all: $(TEST_ASSETS)

$(BUILD_DIR):
	mkdir -p $@

$(TEST_ASSETS): src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c include/wl_assets.h include/wl_map_semantics.h include/wl_game_model.h include/wl_gameplay.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o $@

test: $(TEST_ASSETS)
	cd ../.. && source/modern-c-sdl3/$(TEST_ASSETS)

ifeq ($(HAVE_SDL3),yes)
$(TEST_SDL3): tests/test_sdl3_smoke.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SDL3_CFLAGS) tests/test_sdl3_smoke.c $(SDL3_LIBS) -o $@

test-sdl3: $(TEST_SDL3)
	SDL_VIDEODRIVER=dummy ./$(TEST_SDL3)
else
test-sdl3:
	@echo "SDL3 not available; skipping SDL3 smoke test."
	@echo "Run ../../scripts/bootstrap_sdl3.sh or set PKG_CONFIG_PATH to a directory containing sdl3.pc."
endif

sdl3-info:
	@echo "HAVE_SDL3=$(HAVE_SDL3)"
	@echo "SDL3_PREFIX=$(SDL3_PREFIX)"
	@echo "SDL3_PKG_CONFIG_PATH=$(SDL3_PKG_CONFIG_PATH)"

clean:
	rm -rf $(BUILD_DIR)
