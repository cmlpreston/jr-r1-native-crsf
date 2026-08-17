ROOT := ..
BUILD := build
TARGET := $(BUILD)/jr-r1-sim
LVGL := third_party/lvgl

CXX ?= c++
CC ?= cc
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)
OPENSSL_PREFIX := $(shell brew --prefix openssl@3 2>/dev/null)
OPENSSL_CFLAGS := -I$(OPENSSL_PREFIX)/include
OPENSSL_LIBS := -L$(OPENSSL_PREFIX)/lib -lcrypto

CPPFLAGS := -DJR_R1_SIMULATOR -DLV_CONF_INCLUDE_SIMPLE \
  -DJR_R1_SIM_STATE_DIR=\"$(CURDIR)/state\" \
  -Iinclude -I$(ROOT)/include -I$(ROOT) -I$(LVGL) \
  $(SDL_CFLAGS) $(OPENSSL_CFLAGS)
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -Wno-deprecated-declarations
CFLAGS := -std=c11 -O0 -g
LDLIBS := $(SDL_LIBS) $(OPENSSL_LIBS)

APP_CPP := \
  main.cpp sdl_platform.cpp arduino_compat.cpp preferences.cpp \
  crsf_management_native.cpp \
  $(ROOT)/src/channel2_ui_lvgl.cpp \
  $(ROOT)/src/channel_response.cpp \
  $(ROOT)/src/proportional_input.cpp \
  $(ROOT)/src/control_assignment.cpp \
  $(ROOT)/src/binding_resolver.cpp \
  $(ROOT)/src/physical_input.cpp \
  $(ROOT)/src/r1_input_map.cpp \
  $(ROOT)/src/crsf_channel_output.cpp \
  $(ROOT)/src/hyper7_icon.cpp
LVGL_C := $(shell test -d $(LVGL)/src && find $(LVGL)/src -name '*.c')
APP_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(notdir $(APP_CPP)))
LVGL_OBJ := $(patsubst $(LVGL)/src/%.c,$(BUILD)/lvgl/%.o,$(LVGL_C))

.PHONY: all run clean bootstrap check-deps
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

clean:
	rm -rf $(BUILD)
