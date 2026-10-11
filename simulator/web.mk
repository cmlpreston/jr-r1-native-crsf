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
CXXFLAGS := -MMD -MP -std=c++17 -O2 -Wall -Wextra
CFLAGS := -MMD -MP -std=c11 -O2
LDLIBS := -O2 -sMODULARIZE=1 -sEXPORT_NAME=createR1Simulator \
  -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sENVIRONMENT=web \
  -sEXPORTED_RUNTIME_METHODS='["FS"]' -sFORCE_FILESYSTEM=1

include sources.mk
APP_CPP := main.cpp web_platform.cpp $(SHARED_APP_CPP)
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

# Compiler dependencies follow the actual consumers of headers and .inc files.
-include $(APP_OBJ:.o=.d) $(LVGL_OBJ:.o=.d)
$(APP_OBJ) $(LVGL_OBJ): web.mk sources.mk

$(LVGL_OBJ): $(ROOT)/include/lv_conf.h

$(BUILD)/gps_basemap.o: $(ROOT)/src/highfield_map_data.inc
