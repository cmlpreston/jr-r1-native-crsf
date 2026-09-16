# Build the same LVGL application as a standalone browser program.
# Requires emsdk 4.0.15 (emcc/em++ on PATH) and make bootstrap.
ROOT := ..
LVGL := third_party/lvgl
ORIENTATION ?= landscape
GIT_BRANCH := $(shell git branch --show-current 2>/dev/null || echo unknown)
GIT_COMMIT := $(shell git rev-parse --short=8 HEAD 2>/dev/null || echo unknown)
GIT_STATE := $(shell test -n "$$(git status --porcelain 2>/dev/null)" && echo dirty || echo clean)

BUILD := build-web$(if $(filter portrait,$(ORIENTATION)),-portrait,)
TARGET := web/dist/r1-$(ORIENTATION).js
CXX := em++
CC := emcc
CPPFLAGS := -DJR_R1_SIMULATOR -DLV_CONF_INCLUDE_SIMPLE \
  -DJR_R1_SIM_STATE_DIR=\"/state-$(ORIENTATION)\" \
  -DJR_R1_GIT_BRANCH=\"$(GIT_BRANCH)\" -DJR_R1_GIT_COMMIT=\"$(GIT_COMMIT)\" \
  -DJR_R1_GIT_STATE=\"$(GIT_STATE)\" -DJR_R1_BUILD_ENV=\"browser-demo\" \
  -Iinclude -I$(ROOT)/include -I$(ROOT) -I$(LVGL) \
  $(if $(filter portrait,$(ORIENTATION)),-DJR_R1_UI_PORTRAIT,)
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CFLAGS := -std=c11 -O2
LDLIBS := -O2 -sMODULARIZE=1 -sEXPORT_NAME=createR1Simulator \
  -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sENVIRONMENT=web \
  -sEXPORTED_RUNTIME_METHODS='["FS"]' -sFORCE_FILESYSTEM=1

APP_CPP := \
  main.cpp web_platform.cpp arduino_compat.cpp preferences.cpp \
  crsf_management_native.cpp $(ROOT)/src/crsf_tx_config.cpp \
  $(ROOT)/src/transmitter_ui_lvgl.cpp \
  $(ROOT)/src/channel_response.cpp \
  $(ROOT)/src/control_configuration.cpp \
  $(ROOT)/src/proportional_input.cpp \
  $(ROOT)/src/control_assignment.cpp \
  $(ROOT)/src/binding_resolver.cpp \
  $(ROOT)/src/physical_input.cpp \
  $(ROOT)/src/physical_input_runtime.cpp \
  $(ROOT)/src/r1_input_map.cpp \
  $(ROOT)/src/crsf_channel_output.cpp \
  $(ROOT)/src/output_safety.cpp \
  $(ROOT)/src/jr_propo_boot_logo.cpp \
  $(ROOT)/src/gps_basemap.cpp \
  $(ROOT)/src/hyper7_icon.cpp
LVGL_C := $(shell find $(LVGL)/src -name '*.c')
APP_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(notdir $(APP_CPP)))
LVGL_OBJ := $(patsubst $(LVGL)/src/%.c,$(BUILD)/lvgl/%.o,$(LVGL_C))

.PHONY: web
web: $(TARGET)

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

# UI/model headers change object layouts; rebuild every consumer together.
$(APP_OBJ): $(wildcard $(ROOT)/include/*.h) $(wildcard include/*.h) $(ROOT)/src/transmitter_ui_design.inc $(ROOT)/src/transmitter_ui_portrait.inc ui_audit.inc

$(LVGL_OBJ): $(ROOT)/include/lv_conf.h

$(BUILD)/gps_basemap.o: $(ROOT)/src/highfield_map_data.inc
