CXX ?= c++
CXXFLAGS := -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined

.PHONY: test clean
test: build/protocol_test
	./build/protocol_test

build/protocol_test: test/protocol_test.cpp src/sport_crsf.cpp include/sport_crsf.h \
    src/vehicle_sensors.cpp include/vehicle_sensors.h \
    ../../src/crsf_telemetry.cpp ../../include/crsf_telemetry.h test/Arduino.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -Iinclude -Itest -I../../include \
	    test/protocol_test.cpp src/sport_crsf.cpp src/vehicle_sensors.cpp ../../src/crsf_telemetry.cpp -o $@

clean:
	rm -rf build
