# Sources will be added here as you implement each milestone
# Example structure:
#
# add_library(feed_lib
#     feed_handler.cpp
#     order_book.cpp
#     ring_buffer.cpp
#     allocator.cpp
# )
# target_include_directories(feed_lib PUBLIC ${PROJECT_SOURCE_DIR}/include)
#
# add_executable(feed_processor main.cpp)
# target_link_libraries(feed_processor PRIVATE feed_lib pthread)
add_executable(market_data_feed main.cpp parser.cpp)
