cmake_minimum_required(VERSION 3.20)

project(ptcgp_sim
    VERSION 0.1.0
    DESCRIPTION "Pokemon TCG Pocket Simulator"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Output directories
# ---------------------------------------------------------------------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Helper: download and extract a versioned package
# ---------------------------------------------------------------------------
function(CheckAndDownloadPackage NAME VERSION LOCAL_PATH URL)
    # Do we already have the correct version?
    if(NOT EXISTS ${LOCAL_PATH}/${VERSION}.ver)
        # Was there a previous version that we need to delete?
        if(EXISTS ${LOCAL_PATH})
            message(STATUS "Deleting old " ${NAME})
            file(REMOVE_RECURSE ${LOCAL_PATH})
        endif()
        message(STATUS "Obtaining " ${NAME} " " ${VERSION})
        file(DOWNLOAD ${URL} ${LOCAL_PATH}.zip)
        message(STATUS "Extracting " ${NAME})
        file(ARCHIVE_EXTRACT INPUT ${LOCAL_PATH}.zip DESTINATION ${LOCAL_PATH})
        file(REMOVE ${LOCAL_PATH}.zip)
        # Create an empty file so we know which version we have
        file(WRITE ${LOCAL_PATH}/${VERSION}.ver)
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Third-party: jsmn (single-header JSON parser)
# ---------------------------------------------------------------------------
set(JSMN_VERSION "v1.1.0")
set(JSMN_LOCAL_PATH "${CMAKE_SOURCE_DIR}/ThirdParty/jsmn")
set(JSMN_URL "https://github.com/zserge/jsmn/archive/refs/tags/${JSMN_VERSION}.zip")

CheckAndDownloadPackage("jsmn" "${JSMN_VERSION}" "${JSMN_LOCAL_PATH}" "${JSMN_URL}")

# Locate the header (the archive extracts into a subdirectory jsmn-<version>)
# Support both flat layout and the nested jsmn-<version> layout GitHub produces
string(REPLACE "v" "" JSMN_VERSION_STRIPPED "${JSMN_VERSION}")
if(EXISTS "${JSMN_LOCAL_PATH}/jsmn.h")
    set(JSMN_INCLUDE_DIR "${JSMN_LOCAL_PATH}")
else()
    set(JSMN_INCLUDE_DIR "${JSMN_LOCAL_PATH}/jsmn-${JSMN_VERSION_STRIPPED}")
endif()

message(STATUS "jsmn include dir: ${JSMN_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# Database: download database.json if not present
# ---------------------------------------------------------------------------
set(PTCGP_DATABASE_URL
    "https://raw.githubusercontent.com/bcollazo/deckgym-core/main/database.json"
)
set(PTCGP_DATABASE_LOCAL "${CMAKE_SOURCE_DIR}/database/database.json")

if(NOT EXISTS "${PTCGP_DATABASE_LOCAL}")
    message(STATUS "Obtaining database.json ...")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/database")
    file(DOWNLOAD
        "${PTCGP_DATABASE_URL}"
        "${PTCGP_DATABASE_LOCAL}"
        SHOW_PROGRESS
        STATUS DOWNLOAD_STATUS
        TLS_VERIFY ON
    )
    list(GET DOWNLOAD_STATUS 0 DOWNLOAD_CODE)
    list(GET DOWNLOAD_STATUS 1 DOWNLOAD_MSG)
    if(NOT DOWNLOAD_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download database.json: ${DOWNLOAD_MSG}")
    endif()
    message(STATUS "database.json saved to ${PTCGP_DATABASE_LOCAL}")
else()
    message(STATUS "database.json already present -- skipping download")
endif()

# ---------------------------------------------------------------------------
# Static library: ptcgp_sim_lib
# ---------------------------------------------------------------------------
file(GLOB_RECURSE LIB_SOURCES
    "${CMAKE_SOURCE_DIR}/src/lib/*.cpp"
)

add_library(ptcgp_sim_lib STATIC ${LIB_SOURCES})

target_include_directories(ptcgp_sim_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib
        ${JSMN_INCLUDE_DIR}
)

target_compile_options(ptcgp_sim_lib PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# Pass the database path as a compile definition so the library can locate it
target_compile_definitions(ptcgp_sim_lib PRIVATE
    PTCGP_DATABASE_PATH="${PTCGP_DATABASE_LOCAL}"
)

# ---------------------------------------------------------------------------
# Executable: ptcgp_cli
# ---------------------------------------------------------------------------
file(GLOB_RECURSE CLI_SOURCES
    "${CMAKE_SOURCE_DIR}/src/cli/*.cpp"
)

add_executable(ptcgp_cli ${CLI_SOURCES})

target_link_libraries(ptcgp_cli PRIVATE ptcgp_sim_lib)

target_include_directories(ptcgp_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(ptcgp_cli PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== ptcgp_sim build configuration ===")
message(STATUS "  Version      : ${PROJECT_VERSION}")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Library sources:")
foreach(src ${LIB_SOURCES})
    message(STATUS "    ${src}")
endforeach()
message(STATUS "  CLI sources:")
foreach(src ${CLI_SOURCES})
    message(STATUS "    ${src}")
endforeach()
message(STATUS "======================================")
message(STATUS "")
