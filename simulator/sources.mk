# Domain/UI sources shared by SDL and browser builds. Platform adapters stay local.
SHARED_APP_CPP := \
  arduino_compat.cpp preferences.cpp \
  crsf_management_native.cpp $(ROOT)/src/crsf_tx_config.cpp \
  $(ROOT)/src/transmitter_ui_lvgl.cpp \
  $(ROOT)/src/channel_response.cpp \
  $(ROOT)/src/control_configuration.cpp \
  $(ROOT)/src/control_engine.cpp \
  $(ROOT)/src/channel_model.cpp \
  $(ROOT)/src/channel_settings_repository.cpp \
  $(ROOT)/src/proportional_input.cpp \
  $(ROOT)/src/control_assignment.cpp \
  $(ROOT)/src/binding_resolver.cpp \
  $(ROOT)/src/physical_input.cpp \
  $(ROOT)/src/physical_input_runtime.cpp \
  $(ROOT)/src/r1_input_map.cpp \
  $(ROOT)/src/crsf_channel_output.cpp \
  $(ROOT)/src/crsf_telemetry.cpp \
  $(ROOT)/src/output_safety.cpp \
  $(ROOT)/src/jr_propo_boot_logo.cpp \
  $(ROOT)/src/r1_evo_wordmark.cpp \
  $(ROOT)/src/setup_portal.cpp \
  $(ROOT)/src/offline_map.cpp \
  $(ROOT)/src/gps_basemap.cpp \
  $(ROOT)/src/model_image.cpp \
  $(ROOT)/src/model_image_assets.cpp \
  $(ROOT)/src/hyper7_icon.cpp
