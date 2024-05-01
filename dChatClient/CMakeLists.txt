add_executable(ChatClient ChatClient.cpp)
target_link_libraries(ChatClient raknet dCommon)

add_executable(ChatHttpClient ChatHttpClient.cpp)
target_link_libraries(ChatHttpClient raknet dCommon)
