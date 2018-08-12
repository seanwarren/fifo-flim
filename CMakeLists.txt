project(fifo_flim) 

cmake_minimum_required(VERSION 3.7)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt5Widgets REQUIRED)
find_package(Qt5SerialPort REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

find_package(Cronologic)
if(Cronologic_FOUND)
   set(CL_SOURCE cronologic.h cronologic.cpp)
endif()

find_package(BeckerHickl)
if(BeckerHickl_FOUND)
   set(BH_SOURCE BH.h BH.cpp)
endif()

set(CMAKE_AUTOMOC ON)
cmake_policy(SET CMP0071 OLD)

set(SOURCE
   FLIMage.cpp
   FifoTcspc.cpp
   SimTcspc.cpp
   EventProcessor.cpp
   FlimFileWriter.cpp
   FlimDataSource.cpp
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
#   LZ4Stream.h
#   LZ4ThreadedStream.h
   PLIMLaserModulator.h
)

add_library(fifo_flim STATIC ${SOURCE} 
                             ${HEADERS} 
                             ${UI_HEADERS} 
                             ${UI_RESOURCES})

target_compile_definitions(fifo_flim PUBLIC ${Cronologic_DEFINITIONS} ${BeckerHickl_DEFINITIONS})
target_include_directories(fifo_flim INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} 
                                     PUBLIC    ${BeckerHickl_INCLUDE_DIRS} 
                                               ${Cronologic_INCLUDE_DIRS} 
                                     PRIVATE   ${OpenCV_INCLUDE_DIRS} 
                                               ${Qt5Widgets_INCLUDE_DIRS} 
                                               ${Qt5SerialPort_INCLUDE_DIRS})

target_link_libraries(fifo_flim ${OpenCV_LIBS} 
                                ${BeckerHickl_LIBRARIES} 
                                ${Cronologic_LIBRARIES}
                                InstrumentControl)