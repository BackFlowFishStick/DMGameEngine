#include "DMGameEngine/Core/Layer.h"

namespace DMGameEngine {

Layer::Layer(const std::string& name, LayerType type)
    : m_name(name)
    , m_type(type) {
}

} // namespace DMGameEngine
