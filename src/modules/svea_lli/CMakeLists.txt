px4_add_module(
	MODULE modules__svea_lli
	MAIN svea_lli
	SRCS
		SVEALLI.cpp
		SVEALLI.hpp
	DEPENDS
		px4_work_queue
)
