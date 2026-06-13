if(NOT DEFINED ASSET_BIN)
    message(FATAL_ERROR "ASSET_BIN is not set")
endif()

if(NOT EXISTS "${ASSET_BIN}")
    message(FATAL_ERROR "Asset pack was not generated: ${ASSET_BIN}")
endif()

file(SIZE "${ASSET_BIN}" ASSET_BIN_SIZE)
message(STATUS "Asset pack: ${ASSET_BIN} (${ASSET_BIN_SIZE} bytes)")
