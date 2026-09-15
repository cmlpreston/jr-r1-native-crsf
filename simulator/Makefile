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
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -Wno-deprecated-declarations
CFLAGS := -std=c11 -O0 -g
LDLIBS := $(SDL_LIBS) $(OPENSSL_LIBS)

APP_CPP := \
  main.cpp sdl_platform.cpp arduino_compat.cpp preferences.cpp \
  crsf_management_native.cpp $(ROOT)/src/crsf_tx_config.cpp \
  $(ROOT)/src/transmitter_ui_lvgl.cpp \
  $(ROOT)/src/channel_response.cpp \
  $(ROOT)/src/control_configuration.cpp \
  $(ROOT)/src/proportional_input.cpp \
  $(ROOT)/src/control_assignment.cpp \
  $(ROOT)/src/binding_resolver.cpp \
  $(ROOT)/src/physical_input.cpp \
  $(ROOT)/src/r1_input_map.cpp \
  $(ROOT)/src/crsf_channel_output.cpp \
  $(ROOT)/src/output_safety.cpp \
  $(ROOT)/src/jr_propo_boot_logo.cpp \
  $(ROOT)/src/hyper7_icon.cpp
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

# UI/model headers change object layouts; rebuild every consumer together.
$(APP_OBJ): $(wildcard $(ROOT)/include/*.h) $(wildcard include/*.h) $(ROOT)/src/transmitter_ui_design.inc $(ROOT)/src/transmitter_ui_portrait.inc ui_audit.inc

$(LVGL_OBJ): $(ROOT)/include/lv_conf.h

.PHONY: audit
# Each audit changes preferences, so use a fresh disposable state directory.
audit: all portrait
	@set -e; for binary in ./$(TARGET) ./build-portrait/jr-r1-sim-portrait; do \
	  audit_state=$$(mktemp -d /tmp/jr-r1-ui-audit.XXXXXX); \
	  JR_R1_SIM_STATE="$$audit_state" SDL_VIDEODRIVER=dummy "$$binary" --ui-audit; \
	done
