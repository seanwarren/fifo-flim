project(fifo_flim) 

cmake_minimum_required(VERSION 2.8.10)

option(USE_BECKER_HICKL FALSE)
option(USE_CRONOLOGIC FALSE)

find_package(Qt5Widgets REQUIRED)
find_package(OpenCV REQUIRED)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

if(USE_BECKER_HICKL)
   set(BH_DIR "C:/Program Files (x86)/BH/SPCM/DLL")
   set(BH_SOURCE BH.cpp)
   set(BH_HEADERS BH.h)
   set(BH_LIB ${BH_DIR}/LIB/MSVC64/spcm64.lib)
   set(BH_INCLUDE ${BH_DIR})
endif()
if(USE_CRONOLOGIC)
   set(CL_DIR "C:/Program Files/cronologic/TimeTagger4/driver")
   set(CL_SOURCE cronologic.cpp)
   set(CL_HEADERS cronologic.h)
   set(CL_LIB ${fifo_flim_LIBS} ${CL_DIR}/x64/xtdc4_driver_64.lib)
   set(CL_INCLUDE ${CL_DIR}/include)
endif()


set(CMAKE_AUTOMOC ON)
add_definitions(${QT_DEFINITIONS}) 

set(SOURCE
   FLIMage.cpp
   FifoTcspc.cpp
   SimTcspc.cpp
   EventProcessor.cpp
   lz4.c
   lz4hc.c
   ${BH_SOURCE}
   ${CL_SOURCE}
)

set(HEADERS
   FifoTcspc.h
   FLIMage.h
   PacketBuffer.h
   SimTcspc.h
   EventProcessor.h
   lz4.h
   lz4hc.h
   LZ4Stream.h
   ${BH_HEADERS}
   ${CL_HEADERS}
)

include_directories(${BH_INCLUDE} ${CL_INCLUDE} ${Qt5Widgets_INCLUDE_DIRS} ${InstrumentControl_INCLUDE_DIR})

add_library(fifo_flim OBJECT ${SOURCE} ${HEADERS} ${UI_HEADERS} ${UI_RESOURCES})

set(fifo_flim_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ${InstrumentControl_INCLUDE_DIR} ${BH_INCLUDE} ${CL_INCLUDE} PARENT_SCOPE)
set(fifo_flim_LIBS ${OpenCV_LIBRARIES} ${InstrumentControl_LIBRARIES} ${BH_LIB} ${CL_LIB} PARENT_SCOPE)
