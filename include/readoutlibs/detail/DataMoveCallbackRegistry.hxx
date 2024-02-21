#include <functional>

namespace dunedaq {

namespace readoutlibs {

template<typename DataType>
inline void 
DataMoveCallbackRegistry::register_callback(const std::string& id, std::function<void(DataType&&)> callback) {
  if (m_callback_map.count(id) == 0) {
    TLOG() << "Registering DataMoveCallback with ID: " << id;
    m_callback_map[id] = std::make_shared<DataMoveCallback<DataType>>(DataMoveCallback(id, callback));
  } else {
    TLOG() << "Callback is already registered with ID: " << id << " Ignoring this registration.";
  }
}

template<typename DataType>
inline std::shared_ptr<std::function<void(DataType&&)>>
DataMoveCallbackRegistry::get_callback(const std::string& id) {
  if (m_callback_map.count(id) != 0) {
    TLOG() << "Providing DataMoveCallback with ID: " << id;
    auto callback = dynamic_cast<DataMoveCallback<DataType>*>(m_callback_map[id].get());
    return callback->m_callback;
  } else {
    TLOG() << "No callback registered with ID: " << id << " Returning nullptr.";
    return nullptr;
  }
}

}
}

