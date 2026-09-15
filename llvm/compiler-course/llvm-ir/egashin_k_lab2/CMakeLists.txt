get_filename_component(PASS_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PLUGIN_TARGET "${PASS_DIR_NAME}_LLVM_IR")

if (NOT WIN32 AND NOT CYGWIN)
    file(GLOB_RECURSE SOURCES *.cpp *.h *.hpp)
    add_llvm_pass_plugin(${PLUGIN_TARGET} ${SOURCES}
        DEPENDS
        intrinsics_gen
        BUILDTREE_ONLY
    )
    set(LLVM_TEST_DEPENDS ${PLUGIN_TARGET} ${LLVM_TEST_DEPENDS} PARENT_SCOPE)
endif()
