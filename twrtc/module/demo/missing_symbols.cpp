#include "api/test/create_frame_generator.h"
#include "api/video/i420_buffer.h"
#include "rtc_base/strings/json.h"
#include <json/json.h>
#include "frame_generator.h"

namespace rtc {

bool GetIntFromJsonObject(const Json::Value& in, absl::string_view k, int* out) {
  std::string key(k);
  if (!in.isMember(key) || !in[key].isInt()) {
    return false;
  }
  *out = in[key].asInt();
  return true;
}

bool GetStringFromJsonObject(const Json::Value& in, absl::string_view k, std::string* out) {
  std::string key(k);
  if (!in.isMember(key) || !in[key].isString()) {
    return false;
  }
  *out = in[key].asString();
  return true;
}

} // namespace rtc

namespace webrtc {
namespace test {
std::unique_ptr<FrameGeneratorInterface> CreateSquareFrameGenerator(
    int width,
    int height,
    std::optional<FrameGeneratorInterface::OutputType> type,
    std::optional<int> num_squares) {
  return std::make_unique<SquareGenerator>(
      width, height, type.value_or(FrameGeneratorInterface::OutputType::kI420),
      num_squares.value_or(10));
}

} // namespace test
} // namespace webrtc
