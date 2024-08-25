if (WHISPER_SDL2)
    # command
    set(TARGET assistant)
    add_executable(${TARGET} assistant.cpp)

    include(DefaultTargetOptions)

    target_link_libraries(${TARGET} PRIVATE common common-sdl whisper ${CMAKE_THREAD_LIBS_INIT})
endif ()
