cmake_minimum_required(VERSION 3.24.0)

if (DEFINED BUILD_SAMPLE_NC AND BUILD_SAMPLE_NC STREQUAL "1")
	target_link_libraries(
		${APPNAME_NC}
		"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
		pthread
		m
		dl
	)
endif()

if (DEFINED BUILD_SAMPLE_AL AND BUILD_SAMPLE_AL STREQUAL "1")
	target_link_libraries(
		${APPNAME_AL}
		"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
		pthread
		m
		dl
	)
endif()

if (DEFINED BUILD_PYTHON_SAMPLE AND BUILD_PYTHON_SAMPLE STREQUAL "1")
	target_link_libraries(
		${PYMODNAME_NC}
		PRIVATE
		"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
		pthread
		m
		dl
	)
endif()
