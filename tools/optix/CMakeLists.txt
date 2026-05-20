add_executable(optix_cli main.cpp)
target_link_libraries(optix_cli PRIVATE optix_core)
set_target_properties(optix_cli PROPERTIES OUTPUT_NAME optix)

include(GNUInstallDirs)
install(TARGETS optix_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
