px4_add_module(
	MODULE modules__simulation__svea_sitl
	MAIN svea_sitl
	SRCS
		svea_sitl.cpp
	DEPENDS
		px4_work_queue
)

