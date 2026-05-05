px4_add_module(
	MODULE modules__svea_encoder_uart
	MAIN svea_encoder_uart
	SRCS
		svea_encoder_uart.cpp
	DEPENDS
		px4_work_queue
)
