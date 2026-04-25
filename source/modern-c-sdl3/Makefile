CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g
CPPFLAGS ?= -Iinclude
BUILD_DIR := build
TEST_ASSETS := $(BUILD_DIR)/test_assets

.PHONY: all test clean

all: $(TEST_ASSETS)

$(BUILD_DIR):
	mkdir -p $@

$(TEST_ASSETS): src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c include/wl_assets.h include/wl_map_semantics.h include/wl_game_model.h include/wl_gameplay.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/wl_assets.c src/wl_map_semantics.c src/wl_game_model.c src/wl_gameplay.c tests/test_assets.c -o $@

test: $(TEST_ASSETS)
	cd ../.. && source/modern-c-sdl3/$(TEST_ASSETS)

clean:
	rm -rf $(BUILD_DIR)
