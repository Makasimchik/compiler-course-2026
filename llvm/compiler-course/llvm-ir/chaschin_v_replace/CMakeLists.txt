cmake_minimum_required(VERSION 3.16)

get_filename_component(DIR_BASENAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PLUGIN_TARGET_NAME "${DIR_BASENAME}_Pass")

if (NOT WIN32 AND NOT CYGWIN)
    add_llvm_pass_plugin(${PLUGIN_TARGET_NAME} resuct_shifts.cpp
        DEPENDS
        intrinsics_gen
        BUILDTREE_ONLY
    )
    
    set(LLVM_TEST_DEPENDS ${PLUGIN_TARGET_NAME} ${LLVM_TEST_DEPENDS} PARENT_SCOPE)
endif()
