project(fifo-flim) 

cmake_minimum_required(VERSION 3.7)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(Qt5 COMPONENTS Widgets SerialPort REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

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
   LiveFlimReader.cpp
   FifoTcspc.cpp
   SimTcspc.cpp
   EventProcessor.cpp
   FlimFileWriter.cpp
)

set(HEADERS
   LiveFlimReader.h
   FifoTcspc.h
   FifoTcspcFactory.h
   TcspcEvent.h
   PacketBuffer.h
   SimTcspc.h
   EventProcessor.h
   FlimFileWriter.h
   PLIMLaserModulator.h
)

add_library(fifo-flim STATIC ${SOURCE} 
                             ${HEADERS} 
                             ${BH_SOURCE}
                             ${CL_SOURCE}      
                             ${UI_HEADERS} 
                             ${UI_RESOURCES})

target_compile_definitions(fifo-flim PUBLIC ${Cronologic_DEFINITIONS} ${BeckerHickl_DEFINITIONS})
target_include_directories(fifo-flim INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} 
                                     PUBLIC    ${BeckerHickl_INCLUDE_DIRS} 
                                               ${Cronologic_INCLUDE_DIRS} 
                                     PRIVATE   ${OpenCV_INCLUDE_DIRS})

target_link_libraries(fifo-flim ${OpenCV_LIBS} 
                                ${BeckerHickl_LIBRARIES} 
                                ${Cronologic_LIBRARIES}
                                FlimReader
                                InstrumentControl
                                Qt5::Widgets
                                Qt5::SerialPort)