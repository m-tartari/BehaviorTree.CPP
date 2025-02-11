#pragma once

#include <array>
#include <future>
#include "linfa/manger_protocol.h"

namespace LINFA
{
enum StatusType : uint8_t
{
  // The BT executor is idle
  IDLE = 'I',
  // The BT executor is starting
  STARTING = 'S',
  // The BT executor is running
  RUNNING = 'R',
  // The BT executor is paused
  PAUSED = 'P',
  // The BT executor is stopping
  STOPPING = 's',
};

std::string toStatusStr(const StatusType status)
{
  switch(status)
  {
    case StatusType::IDLE:
      return "IDLE";
    case StatusType::STARTING:
      return "STARTING";
    case StatusType::RUNNING:
      return "RUNNING";
    case StatusType::PAUSED:
      return "PAUSED";
    case StatusType::STOPPING:
      return "STOPPING";
  }
  return "";
}

/**
 * @brief The Manager is used to create an interface between
 * your BT.CPP executor and Linfa_BT.
 *
 * An inter-process communication mechanism allows the two processes
 * to communicate through a TCP port. The user should provide the
 * port to be used in the constructor.
 */
class Manager
{
public:
  Manager(const BT::Tree& tree, unsigned server_port = 1667);

  ~Manager();

  Manager(const Manager& other) = delete;
  Manager& operator=(const Manager& other) = delete;

  Manager(Manager&& other) = default;
  Manager& operator=(Manager&& other) = default;

  /**
   * @brief setMaxHeartbeatDelay is used to tell the publisher
   * when a connection with Groot2 should be cancelled, if no
   * heartbeat is received.
   *
   * Default is 5000 ms
   */
  void setMaxHeartbeatDelay(std::chrono::milliseconds delay);
  std::chrono::milliseconds maxHeartbeatDelay() const;

  void setStatus(const StatusType status);
  StatusType getStatus() const;

  void setXmlTree(std::string xml_tree);
  std::string getXmlTree() const;

private:
  void flush();

  void serverLoop();

  void heartbeatLoop();

  void updateStatus();

  struct PImpl;
  std::unique_ptr<PImpl> _p;
};

}  // namespace LINFA
