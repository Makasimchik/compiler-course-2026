get_filename_component(CURRENT_DIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

set(TARGET_NAME "noexcept")

file(GLOB_RECURSE SOURCES *.cpp *.h *.hpp)
add_llvm_library(${TARGET_NAME} MODULE ${SOURCES} PLUGIN_TOOL clang)

set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")

if(WIN32 OR CYGWIN)
    set(LLVM_LINK_COMPONENTS Support)
    clang_target_link_libraries(${TARGET_NAME} PRIVATE
        clangAST
        clangBasic
        clangFrontend
        clangAnalysis
    )
endif()

if(UNIX AND NOT WIN32)
    if(APPLE)
        set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".dylib")
    else()
        set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".so")
    endif()
endif()

set(CLANG_TEST_DEPS "${TARGET_NAME}" ${CLANG_TEST_DEPS} PARENT_SCOPE)
