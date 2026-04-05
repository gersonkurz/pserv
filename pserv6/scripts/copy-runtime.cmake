if(NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "SRC and DST must be defined")
endif()

file(GLOB_RECURSE PAYLOAD_FILES
    "${SRC}/*.dll"
    "${SRC}/*.mui"
    "${SRC}/*.png"
    "${SRC}/*.winmd"
    "${SRC}/*.xaml"
    "${SRC}/*.xbf"
    "${SRC}/*.pri"
    "${SRC}/*.html"
)

file(GLOB RESTART_AGENT "${SRC}/restartAgent.exe")
list(APPEND PAYLOAD_FILES ${RESTART_AGENT})

foreach(F IN LISTS PAYLOAD_FILES)
    file(RELATIVE_PATH REL "${SRC}" "${F}")
    get_filename_component(REL_DIR "${REL}" DIRECTORY)
    file(COPY "${F}" DESTINATION "${DST}/${REL_DIR}")
endforeach()

