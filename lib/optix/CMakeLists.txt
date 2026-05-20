include(GNUInstallDirs)

add_library(optix_plugin_sdk INTERFACE)
target_include_directories(optix_plugin_sdk INTERFACE
  ${PROJECT_SOURCE_DIR}/include
)

add_library(optix_core STATIC
  core/types.cpp
  core/metadata.cpp
  core/artifact_io.cpp
  core/event_bus.cpp
  core/plugin_manager.cpp
  core/pipeline_orchestrator.cpp
  stages/ingest_stage.cpp
  stages/normalize_stage.cpp
  stages/diagnose_stage.cpp
  stages/index_stage.cpp
  stages/rule_stage.cpp
  stages/suggest_stage.cpp
)
set_target_properties(optix_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(optix_core PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(optix_core PUBLIC optix_plugin_sdk)
if(UNIX)
  target_link_libraries(optix_core PRIVATE dl)
endif()

if(OPTIX_ENABLE_LIBTOOLING)
  find_package(Clang CONFIG REQUIRED)
  target_compile_definitions(optix_core PRIVATE OPTIX_HAS_LIBTOOLING=1)
  if(DEFINED CLANG_INCLUDE_DIRS)
    target_include_directories(optix_core PRIVATE ${CLANG_INCLUDE_DIRS})
  endif()
  if(DEFINED LLVM_INCLUDE_DIRS)
    target_include_directories(optix_core PRIVATE ${LLVM_INCLUDE_DIRS})
  endif()

  set(_optix_has_clang_link_target FALSE)
  if(TARGET clang-cpp)
    target_link_libraries(optix_core PRIVATE clang-cpp)
    set(_optix_has_clang_link_target TRUE)
  else()
    foreach(_t clangTooling clangASTMatchers clangBasic clangFrontend)
      if(TARGET ${_t})
        target_link_libraries(optix_core PRIVATE ${_t})
        set(_optix_has_clang_link_target TRUE)
      endif()
    endforeach()
  endif()

  if(NOT _optix_has_clang_link_target)
    message(FATAL_ERROR
      "Clang package found but no usable LibTooling link targets were exported. "
      "Expected clang-cpp or clangTooling/clangASTMatchers/clangBasic/clangFrontend.")
  endif()

  if(TARGET LLVM)
    target_link_libraries(optix_core PRIVATE LLVM)
  endif()
endif()

add_library(optix_plugins_ksys_parser SHARED plugins/parser/ksys_parser_plugin.cpp)
target_link_libraries(optix_plugins_ksys_parser PRIVATE optix_core)
set_target_properties(optix_plugins_ksys_parser PROPERTIES PREFIX "")

add_library(optix_plugins_default_rulepack SHARED plugins/rulepack/default_rulepack_plugin.cpp)
target_link_libraries(optix_plugins_default_rulepack PRIVATE optix_core)
set_target_properties(optix_plugins_default_rulepack PROPERTIES PREFIX "")

add_library(optix_plugins_markdown_reporter SHARED plugins/reporter/markdown_reporter_plugin.cpp)
target_link_libraries(optix_plugins_markdown_reporter PRIVATE optix_core)
set_target_properties(optix_plugins_markdown_reporter PROPERTIES PREFIX "")

add_library(optix_plugins_unified_diff_transformer SHARED plugins/transformer/unified_diff_transformer_plugin.cpp)
target_link_libraries(optix_plugins_unified_diff_transformer PRIVATE optix_core)
set_target_properties(optix_plugins_unified_diff_transformer PROPERTIES PREFIX "")

add_library(optix_plugins_basic_advisor SHARED plugins/advisor/basic_advisor_plugin.cpp)
target_link_libraries(optix_plugins_basic_advisor PRIVATE optix_core)
set_target_properties(optix_plugins_basic_advisor PROPERTIES PREFIX "")

install(TARGETS
  optix_plugins_ksys_parser
  optix_plugins_default_rulepack
  optix_plugins_markdown_reporter
  optix_plugins_unified_diff_transformer
  optix_plugins_basic_advisor
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/optix/plugins
)
