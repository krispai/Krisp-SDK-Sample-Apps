if(NOT DEFINED KRISP_3PARTY_LIB_DIR)
	message(FATAL_ERROR "KRISP_3PARTY_LIB_DIR is not defined")
endif()

# Find all .lib files in the directory
file(GLOB ALL_EXTERNAL_LIBS "${KRISP_3PARTY_LIB_DIR}/*.lib")

set(KRISP_THIRDPARTY_LIBS 
	${ALL_EXTERNAL_LIBS}
	Crypt32.lib
	Ws2_32.lib
)
