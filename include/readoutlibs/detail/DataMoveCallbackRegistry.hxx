#include <functional>

namespace dunedaq {

namespace readoutlibs {

template<typename DataType>
inline void 
DataMoveCallbackRegistry::register_callback(const std::string& id, std::function<void(DataType&&)> callback) {
  m_callback_map[id] = std::make_shared<DataMoveCallback<DataType>>(DataMoveCallback(id, callback));
}

template<typename DataType>
inline std::unique_ptr<std::function<void(DataType&&)>>&
DataMoveCallbackRegistry::get_callback(const std::string& id) {
  auto callback = dynamic_cast<DataMoveCallback<DataType>*>(m_callback_map[id].get());
  return callback->m_callback;
}

}
}

