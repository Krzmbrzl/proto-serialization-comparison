cmake_minimum_required(VERSION 3.15)

project(ProtoSerializationComparison
	VERSION "1.0.0"
	LANGUAGES "CXX"
)

set(CMAKE_CXX_STANDARD 14)


set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(BUILD_STATIC_LIBS OFF CACHE BOOL "")

set(3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdParty")

set(BUILD_TESTING OFF CACHE BOOL "")
add_subdirectory("${3RDPARTY_DIR}/capnproto")

add_executable(serialization_benchmark "benchmark.cpp")

# Generate 

find_package(Protobuf REQUIRED)

set(PROTO_DIR "${CMAKE_SOURCE_DIR}/proto-definitions")
set(PROTOBUF_FILE "${PROTO_DIR}/test.proto")
set(CPNPROTO_FILE "${PROTO_DIR}/test.capnp")
set(CPNPROTO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cpnproto_generated")

if (NOT EXISTS "${CPNPROTO_OUT_DIR}")
	file(MAKE_DIRECTORY "${CPNPROTO_OUT_DIR}")
endif()

protobuf_generate(LANGUAGE cpp TARGET serialization_benchmark PROTOS ${PROTOBUF_FILE})
include_directories("${CMAKE_BINARY_DIR}")
target_link_libraries(serialization_benchmark PRIVATE protobuf::libprotobuf)
target_include_directories(serialization_benchmark PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/proto-definitions")

get_target_property(CPNPROTO_INCLUDE_DIR capnp_tool CAPNP_INCLUDE_DIRECTORY)
add_custom_target(generate_capnproto_files
	COMMAND $<TARGET_FILE:capnp_tool> compile -o$<TARGET_FILE:capnpc_cpp>:"${CPNPROTO_OUT_DIR}" --src-prefix="${PROTO_DIR}" -I"${CPNPROTO_INCLUDE_DIR}" "${CPNPROTO_FILE}"
	COMMENT "Generating CapnProto files"
)

add_dependencies(serialization_benchmark generate_capnproto_files)

file(GLOB CPNPROTO_GENERATED_FILES "${CPNPROTO_OUT_DIR}/*")

target_sources(serialization_benchmark PRIVATE ${CPNPROTO_GENERATED_FILES})
target_include_directories(serialization_benchmark PRIVATE "${CPNPROTO_OUT_DIR}/")
target_link_libraries(serialization_benchmark PRIVATE CapnProto::capnp)


add_subdirectory("${3RDPARTY_DIR}/lz4/build/cmake" lz4)
target_link_libraries(serialization_benchmark PRIVATE lz4_static)
target_include_directories(serialization_benchmark PRIVATE "${3RDPARTY_DIR}/lz4/lib/")


set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "")
add_subdirectory("${3RDPARTY_DIR}/google_benchmark" "google_benchmark")
target_link_libraries(serialization_benchmark PRIVATE benchmark::benchmark)
