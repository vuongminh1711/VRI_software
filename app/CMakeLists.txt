add_executable(vri_app
  src/main.cc
  src/rf_helpers.cc
  src/cell_search.cc
  src/receive.cc
  src/transmit.cc
  src/monitor.cc
)
target_link_libraries(vri_app srsran_common srsran_phy srsran_rf pthread)
