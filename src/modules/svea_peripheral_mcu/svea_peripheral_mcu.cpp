#include "svea_peripheral_mcu.hpp"

// This file is intentionally left minimal.
// Implementation is split across:
// - svea_peripheral_mcu_main.cpp: task_spawn, instantiate, print_status
// - svea_peripheral_mcu_protocol.cpp: frame parsing and CRC
// - svea_peripheral_mcu_topics.cpp: topic handlers (wheel_distance, wheel_encoders)
// - svea_peripheral_mcu_transport.cpp: serial I/O
//
// The class definition is in svea_peripheral_mcu.hpp and uses the
// descriptor-based ModuleBase pattern for compatibility with the current PX4 API.
