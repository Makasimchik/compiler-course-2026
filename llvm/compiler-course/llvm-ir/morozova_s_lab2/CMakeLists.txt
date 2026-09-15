get_filename_component(FOLDER_ID ${CMAKE_CURRENT_SOURCE_DIR} NAME)

if (NOT WIN32 AND NOT CYGWIN)
    add_llvm_pass_plugin(ReplacePowi ReplacePowi.cpp
        DEPENDS
        intrinsics_gen
        BUILDTREE_ONLY
    )
    set(LLVM_TEST_DEPENDS ReplacePowi ${LLVM_TEST_DEPENDS} PARENT_SCOPE)
endif()
