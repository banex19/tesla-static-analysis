find_package(Threads)

option(TESLA_PERFORMANCE "Use build settings for performance analysis" OFF)
if(TESLA_PERFORMANCE)
  set(PERF_FLAGS "-pg" "-O3")
else()
  set(PERF_FLAGS "")
endif()

function(add_tesla_executable C_SOURCES EXE_NAME STATIC)
  set(TESLA_LINK "-Wl,-rpath,/home/test/tesla_install/lib" "-L/home/test/tesla_install/lib" "-ltesla")

  set(TESLA_INCLUDE "-I/home/test/tesla_install/include")

  set(CMAKE_C_FLAGS "-g ${CMAKE_C_FLAGS}")
  set(CMAKE_C_FLAGS "-DTESLA ${CMAKE_C_FLAGS}")
  string(REPLACE " " ";" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
  set(TESLA_FILES)
  set(LL_FILES)
  set(INSTR_FILES)
  foreach(source ${C_SOURCES})
    get_filename_component(name ${source} NAME_WE)
    set(full_source "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    set(prefix ${EXE_NAME}_${name})

    add_custom_command(
      OUTPUT ${prefix}.tesla
      COMMAND tesla analyse ${full_source} -o ${prefix}.tesla -- ${CMAKE_C_FLAGS} ${TESLA_INCLUDE}
      DEPENDS ${full_source}
    )
    add_custom_target(${prefix}_tesla
      ALL DEPENDS ${prefix}.tesla
    )
    list(APPEND TESLA_FILES ${prefix}.tesla)

    add_custom_command(
      OUTPUT ${prefix}.bc
      COMMAND ${CMAKE_C_COMPILER}
      ARGS ${CMAKE_C_FLAGS} -S -emit-llvm ${TESLA_INCLUDE} ${full_source} -o ${prefix}.bc
      DEPENDS ${full_source}
    )
    add_custom_target(${prefix}_ll
      ALL DEPENDS ${prefix}.bc
    )
    list(APPEND LL_FILES ${prefix}.bc)
  endforeach()

  add_custom_command(
    OUTPUT ${EXE_NAME}.bc
    COMMAND llvm-link34 ${LL_FILES} -o ${EXE_NAME}.bc
    DEPENDS ${LL_FILES}
  )
  add_custom_target(${EXE_NAME}_lto
    ALL DEPENDS ${EXE_NAME}.bc
  )

  add_custom_command(
    OUTPUT ${EXE_NAME}.instr.bc
    COMMAND tesla instrument -tesla-manifest ${EXE_NAME}.manifest ${EXE_NAME}.bc -o ${EXE_NAME}.instr.bc
    DEPENDS ${EXE_NAME}.bc ${EXE_NAME}.manifest
  )
  add_custom_target(${EXE_NAME}_instr
    ALL DEPENDS ${EXE_NAME}.instr.bc
  )

  add_custom_command(
    OUTPUT ${EXE_NAME}.manifest
    COMMAND tesla cat ${TESLA_FILES} -o ${EXE_NAME}.manifest
    DEPENDS ${TESLA_FILES}
  )
  add_custom_target(${EXE_NAME}-manifest
    ALL DEPENDS ${EXE_NAME}.manifest
  )

  if(STATIC AND CMAKE_USE_STATIC)
    add_custom_command(
      OUTPUT ${EXE_NAME}.static.manifest
      COMMAND tesla static ${EXE_NAME}.manifest ${EXE_NAME}.bc -modelcheck -bound=1000 -o ${EXE_NAME}.static.manifest
      DEPENDS ${EXE_NAME}.manifest ${EXE_NAME}.bc
    )
    add_custom_target(${EXE_NAME}-static-manifest
      ALL_DEPENDS ${EXE_NAME}.static.manifest
    )
    
    add_custom_command(
      OUTPUT ${EXE_NAME}.static.instr.bc
      COMMAND tesla instrument -tesla-manifest ${EXE_NAME}.static.manifest ${EXE_NAME}.bc -o ${EXE_NAME}.static.instr.bc
      DEPENDS ${EXE_NAME}.bc ${EXE_NAME}.static.manifest
    )
    add_custom_target(${EXE_NAME}_static-instr
      ALL DEPENDS ${EXE_NAME}.static.instr.bc
    )

    set(INSTR_FILE ${EXE_NAME}.static.instr.bc)
    add_custom_command(
      OUTPUT ${EXE_NAME}_static
      COMMAND ${CMAKE_C_COMPILER} ${PERF_FLAGS} ${CMAKE_C_FLAGS} ${CMAKE_THREAD_LIBS_INIT} ${TESLA_LINK} ${INSTR_FILE} -o ${EXE_NAME}_static
      DEPENDS ${INSTR_FILE}
    )
    add_custom_target(${EXE_NAME}-static-tesla-all
      ALL DEPENDS ${EXE_NAME}_static
    )
  endif()

  set(INSTR_FILE ${EXE_NAME}.instr.bc)
  add_custom_command(
    OUTPUT ${EXE_NAME}
    COMMAND ${CMAKE_C_COMPILER} ${PERF_FLAGS} ${CMAKE_C_FLAGS} ${CMAKE_THREAD_LIBS_INIT} ${TESLA_LINK} ${INSTR_FILE} -o ${EXE_NAME}
    DEPENDS ${INSTR_FILE}
  )
  add_custom_target(${EXE_NAME}-tesla-all
    ALL DEPENDS ${EXE_NAME}
  )
endfunction()
