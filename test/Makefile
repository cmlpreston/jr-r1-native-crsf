ROOT := ..
BUILD := build
CXX ?= c++
CPPFLAGS := -I$(ROOT)/include
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -Werror

TESTS := \
  channel_response_test \
  proportional_input_test \
  crsf_channel_output_test \
  physical_input_test \
  physical_input_runtime_test \
  binding_resolver_test

.PHONY: all run clean

all: $(addprefix $(BUILD)/,$(TESTS))

run: all
	@set -e; for test in $(TESTS); do ./$(BUILD)/$$test; done

$(BUILD)/channel_response_test: channel_response_test.cpp $(ROOT)/src/channel_response.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/proportional_input_test: proportional_input_test.cpp $(ROOT)/src/proportional_input.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

$(BUILD)/crsf_channel_output_test: crsf_channel_output_test.cpp $(ROOT)/src/crsf_channel_output.cpp
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

