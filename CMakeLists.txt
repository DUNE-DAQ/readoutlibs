cmake_minimum_required(VERSION 3.12)
project(readoutlibs VERSION 1.5.3)

find_package(daq-cmake REQUIRED)

daq_setup_environment()

find_package(ers REQUIRED)
find_package(appfwk REQUIRED)
find_package(logging REQUIRED)
find_package(iomanager REQUIRED)
find_package(opmonlib REQUIRED)
find_package(daqdataformats REQUIRED)
find_package(dfmessages REQUIRED)
find_package(folly REQUIRED)
find_package(Boost COMPONENTS iostreams unit_test_framework REQUIRED)
set(BOOST_LIBS Boost::iostreams ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_LIBRARIES})

daq_codegen( readoutconfig.jsonnet sourceemulatorconfig.jsonnet recorderconfig.jsonnet TEMPLATES Structs.hpp.j2 Nljs.hpp.j2 )
daq_codegen( *info.jsonnet DEP_PKGS opmonlib TEMPLATES opmonlib/InfoStructs.hpp.j2 opmonlib/InfoNljs.hpp.j2 )

##############################################################################
# Dependency sets
set(READOUT_DEPENDENCIES
#tools
  Folly::folly
  ers::ers
  logging::logging
#dunedaq
  appfwk::appfwk
  opmonlib::opmonlib
  daqdataformats::daqdataformats
  dfmessages::dfmessages
)

##############################################################################
# Extra options and tweaks
set(READOUT_USE_LIBNUMA ON)

if(${READOUT_USE_LIBNUMA})
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(numa REQUIRED IMPORTED_TARGET "numa")
  list(APPEND READOUT_DEPENDENCIES numa)
  #list(APPEND READOUT_DEPENDENCIES ${numa_LINK_LIBRARIES})
  add_compile_definitions(WITH_LIBNUMA_SUPPORT)
endif()

##############################################################################
# Main library
daq_add_library(
  *.cpp
  LINK_LIBRARIES ${READOUT_DEPENDENCIES}
)


if(${READOUT_USE_LIBNUMA})
  target_include_directories(readoutlibs PUBLIC ${numa_INCLUDE_DIRS})
endif()

##############################################################################
# Integration tests
daq_add_application(readoutlibs_test_ratelimiter test_ratelimiter_app.cxx TEST LINK_LIBRARIES readoutlibs)
daq_add_application(readoutlibs_test_lb_allocation test_lb_allocation_app.cxx TEST LINK_LIBRARIES readoutlibs)
daq_add_application(readoutlibs_test_bufferedfilewriter test_bufferedfilewriter_app.cxx TEST LINK_LIBRARIES readoutlibs ${BOOST_LIBS})
daq_add_application(readoutlibs_test_bufferedfilereader test_bufferedfilereader_app.cxx TEST LINK_LIBRARIES readoutlibs ${BOOST_LIBS})
daq_add_application(readoutlibs_test_skiplist test_skiplist_app.cxx TEST LINK_LIBRARIES readoutlibs ${BOOST_LIBS})
daq_add_application(readoutlibs_test_composite_key test_composite_key_app.cxx TEST LINK_LIBRARIES readoutlibs ${BOOST_LIBS})

##############################################################################
# Unit Tests

daq_add_unit_test(readoutlibs_BufferedReadWrite_test LINK_LIBRARIES readoutlibs ${BOOST_LIBS})
#daq_add_unit_test(readoutlibs_VariableSizeElementQueue_test LINK_LIBRARIES readoutlibs ${BOOST_LIBS})

##############################################################################
# Installation
daq_install()
