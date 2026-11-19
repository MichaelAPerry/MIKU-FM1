# SPDX-License-Identifier: GPL-3.0-only
execute_process(
  COMMAND "${VECTORS}"
  COMMAND "${NODE}" "${CMAKE_CURRENT_LIST_DIR}/test_mkx_js.js"
  RESULTS_VARIABLE statuses)
if(NOT statuses STREQUAL "0;0")
  message(FATAL_ERROR "MKX vector generator and Node exit codes: ${statuses}")
endif()
