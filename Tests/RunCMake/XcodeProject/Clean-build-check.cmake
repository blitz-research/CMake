set(pattern "${RunCMake_TEST_BINARY_DIR}/build/empty.build/Debug/Objects-normal/*/empty.o")
file(GLOB objs "${pattern}")
if(NOT objs)
  set(RunCMake_TEST_FAILED "Expected object does not exist:\n ${pattern}")
endif()