ROOT := ..
BUILD := build
CXX ?= c++
CPPFLAGS := -I$(ROOT)/include
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -Werror

TESTS := \
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

run: all
	@set -e; for test in $(TESTS); do ./$(BUILD)/$$test; done

$(BUILD)/channel_response_test: channel_response_test.cpp $(ROOT)/src/channel_response.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/control_configuration_test: control_configuration_test.cpp \
    $(ROOT)/src/control_configuration.cpp $(ROOT)/src/channel_response.cpp \
    $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/proportional_input_test: proportional_input_test.cpp $(ROOT)/src/proportional_input.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/crsf_channel_output_test: crsf_channel_output_test.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/output_safety_test: output_safety_test.cpp \
    $(ROOT)/src/output_safety.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/physical_input_test: physical_input_test.cpp \
    $(ROOT)/src/physical_input.cpp $(ROOT)/src/control_assignment.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/physical_input_runtime_test: physical_input_runtime_test.cpp \
    $(ROOT)/src/physical_input_runtime.cpp $(ROOT)/src/physical_input.cpp \
    $(ROOT)/src/control_assignment.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/binding_resolver_test: binding_resolver_test.cpp \
    $(ROOT)/src/binding_resolver.cpp $(ROOT)/src/control_assignment.cpp \
    $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD)

$(BUILD)/pcb_io_test: pcb_io_test.cpp $(ROOT)/src/pcb_io.cpp \
    $(ROOT)/src/mcp23017_input.cpp $(ROOT)/src/r1_input_map.cpp \
    $(ROOT)/src/physical_input_runtime.cpp $(ROOT)/src/physical_input.cpp \
    $(ROOT)/src/control_assignment.cpp
	@mkdir -p $(@D)
	$(CXX) -Imocks $(CPPFLAGS) $(CXXFLAGS) -DJR_R1_XIAO_PCB=1 $^ -o $@

$(BUILD)/crsf_tx_config_test: crsf_tx_config_test.cpp $(ROOT)/src/crsf_tx_config.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/crsf_tx_telemetry_test: crsf_tx_telemetry_test.cpp $(ROOT)/src/crsf_telemetry.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) -Icrsf_mocks $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

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
