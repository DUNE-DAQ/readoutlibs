/**
 * @file DataMoveCallbackRegistry.hpp
 *
 * This is part of the DUNE DAQ , copyright 2024.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef READOUTLIBS_INCLUDE_READOUTLIBS_DATAMOVECALLBACKREGISTRY_HPP_
#define READOUTLIBS_INCLUDE_READOUTLIBS_DATAMOVECALLBACKREGISTRY_HPP_

#include "logging/Logging.hpp"

#include <functional>
#include <memory>
#include <string>
#include <map>

namespace dunedaq {
namespace readoutlibs {

class CallbackConcept {
public:
  explicit CallbackConcept(std::string id)
    : m_id(id)
  {
  }
  virtual ~CallbackConcept() = default;
  std::string id() const { return m_id; }
protected:
  std::string m_id;
};

template<typename DataType>
class DataMoveCallback : public CallbackConcept
{
public:
  DataMoveCallback(std::string id, std::function<void(DataType&&)> callback)
    : CallbackConcept(id)
  {
    m_callback = std::make_shared<std::function<void(DataType&&)>>(callback);
  }
  std::shared_ptr<std::function<void(DataType&&)>> m_callback;
};

class DataMoveCallbackRegistry
{
public:
  static std::shared_ptr<DataMoveCallbackRegistry> get()
  {
    if (!s_instance)
      s_instance = std::shared_ptr<DataMoveCallbackRegistry>(new DataMoveCallbackRegistry());

    return s_instance;
  }

  DataMoveCallbackRegistry(const DataMoveCallbackRegistry&) = delete;            ///< DataMoveCallbackRegistry is not copy-constructible
  DataMoveCallbackRegistry& operator=(const DataMoveCallbackRegistry&) = delete; ///< DataMoveCallbackRegistry is not copy-assignable
  DataMoveCallbackRegistry(DataMoveCallbackRegistry&&) = delete;                 ///< DataMoveCallbackRegistry is not move-constructible
  DataMoveCallbackRegistry& operator=(DataMoveCallbackRegistry&&) = delete;      ///< DataMoveCallbackRegistry is not move-assignable 

  template<typename DataType>
  void register_callback(const std::string& id, std::function<void(DataType&&)> callback);
 
  template<typename DataType>
  std::shared_ptr<std::function<void(DataType&&)>>
  get_callback(const std::string& id);

private:
  DataMoveCallbackRegistry() {}
  std::map<std::string, std::shared_ptr<CallbackConcept>> m_callback_map;
  static std::shared_ptr<DataMoveCallbackRegistry> s_instance;
};

} // namespace readoutlibs
} // namespace dunedaq

#include "detail/DataMoveCallbackRegistry.hxx"

#endif // READOUTLIBS_INCLUDE_READOUTLIBS_DATAMOVECALLBACKREGISTRY_HPP_
