set(DCHATFILTER_SOURCES "dChatFilter.h" "dChatFilter.cpp")

add_library(dChatFilter STATIC ${DCHATFILTER_SOURCES})
target_include_directories(dChatFilter PUBLIC .)
target_link_libraries(dChatFilter PUBLIC dDatabase)
