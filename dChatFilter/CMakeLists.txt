set(DCHATFILTER_SOURCES "dChatFilter.cpp")

add_library(dChatFilter ${DCHATFILTER_SOURCES})
target_link_libraries(dChatFilter dDatabase)
