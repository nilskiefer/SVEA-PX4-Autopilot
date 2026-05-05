px4_add_module(
	MODULE modules__svea_peripheral_mcu
	MAIN svea_peripheral_mcu
	SRCS
		svea_peripheral_mcu.cpp
	DEPENDS
		px4_work_queue
)
