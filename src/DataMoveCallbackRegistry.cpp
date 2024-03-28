/**
 * @file DataMoveCallbackRegistry.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2024.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "readoutlibs/DataMoveCallbackRegistry.hpp"

#include <memory>

std::shared_ptr<dunedaq::readoutlibs::DataMoveCallbackRegistry> dunedaq::readoutlibs::DataMoveCallbackRegistry::s_instance = nullptr;

