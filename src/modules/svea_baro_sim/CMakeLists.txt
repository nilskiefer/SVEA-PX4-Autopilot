############################################################################
#
#   Copyright (c) 2026 PX4 Development Team. All rights reserved.
#
############################################################################

px4_add_module(
	MODULE modules__svea_baro_sim
	MAIN svea_baro_sim
	SRCS
		svea_baro_sim.cpp
	DEPENDS
		px4_work_queue
	)
