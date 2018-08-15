zephyr_interface_library_named(KNOT)

target_include_directories(KNOT INTERFACE src)

zephyr_library()
zephyr_library_sources(
  src/knot_protocol.c
)

zephyr_library_link_libraries(KNOT)
target_link_libraries(KNOT INTERFACE zephyr_interface)
