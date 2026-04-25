# SDL3 Bootstrap Boundary

The port keeps deterministic headless C tests as the default gate, but SDL3 presentation work needs an optional local dependency path on machines that do not have SDL3 development files installed.

## Local dependency layout

`scripts/bootstrap_sdl3.sh` clones and builds SDL3 into ignored repo-local paths:

- `.deps/SDL-src/` - SDL source checkout
- `.deps/SDL-build/` - CMake build tree
- `.deps/SDL3/` - install prefix with `sdl3.pc`

These paths are ignored and should not be committed.

## Use

```bash
scripts/bootstrap_sdl3.sh
cd source/modern-c-sdl3
make test-sdl3
```

The script requires `git`, `cmake`, a C compiler, and normal SDL3 build prerequisites. It does not install system packages and does not modify `/usr`.

## Headless smoke seam

`source/modern-c-sdl3/tests/test_sdl3_smoke.c` initializes SDL video, creates a hidden 64x64 window, obtains the window surface, fills it, and updates it. The make target runs it with `SDL_VIDEODRIVER=dummy` so future presentation checks can remain headless.

If SDL3 is unavailable, `make test-sdl3` prints a skip message and exits successfully. The normal `make test` gate remains SDL-free.

## Headless/Linux dependency note

On this Debian 12 container, SDL3 could be built without system SDL development packages by using repo-local CMake/Ninja tools and configuring SDL with desktop/audio backends disabled plus `SDL_UNIX_CONSOLE_BUILD=ON`. The resulting enabled presentation backends include `dummy` and `offscreen`, which are enough for the first headless smoke tests.

Because this repo path contains spaces, the Makefile prefers relative `.deps/SDL3` include/lib paths when the repo-local `sdl3.pc` exists. That avoids unquoted absolute paths emitted by `pkg-config` from being split by the shell.
