ROOT := ..
BUILD ?= build
TARGET ?= $(BUILD)/jr-r1-sim
EXTRA_CPPFLAGS ?=
LVGL := third_party/lvgl

CXX ?= c++
CC ?= cc
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null)
OPENSSL_CFLAGS := -I$(OPENSSL_PREFIX)/include
OPENSSL_LIBS := -L$(OPENSSL_PREFIX)/lib -lcrypto
GIT_BRANCH := $(shell git branch --show-current 2>/dev/null || echo unknown)
GIT_COMMIT := $(shell git rev-parse --short=8 HEAD 2>/dev/null || echo unknown)
GIT_STATE := $(shell test -n "$$(git status --porcelain 2>/dev/null)" && echo dirty || echo clean)

CPPFLAGS := -DJR_R1_SIMULATOR -DLV_CONF_INCLUDE_SIMPLE \
  -DJR_R1_SIM_STATE_DIR=\"$(CURDIR)/state\" \
  -DJR_R1_GIT_BRANCH=\"$(GIT_BRANCH)\" \
  -DJR_R1_GIT_COMMIT=\"$(GIT_COMMIT)\" \
  -DJR_R1_GIT_STATE=\"$(GIT_STATE)\" \
  -DJR_R1_BUILD_ENV=\"simulator\" \
  -Iinclude -I$(ROOT)/include -I$(ROOT) -I$(LVGL) \
  $(SDL_CFLAGS) $(OPENSSL_CFLAGS) $(EXTRA_CPPFLAGS)
CXXFLAGS := -MMD -MP -std=c++17 -O0 -g -Wall -Wextra -Wno-deprecated-declarations
CFLAGS := -MMD -MP -std=c11 -O0 -g
LDLIBS := $(SDL_LIBS) $(OPENSSL_LIBS)

include sources.mk
APP_CPP := main.cpp sdl_platform.cpp $(SHARED_APP_CPP)
LVGL_C := $(shell test -d $(LVGL)/src && find $(LVGL)/src -name '*.c')
APP_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(notdir $(APP_CPP)))
LVGL_OBJ := $(patsubst $(LVGL)/src/%.c,$(BUILD)/lvgl/%.o,$(LVGL_C))

.PHONY: all run portrait portrait-run browser clean bootstrap check-deps
all: check-deps $(TARGET)

check-deps:
	@command -v sdl2-config >/dev/null || (echo "SDL2 is required (brew install sdl2)"; exit 1)
	@test -f $(OPENSSL_PREFIX)/include/openssl/md5.h || (echo "OpenSSL is required (brew install openssl@3)"; exit 1)
	@test -d $(LVGL)/src || (echo "LVGL missing; run: make bootstrap"; exit 1)

bootstrap:
	@test -d $(LVGL)/.git || git clone --depth 1 --branch v8.4.0 https://github.com/lvgl/lvgl.git $(LVGL)

$(TARGET): $(APP_OBJ) $(LVGL_OBJ)
	@mkdir -p $(@D)
	$(CXX) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

define app-rule
$(BUILD)/$(notdir $(basename $(1))).o: $(1)
	@mkdir -p $$(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $$< -o $$@
endef
$(foreach source,$(filter $(ROOT)/%,$(APP_CPP)),$(eval $(call app-rule,$(source))))

$(BUILD)/lvgl/%.o: $(LVGL)/src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

portrait:
	$(MAKE) BUILD=build-portrait TARGET=build-portrait/jr-r1-sim-portrait \
		EXTRA_CPPFLAGS=-DJR_R1_UI_PORTRAIT all

portrait-run: portrait
	./build-portrait/jr-r1-sim-portrait

browser: all portrait
	python3 browser_demo.py

clean:
	rm -rf build build-portrait

# Compiler dependencies follow the actual consumers of headers and .inc files.
-include $(APP_OBJ:.o=.d) $(LVGL_OBJ:.o=.d)
$(APP_OBJ) $(LVGL_OBJ): Makefile sources.mk

$(LVGL_OBJ): $(ROOT)/include/lv_conf.h

.PHONY: audit
# Each audit changes preferences, so use a fresh disposable state directory.
audit: all portrait
	@set -e; for binary in ./$(TARGET) ./build-portrait/jr-r1-sim-portrait; do \
	  audit_state=$$(mktemp -d /tmp/jr-r1-ui-audit.XXXXXX); \
	  JR_R1_SIM_STATE="$$audit_state" SDL_VIDEODRIVER=dummy "$$binary" --ui-audit; \
	done

$(BUILD)/gps_basemap.o: $(ROOT)/src/highfield_map_data.inc
