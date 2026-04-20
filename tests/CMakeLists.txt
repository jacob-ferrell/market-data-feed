add_executable(test_order_book
    test_order_book.cpp
    ../src/order_book.cpp
    ../src/flat_hash_map.cpp
    ../src/ticker_registry.cpp
)
target_include_directories(test_order_book PRIVATE ${CMAKE_SOURCE_DIR}/src)
