ROOT := ..
BUILD := build
CXX ?= c++
PYTHON ?= python3
CPPFLAGS := -I$(ROOT)/include
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -Werror
ARDUINOJSON_DIR := $(firstword $(wildcard \
  $(ROOT)/.pio/libdeps/seeed_xiao_esp32s3-pcb/ArduinoJson/src \
  $(ROOT)/.pio/libdeps/seeed_xiao_esp32s3-pcb-portrait/ArduinoJson/src))

TESTS := \
  run_records_test \
  main_input_pipeline_integration_test \
  input_pipeline_test \
  critical_diagnostics_test \
  input_bus_guard_test \
  ui_cold_storage_test \
  raster_map_sampler_test \
  lcd_pixel_transport_test \
  brake_abs_test \
  er6g_mode_pin_test \
  esc_drive_programmer_test \
  er6g_mode_rf_test \
  er6g_listener_test \
  er6g_reader_test \
  er6g_write_test \
  er6g_boot_probe_test \
  er6g_startup_stage_test \
  er6g_single_startup_test \
  er6g_startup_peer_test \
  er4_bridge_integration_test \
  er4_direct_read_test \
  esc_programmer_test \
  graph_link_warning_test \
  first_drive_features_test \
  ers_sensor_config_test \
  gps_acquisition_chime_test \
  gps_local_time_test \
  gps_widget_view_test \
  race_timer_test \
  firmware_package_test \
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
  power_energy_statistics_test \
  trip_run_statistics_test \
  battery_load_balance_test \
  control_history_test \
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

$(BUILD)/run_records_test: run_records_test.cpp $(ROOT)/src/run_record_store.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DJR_R1_SIMULATOR $(filter %.cpp,$^) -o $@

$(BUILD)/er4_bridge_integration_test: er4_bridge_integration_test.cpp $(wildcard er4_stubs/*) \
    $(ROOT)/receiver/er4/Er4EscBridge.cpp $(ROOT)/receiver/er4/Er4EscBridge.h \
    $(ROOT)/include/esc_programmer_server.h $(ROOT)/include/esc_programmer_wire.h
	@mkdir -p $(@D)
	$(CXX) -Ier4_stubs $(CPPFLAGS) $(CXXFLAGS) -Wno-unused-const-variable $< -o $@

$(BUILD)/er4_direct_read_test: er4_direct_read_test.cpp $(wildcard er4_stubs/*) \
    $(ROOT)/receiver/er4/Er4EscBridge.cpp $(ROOT)/receiver/er4/Er4EscBridge.h \
    $(ROOT)/include/esc_programmer_server.h $(ROOT)/include/esc_programmer_wire.h
	@mkdir -p $(@D)
	$(CXX) -Ier4_stubs $(CPPFLAGS) $(CXXFLAGS) -Wno-unused-const-variable $< -o $@

$(BUILD)/er6g_listener_test: er6g_listener_test.cpp $(wildcard er6g_stubs/* er6g_stubs/driver/*) \
    $(ROOT)/receiver/er6g/listener/Er6gEscListen.cpp $(ROOT)/receiver/er6g/listener/Er6gEscListen.h
	@mkdir -p $(@D)
	$(CXX) -Ier6g_stubs $(CXXFLAGS) $< -o $@

$(BUILD)/er6g_reader_test: er6g_reader_test.cpp $(wildcard er6g_stubs/* er6g_stubs/driver/* er6g_stubs/rom/* er6g_stubs/soc/*) \
    $(ROOT)/receiver/er6g/reader/Er6gEscRead.cpp $(ROOT)/receiver/er6g/reader/Er6gEscRead.h er6g_stubs/startup_peer.cpp
	@mkdir -p $(@D)
	$(CXX) -Ier6g_stubs -I$(ROOT)/receiver/er6g/listener -I$(ROOT)/include $(CXXFLAGS) $< er6g_stubs/startup_peer.cpp -o $@

$(BUILD)/er6g_boot_probe_test: er6g_boot_probe_test.cpp $(wildcard er6g_stubs/* er6g_stubs/driver/* er6g_stubs/rom/* er6g_stubs/soc/*) \
    $(ROOT)/receiver/er6g/reader/Er6gEscRead.cpp $(ROOT)/receiver/er6g/reader/Er6gEscRead.h er6g_stubs/startup_peer.cpp
	@mkdir -p $(@D)
	$(CXX) -Ier6g_stubs -I$(ROOT)/receiver/er6g/listener -I$(ROOT)/include $(CXXFLAGS) $< er6g_stubs/startup_peer.cpp -o $@

$(BUILD)/er6g_write_test: er6g_write_test.cpp $(wildcard er6g_stubs/* er6g_stubs/driver/* er6g_stubs/rom/* er6g_stubs/soc/*) $(ROOT)/receiver/er6g/reader/Er6gEscRead.cpp
	@mkdir -p $(@D)
	$(CXX) -Ier6g_stubs -I$(ROOT)/receiver/er6g/listener -I$(ROOT)/include $(CXXFLAGS) $< -o $@

$(BUILD)/er6g_startup_stage_test: er6g_startup_stage_test.cpp $(wildcard er6g_startup_stubs/* er6g_startup_stubs/driver/* er6g_stubs/* er6g_stubs/driver/* er6g_stubs/rom/* er6g_stubs/soc/*) \
    $(ROOT)/receiver/er6g/startup/src/main.cpp $(ROOT)/include/er6g_esc_startup.h
	@mkdir -p $(@D)
	$(CXX) -Ier6g_startup_stubs -Ier6g_stubs -I$(ROOT)/include $(CXXFLAGS) $< -o $@

$(BUILD)/er6g_startup_peer_test: er6g_startup_peer_test.cpp $(wildcard er6g_startup_stubs/*) \
    $(ROOT)/receiver/er6g/reader/Er6gEscStartupPeer.cpp $(ROOT)/receiver/er6g/reader/Er6gEscStartupPeer.h $(ROOT)/include/er6g_esc_startup.h
	@mkdir -p $(@D)
	$(CXX) -Ier6g_startup_stubs -I$(ROOT)/include $(CXXFLAGS) $< -o $@

$(BUILD)/er6g_single_startup_test: er6g_single_startup_test.cpp $(wildcard er6g_stubs/* er6g_stubs/driver/* er6g_stubs/rom/* er6g_stubs/soc/* er6g_startup_stubs/*) \
    $(ROOT)/receiver/er6g/reader/Er6gEscSingleStartup.cpp $(ROOT)/receiver/er6g/reader/Er6gEscRead.cpp $(ROOT)/receiver/er6g/reader/Er6gEscStartupPeer.h
	@mkdir -p $(@D)
	$(CXX) -Ier6g_stubs -Ier6g_startup_stubs -I$(ROOT)/receiver/er6g/listener -I$(ROOT)/include $(CXXFLAGS) $< -o $@

$(BUILD)/module_link_test: module_link_test.cpp $(ROOT)/include/module_link.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/rf_bench_session_test: rf_bench_session_test.cpp $(ROOT)/include/rf_bench_session.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

run: all
	$(PYTHON) critical_task_boundary_test.py
	$(PYTHON) font_subset_test.py
	$(PYTHON) lcd_pixel_bus_test.py
	$(PYTHON) package_ota_test.py
	$(PYTHON) upload_preflight_test.py
	$(PYTHON) er4_upload_preflight_test.py
	$(PYTHON) er6g_upload_preflight_test.py
	$(PYTHON) er6g_mode_upload_preflight_test.py
	$(PYTHON) er6g_single_upload_preflight_test.py
	$(PYTHON) er6g_restore_upload_preflight_test.py
	$(PYTHON) er6g_write_upload_preflight_test.py
	$(PYTHON) flash_via_pi_test.py
	$(PYTHON) package_ci_firmware_test.py
	$(PYTHON) ci_ccache_test.py
	$(PYTHON) input_monitor_test.py
	@set -e; for test in *test.cjs; do node "$$test"; done
	@set -e; for test in $(TESTS); do ./$(BUILD)/$$test; done

$(BUILD)/control_engine_test: control_engine_test.cpp $(ROOT)/src/control_engine.cpp \
    $(ROOT)/src/control_configuration.cpp $(ROOT)/src/proportional_input.cpp \
    $(ROOT)/src/channel_response.cpp $(ROOT)/src/crsf_channel_output.cpp $(ROOT)/src/output_safety.cpp
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

$(BUILD)/input_bus_guard_test: input_bus_guard_test.cpp $(ROOT)/src/mcp23017_input.cpp \
    $(ROOT)/include/input_bus_guard.h $(ROOT)/include/mcp23017_input.h
	@mkdir -p $(@D)
	$(CXX) -Imocks $(CPPFLAGS) $(CXXFLAGS) -DJR_R1_INPUT_BUS_HOST_LOCK=1 -pthread $(filter %.cpp,$^) -o $@

$(BUILD)/crsf_tx_config_test: crsf_tx_config_test.cpp $(ROOT)/src/crsf_tx_config.cpp $(ROOT)/src/crsf_channel_output.cpp $(ROOT)/include/crsf_tx_config.h $(ROOT)/include/ui_cold_storage.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/ui_cold_storage_test: ui_cold_storage_test.cpp cold_storage_mocks/esp_heap_caps.h $(ROOT)/include/ui_cold_storage.h $(ROOT)/include/ui_telemetry_history.h $(ROOT)/include/sensor_statistics.h
	@mkdir -p $(@D)
	$(CXX) -Icold_storage_mocks $(CPPFLAGS) $(CXXFLAGS) -DARDUINO=1 $< -o $@

$(BUILD)/crsf_tx_telemetry_test: crsf_tx_telemetry_test.cpp $(ROOT)/src/crsf_telemetry.cpp $(ROOT)/src/crsf_channel_output.cpp
	@mkdir -p $(@D)
	$(CXX) -Icrsf_mocks $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/dashboard_widgets_test: dashboard_widgets_test.cpp $(ROOT)/include/dashboard_widgets.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/power_energy_statistics_test: power_energy_statistics_test.cpp $(ROOT)/include/power_energy_statistics.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/trip_run_statistics_test: trip_run_statistics_test.cpp $(ROOT)/include/trip_run_statistics.h $(ROOT)/include/power_energy_statistics.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/battery_load_balance_test: battery_load_balance_test.cpp $(ROOT)/include/battery_load_balance.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/control_history_test: control_history_test.cpp $(ROOT)/include/control_history.h
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

$(BUILD)/gps_widget_view_test: gps_widget_view_test.cpp $(ROOT)/include/gps_widget_view.h
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
	@test -n "$(ARDUINOJSON_DIR)" || (echo "ArduinoJson missing; first run: .venv/bin/pio run -e seeed_xiao_esp32s3-pcb"; exit 1)
	$(CXX) $(CPPFLAGS) -I'$(ARDUINOJSON_DIR)' $(CXXFLAGS) offline_map_test.cpp $(ROOT)/src/offline_map.cpp -o $@

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

$(BUILD)/firmware_package_test: firmware_package_test.cpp $(ROOT)/include/firmware_package.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/race_timer_test: race_timer_test.cpp $(ROOT)/include/race_timer.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/gps_acquisition_chime_test: gps_acquisition_chime_test.cpp $(ROOT)/include/gps_acquisition_chime.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/gps_local_time_test: gps_local_time_test.cpp $(ROOT)/include/gps_local_time.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/ers_sensor_config_test: ers_sensor_config_test.cpp $(ROOT)/include/ers_sensor_config.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/first_drive_features_test: first_drive_features_test.cpp $(ROOT)/include/drive_battery.h $(ROOT)/include/sensor_statistics.h
	@mkdir -p $(@D)
	$(CXX) -Icrsf_mocks $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/graph_link_warning_test: graph_link_warning_test.cpp $(ROOT)/include/rf_link_warning.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/esc_programmer_test: esc_programmer_test.cpp $(ROOT)/include/esc_programmer.h $(ROOT)/include/esc_programmer_wire.h $(ROOT)/include/esc_programmer_server.h $(ROOT)/include/surpass_protocol.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/esc_drive_programmer_test: esc_drive_programmer_test.cpp $(ROOT)/include/esc_drive_programmer.h $(ROOT)/src/output_safety.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/er6g_mode_rf_test: er6g_mode_rf_test.cpp $(ROOT)/receiver/er6g/reader/Er6gEscMode.cpp $(ROOT)/receiver/er6g/reader/Er6gEscMode.h
	@mkdir -p $(@D)
	$(CXX) -Ier4_stubs $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/er6g_mode_pin_test: er6g_mode_pin_test.cpp $(ROOT)/receiver/er6g/reader/Er6gEscRead.cpp
	@mkdir -p $(@D)
	$(CXX) -Ier6g_stubs -I$(ROOT)/receiver/er6g/listener $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/brake_abs_test: brake_abs_test.cpp $(ROOT)/include/brake_abs.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/raster_map_sampler_test: raster_map_sampler_test.cpp $(ROOT)/include/raster_map_sampler.h $(ROOT)/include/gps_map_projection.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/lcd_pixel_transport_test: lcd_pixel_transport_test.cpp $(ROOT)/include/lcd_pixel_transport.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/main_input_pipeline_integration_test: main_input_pipeline_integration_test.cpp $(ROOT)/src/main_input_pipeline.inc \
    $(ROOT)/src/physical_input.cpp $(ROOT)/src/physical_input_runtime.cpp $(ROOT)/src/binding_resolver.cpp \
    $(ROOT)/src/crsf_channel_output.cpp $(ROOT)/src/control_assignment.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@

$(BUILD)/critical_diagnostics_test: critical_diagnostics_test.cpp $(ROOT)/include/bounded_event_queue.h
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD)/input_pipeline_test: input_pipeline_test.cpp $(ROOT)/src/physical_input.cpp $(ROOT)/src/physical_input_runtime.cpp $(ROOT)/src/crsf_channel_output.cpp $(ROOT)/src/control_assignment.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.cpp,$^) -o $@
