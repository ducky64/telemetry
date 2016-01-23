#include "telemetry.h"

namespace telemetry {

namespace protocol {

template<> uint8_t numeric_subtype<uint8_t>() {
  return NUMERIC_SUBTYPE_UINT;
}
template<> uint8_t numeric_subtype<uint16_t>() {
  return NUMERIC_SUBTYPE_UINT;
}
template<> uint8_t numeric_subtype<uint32_t>() {
  return NUMERIC_SUBTYPE_UINT;
}

template<> uint8_t numeric_subtype<int8_t>() {
  return NUMERIC_SUBTYPE_SINT;
}
template<> uint8_t numeric_subtype<int16_t>() {
  return NUMERIC_SUBTYPE_SINT;
}
template<> uint8_t numeric_subtype<int32_t>() {
  return NUMERIC_SUBTYPE_SINT;
}

template<> uint8_t numeric_subtype<float>() {
  return NUMERIC_SUBTYPE_FLOAT;
}

template<> uint8_t numeric_subtype<double>() {
  return NUMERIC_SUBTYPE_FLOAT;
}

}

}
