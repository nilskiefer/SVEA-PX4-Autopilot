px4_add_module(
	MODULE modules__svea_peripheral_mcu
	MAIN svea_peripheral_mcu
	SRCS
		svea_peripheral_mcu_main.cpp
		svea_peripheral_mcu_protocol.cpp
		svea_peripheral_mcu_topics.cpp
		svea_peripheral_mcu_transport.cpp
	DEPENDS
		px4_work_queue
)
