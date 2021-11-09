cmake_minimum_required(VERSION 3.15)

project(ProtoSerializationComparison
	VERSION "1.0.0"
	LANGUAGES "CXX"
)

set(CMAKE_CXX_STANDARD 14)


set(3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdParty")

add_subdirectory("${3RDPARTY_DIR}/capnproto")

add_executable(proto_test "main.cpp")

# Generate 

find_package(Protobuf REQUIRED)

set(PROTO_DIR "${CMAKE_SOURCE_DIR}/proto-definitions")
set(PROTOBUF_FILE "${PROTO_DIR}/test.proto")
set(CPNPROTO_FILE "${PROTO_DIR}/test.capnp")
set(CPNPROTO_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/cpnproto_generated")

if (NOT EXISTS "${CPNPROTO_OUT_DIR}")
	file(MAKE_DIRECTORY "${CPNPROTO_OUT_DIR}")
endif()

protobuf_generate(LANGUAGE cpp TARGET proto_test PROTOS ${PROTOBUF_FILE})
include_directories("${CMAKE_BINARY_DIR}")
target_link_libraries(proto_test PRIVATE protobuf::libprotobuf)
target_include_directories(proto_test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/proto-definitions")

get_target_property(CPNPROTO_INCLUDE_DIR capnp_tool CAPNP_INCLUDE_DIRECTORY)
add_custom_command(TARGET proto_test
	PRE_BUILD
	COMMAND $<TARGET_FILE:capnp_tool> compile -o$<TARGET_FILE:capnpc_cpp>:"${CPNPROTO_OUT_DIR}" --src-prefix="${PROTO_DIR}" -I"${CPNPROTO_INCLUDE_DIR}" "${CPNPROTO_FILE}"
)
file(GLOB CPNPROTO_GENERATED_FILES "${CPNPROTO_OUT_DIR}/*")

target_sources(proto_test PRIVATE ${CPNPROTO_GENERATED_FILES})
target_include_directories(proto_test PRIVATE "${CPNPROTO_OUT_DIR}")
target_link_libraries(proto_test PRIVATE CapnProto::capnp)
