ROOT := ..
BUILD := build
CXX ?= c++
PYTHON ?= python3
CPPFLAGS := -I$(ROOT)/include
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -Werror

TESTS := \
  module_link_test \
  channel_model_test \
  control_engine_test \
  remote_arm_hold_test \
  rf_bench_session_test \
  model_appearance_test \
  channel_mixing_test \
  remote_screen_test \
  offline_map_test \
  ui_pixel_cache_test \
  boot_diagnostics_test \
  gps_map_test \
  simulator_handset_test \
  speed_statistics_test \
  dashboard_widgets_test \
  channel_response_test \
  control_configuration_test \
  proportional_input_test \
  crsf_channel_output_test \
  output_safety_test \
  physical_input_test \
  physical_input_runtime_test \
  binding_resolver_test \
  pcb_io_test \
  crsf_tx_config_test \
  crsf_tx_telemetry_test

.PHONY: all run clean

all: $(addprefix $(BUILD)/,$(TESTS))

$(BUILD)/module_link_test: module_link_test.cpp $(ROOT)/include/module_link.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/rf_bench_session_test: rf_bench_session_test.cpp $(ROOT)/include/rf_bench_session.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

run: all
	$(PYTHON) upload_preflight_test.py
	$(PYTHON) flash_via_pi_test.py
	$(PYTHON) input_monitor_test.py
	@set -e; for test in *test.cjs; do node "$$test"; done
	@set -e; for test in $(TESTS); do ./$(BUILD)/$$test; done

$(BUILD)/control_engine_test: control_engine_test.cpp $(ROOT)/src/control_engine.cpp \
    $(ROOT)/src/control_configuration.cpp $(ROOT)/src/proportional_input.cpp \
    $(ROOT)/src/channel_response.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/channel_model_test: channel_model_test.cpp $(ROOT)/src/channel_model.cpp \
    $(ROOT)/src/channel_settings_repository.cpp $(ROOT)/simulator/preferences.cpp \
    $(ROOT)/src/control_configuration.cpp $(ROOT)/src/channel_response.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) -I$(ROOT)/simulator/include $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/channel_response_test: channel_response_test.cpp $(ROOT)/src/channel_response.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/control_configuration_test: control_configuration_test.cpp \
    $(ROOT)/src/control_configuration.cpp $(ROOT)/src/channel_response.cpp \
    $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/proportional_input_test: proportional_input_test.cpp $(ROOT)/src/proportional_input.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/crsf_channel_output_test: crsf_channel_output_test.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/output_safety_test: output_safety_test.cpp \
    $(ROOT)/src/output_safety.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/physical_input_test: physical_input_test.cpp \
    $(ROOT)/src/physical_input.cpp $(ROOT)/src/control_assignment.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/physical_input_runtime_test: physical_input_runtime_test.cpp \
    $(ROOT)/src/physical_input_runtime.cpp $(ROOT)/src/physical_input.cpp \
    $(ROOT)/src/control_assignment.cpp $(ROOT)/include/waveshare_touch_orientation.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/binding_resolver_test: binding_resolver_test.cpp \
    $(ROOT)/src/binding_resolver.cpp $(ROOT)/src/control_assignment.cpp \
    $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

clean:
	rm -rf $(BUILD)

$(BUILD)/pcb_io_test: pcb_io_test.cpp $(ROOT)/src/pcb_io.cpp \
    $(ROOT)/src/mcp23017_input.cpp $(ROOT)/src/r1_input_map.cpp \
    $(ROOT)/src/physical_input_runtime.cpp $(ROOT)/src/physical_input.cpp \
    $(ROOT)/src/control_assignment.cpp
	@mkdir -p $(@D)
	$(CXX) -Imocks $(CPPFLAGS) $(CXXFLAGS) -DJR_R1_XIAO_PCB=1 $(filter %.cpp,$^) -o $@

$(BUILD)/crsf_tx_config_test: crsf_tx_config_test.cpp $(ROOT)/src/crsf_tx_config.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/crsf_tx_telemetry_test: crsf_tx_telemetry_test.cpp $(ROOT)/src/crsf_telemetry.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) -Icrsf_mocks $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/dashboard_widgets_test: dashboard_widgets_test.cpp $(ROOT)/include/dashboard_widgets.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/speed_statistics_test: speed_statistics_test.cpp $(ROOT)/src/binding_resolver.cpp $(ROOT)/src/control_assignment.cpp $(ROOT)/src/crsf_channel_output.cpp $(ROOT)/include/speed_statistics.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/simulator_handset_test: simulator_handset_test.cpp $(ROOT)/src/physical_input_runtime.cpp $(ROOT)/src/physical_input.cpp $(ROOT)/src/binding_resolver.cpp $(ROOT)/src/control_assignment.cpp $(ROOT)/src/crsf_channel_output.cpp $(ROOT)/simulator/include/handset_inputs.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/gps_map_test: gps_map_test.cpp $(ROOT)/include/gps_map_projection.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/boot_diagnostics_test: boot_diagnostics_test.cpp $(ROOT)/src/boot_diagnostics.cpp
	@mkdir -p $(@D)
	$(CXX) -Iboot_mocks $(CPPFLAGS) $(CXXFLAGS) -DJR_R1_XIAO_PCB=1 $(filter %.cpp,$^) -o $@

$(BUILD)/ui_pixel_cache_test: ui_pixel_cache_test.cpp $(ROOT)/include/ui_pixel_cache.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/offline_map_test: offline_map_test.cpp $(ROOT)/src/offline_map.cpp $(ROOT)/include/offline_map.h $(ROOT)/include/offline_map_json.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -I'$(ROOT)/.pio/libdeps/seeed_xiao_esp32s3-pcb-portrait/ArduinoJson/src' $(CXXFLAGS) offline_map_test.cpp $(ROOT)/src/offline_map.cpp -o $@

$(BUILD)/remote_screen_test: remote_screen_test.cpp $(ROOT)/include/network_hostname.h $(ROOT)/include/wifi_retry.h $(ROOT)/include/remote_screen.h $(ROOT)/include/remote_control_policy.h $(ROOT)/include/output_safety.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/channel_mixing_test: channel_mixing_test.cpp $(ROOT)/src/control_configuration.cpp $(ROOT)/src/channel_response.cpp $(ROOT)/src/crsf_channel_output.cpp $(ROOT)/src/output_safety.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(addprefix $(BUILD)/,$(TESTS)): $(wildcard $(ROOT)/include/*.h)

$(BUILD)/model_appearance_test: model_appearance_test.cpp $(ROOT)/include/model_appearance.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/remote_arm_hold_test: remote_arm_hold_test.cpp $(ROOT)/include/remote_arm_hold.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
