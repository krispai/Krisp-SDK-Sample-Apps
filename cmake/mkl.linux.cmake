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


if (DEFINED BUILD_SHARED_LIBRARY AND BUILD_SHARED_LIBRARY STREQUAL "1")
	target_link_libraries(
		${DLLNAME}
		"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
		pthread
		m
		dl
	)
endif()

if (DEFINED BUILD_PYTHON_SAMPLE AND BUILD_PYTHON_SAMPLE STREQUAL "1")
	target_link_libraries(
		audio_processor
		PRIVATE
		"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
		pthread
		m
		dl
	)
endif()

if (DEFINED BUILD_NODEJS_SAMPLE AND BUILD_NODEJS_SAMPLE STREQUAL "1")
	target_link_libraries(
		${MODNAME_NODE}
		PRIVATE
		"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
		pthread
		m
		dl
	)
endif()
