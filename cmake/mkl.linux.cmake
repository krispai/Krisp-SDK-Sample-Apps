cmake_minimum_required(VERSION 3.24.0)

target_link_libraries(
	${APPNAME_NC}
	"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
	pthread
	m
	dl
)

target_link_libraries(
	${DLLNAME}
	"$<LINK_GROUP:RESCAN,${MKL_LIB_LIST}>"
	pthread
	m
	dl
)
