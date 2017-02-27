project(fifo_flim) 

cmake_minimum_required(VERSION 3.7)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5SerialPort REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

find_package(Cronologic)
add_definitions(${Cronologic_DEFINITIONS}) 
if(Cronologic_FOUND)
   set(CL_SOURCE cronologic.h cronologic.cpp)
endif()

find_package(BeckerHickl)
add_definitions(${BeckerHickl_DEFINITIONS}) 
if(BeckerHickl_FOUND)
   set(BH_SOURCE BH.h BH.cpp)
endif()

set(CMAKE_AUTOMOC ON)
add_definitions(${QT_DEFINITIONS}) 

set(SOURCE
   FLIMage.cpp
   FifoTcspc.cpp
   SimTcspc.cpp
   EventProcessor.cpp
   FlimFileWriter.cpp
   FlimDataSource.cpp
   lz4.c
   lz4hc.c
   ${BH_SOURCE}
   ${CL_SOURCE}
)

set(HEADERS
   FlimDataSource.h
   FifoTcspc.h
   FifoTcspcFactory.h
   FLIMage.h
   TcspcEvent.h
   PacketBuffer.h
   SimTcspc.h
   EventProcessor.h
   FlimFileWriter.h
   FlimFileReader.h
   lz4.h
   lz4hc.h
   LZ4Stream.h
   LZ4ThreadedStream.h
   PLIMLaserModulator.h
)

set(COMMON_INCLUDE ${BeckerHickl_INCLUDE_DIRS} 
                   ${Cronologic_INCLUDE_DIRS} 
                   ${OpenCV_INCLUDE_DIRS} 
                   ${InstrumentControl_INCLUDE_DIR})

include_directories(${Qt5Widgets_INCLUDE_DIRS} 
                    ${Qt5SerialPort_INCLUDE_DIRS} 
                    ${COMMON_INCLUDE})

add_library(fifo_flim OBJECT ${SOURCE} 
                             ${HEADERS} 
                             ${UI_HEADERS} 
                             ${UI_RESOURCES})

set(fifo_flim_DEFINITIONS ${Cronologic_DEFINITIONS} ${BeckerHickl_DEFINITIONS} PARENT_SCOPE)
set(fifo_flim_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ${COMMON_INCLUDE} PARENT_SCOPE)
set(fifo_flim_LIBS ${OpenCV_LIBS} 
                   ${InstrumentControl_LIBRARIES} 
                   ${BeckerHickl_LIBRARIES} 
                   ${Cronologic_LIBRARIES} 
                   PARENT_SCOPE)