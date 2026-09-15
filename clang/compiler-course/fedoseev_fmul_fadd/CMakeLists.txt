get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(TARGET_NAME "${CURRENT_DIR_NAME}_LLVM_IR")

if (NOT WIN32 AND NOT CYGWIN)
    set(SOURCES fmadplugin.ll)
    set_source_files_properties(fmadplugin.ll PROPERTIES LANGUAGE CXX)
    
    add_llvm_pass_plugin(${TARGET_NAME} ${SOURCES}
        DEPENDS
        intrinsics_gen
        BUILDTREE_ONLY
    )
    set(LLVM_TEST_DEPENDS ${TARGET_NAME} ${LLVM_TEST_DEPENDS} PARENT_SCOPE)
endif()