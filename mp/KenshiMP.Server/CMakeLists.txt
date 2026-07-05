# Server logic as a static library so it can run either as the dedicated
# server exe (main.cpp) or embedded inside the game process (listen server —
# Core's EmbeddedServer wraps GameServer on a background thread).
add_library(KenshiMP.ServerLib STATIC
    server.cpp
    upnp.cpp
    game_state.cpp
    player_manager.cpp
    entity_manager.cpp
    zone_manager.cpp
    combat_resolver.cpp
    world_persistence.cpp
    authority_validator.cpp
)

target_include_directories(KenshiMP.ServerLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(KenshiMP.ServerLib PUBLIC
    KenshiMP.Common
    enet
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    ws2_32
    winmm
    ole32
    oleaut32
    iphlpapi
)

# Dedicated server executable — console front-end over ServerLib
add_executable(KenshiMP.Server main.cpp)
target_link_libraries(KenshiMP.Server PRIVATE KenshiMP.ServerLib)

# Copy server exe to Kenshi directory so it can be run standalone
get_filename_component(KENSHI_DIR "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
if(EXISTS "${KENSHI_DIR}/kenshi_x64.exe")
    add_custom_command(TARGET KenshiMP.Server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:KenshiMP.Server>"
            "${KENSHI_DIR}/KenshiMP.Server.exe"
        COMMENT "Copying KenshiMP.Server.exe to Kenshi directory"
    )
endif()
