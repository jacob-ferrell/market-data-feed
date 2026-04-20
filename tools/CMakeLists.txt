  add_executable(generate_messages generate_messages.cpp ../src/ticker_registry.cpp)
  target_include_directories(generate_messages PRIVATE ${CMAKE_SOURCE_DIR}/src)
