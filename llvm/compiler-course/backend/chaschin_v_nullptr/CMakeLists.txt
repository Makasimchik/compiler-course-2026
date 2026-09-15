get_filename_component(DIR_BASENAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PLUGIN_TARGET "${DIR_BASENAME}_BACKEND")

file(GLOB_RECURSE SRC_FILES *.cpp *.h *.hpp)

add_llvm_pass_plugin(${PLUGIN_TARGET}
    ${SRC_FILES}
    DEPENDS
    intrinsics_gen
    X86
    BUILDTREE_ONLY
)

target_include_directories(${PLUGIN_TARGET} PUBLIC ${PATH_TO_X86})
set(LLVM_TEST_DEPENDS ${PLUGIN_TARGET} ${LLVM_TEST_DEPENDS} PARENT_SCOPE)