#pragma once

#include <cstdint>
#include <array>
#include <cstring>
#include <stdexcept>
#include <random>
#include <memory>
#include <condition_variable>
#include <mutex>
#include "behaviortree_cpp/basic_types.h"
#include "behaviortree_cpp/contrib/json.hpp"

namespace LINFA
{

/*
 * All the messages exchange with the BT executor are multipart ZMQ request-replies.
 *
 * The first part of the request and the reply have fixed size and are described below.
 * The request and reply must have the same value of the fields:
 *
 *  - request_id
 *  - request_type
 *  - protocol_id
 */

enum RequestType : uint8_t
{
  // Request the staus of the BT executor
  GET_STATUS = 'G',
  // Start the execution of the BT
  START = 'S',
  // Stop the execution of the BT
  STOP = 's',
  // Pause the execution of the BT
  PAUSE = 'P',
  // Resume the execution of the BT
  RESUME = 'p',

  UNDEFINED = 0,
};

inline const char* ToString(const RequestType& type)
{
  switch(type)
  {
    case RequestType::GET_STATUS:
      return "status";
    case RequestType::START:
      return "start";
    case RequestType::STOP:
      return "stop";
    case RequestType::PAUSE:
      return "pause";
    case RequestType::RESUME:
      return "resume";

    case RequestType::UNDEFINED:
      return "undefined";
  }
  return "undefined";
}

constexpr uint8_t kProtocolID = 2;
using TreeUniqueUUID = std::array<char, 16>;

struct RequestHeader
{
  uint32_t unique_id = 0;
  uint8_t protocol = kProtocolID;
  RequestType type = RequestType::UNDEFINED;

  static size_t size()
  {
    return sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t);
  }

  RequestHeader() = default;

  RequestHeader(RequestType type) : type(type)
  {
    // a random number for request_id will do
    static std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uint32_t> dist;
    unique_id = dist(mt);
  }

  bool operator==(const RequestHeader& other) const
  {
    return type == other.type && unique_id == other.unique_id;
  }
  bool operator!=(const RequestHeader& other) const
  {
    return !(*this == other);
  }
};

struct ReplyHeader
{
  RequestHeader request;

  static size_t size()
  {
    return RequestHeader::size();
  }

  ReplyHeader()
  {}
};

template <typename T>
inline unsigned Serialize(char* buffer, unsigned offset, T value)
{
  memcpy(buffer + offset, &value, sizeof(T));
  return sizeof(T);
}

template <typename T>
inline unsigned Deserialize(const char* buffer, unsigned offset, T& value)
{
  memcpy(reinterpret_cast<char*>(&value), buffer + offset, sizeof(T));
  return sizeof(T);
}

inline std::string SerializeHeader(const RequestHeader& header)
{
  std::string buffer;
  buffer.resize(6);
  unsigned offset = 0;
  offset += Serialize(buffer.data(), offset, header.protocol);
  offset += Serialize(buffer.data(), offset, uint8_t(header.type));
  offset += Serialize(buffer.data(), offset, header.unique_id);
  return buffer;
}

inline std::string SerializeHeader(const ReplyHeader& header)
{
  return SerializeHeader(header.request);
  // // copy the first part directly (6 bytes)
  // std::string buffer = SerializeHeader(header.request);
  // // add the following 16 bytes
  // unsigned const offset = 6;
  // buffer.resize(offset + 16);
  // Serialize(buffer.data(), offset, header.tree_id);
  // return buffer;
}

inline RequestHeader DeserializeRequestHeader(const std::string& buffer)
{
  RequestHeader header;
  unsigned offset = 0;
  offset += Deserialize(buffer.data(), offset, header.protocol);
  uint8_t type;
  offset += Deserialize(buffer.data(), offset, type);
  header.type = static_cast<RequestType>(type);
  offset += Deserialize(buffer.data(), offset, header.unique_id);
  return header;
}

inline ReplyHeader DeserializeReplyHeader(const std::string& buffer)
{
  ReplyHeader header;
  header.request = DeserializeRequestHeader(buffer);
  // unsigned const offset = 6;
  // Deserialize(buffer.data(), offset, header.tree_id);
  return header;
}

}  // namespace LINFA
