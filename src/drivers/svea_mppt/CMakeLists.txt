px4_add_module(
	MODULE drivers__svea_mppt
	MAIN svea_mppt
	SRCS svea_mppt.cpp
	MODULE_CONFIG
		module.yaml
	DEPENDS px4_work_queue
)
