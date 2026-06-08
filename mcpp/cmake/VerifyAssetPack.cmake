if(NOT DEFINED ASSET_BIN)
    message(FATAL_ERROR "ASSET_BIN is not set")
endif()

if(NOT EXISTS "${ASSET_BIN}")
    message(FATAL_ERROR "Asset pack was not generated: ${ASSET_BIN}")
endif()

file(SIZE "${ASSET_BIN}" ASSET_BIN_SIZE)
message(STATUS "Runtime asset pack: ${ASSET_BIN} (${ASSET_BIN_SIZE} bytes)")

if(ASSET_BIN_SIZE LESS 4096)
    message(FATAL_ERROR "Asset pack is too small (${ASSET_BIN_SIZE} bytes); refusing to build an executable without embedded assets")
endif()
