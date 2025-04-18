#include "behaviortree_cpp/utils/strcat.hpp"
#include "behaviortree_cpp/xml_parsing.h"
#include "cppzmq/zmq.hpp"
#include "cppzmq/zmq_addon.hpp"
#include "linfa/manager.h"

namespace LINFA
{

struct Manager::PImpl
{
  PImpl() : context(), server(context, ZMQ_REP), publisher(context, ZMQ_PUB)
  {
    server.set(zmq::sockopt::linger, 0);
    publisher.set(zmq::sockopt::linger, 0);

    int timeout_rcv = 100;
    server.set(zmq::sockopt::rcvtimeo, timeout_rcv);
    publisher.set(zmq::sockopt::rcvtimeo, timeout_rcv);

    int timeout_ms = 1000;
    server.set(zmq::sockopt::sndtimeo, timeout_ms);
    publisher.set(zmq::sockopt::sndtimeo, timeout_ms);
  }

  unsigned server_port = 0;
  std::string server_address;
  std::string publisher_address;

  std::string tree_xml;
  StatusType status;

  std::atomic_bool active_server = false;
  std::thread server_thread;

  std::chrono::system_clock::time_point last_heartbeat;
  std::chrono::milliseconds max_heartbeat_delay = std::chrono::milliseconds(5000);

  std::thread heartbeat_thread;

  zmq::context_t context;
  zmq::socket_t server;
  zmq::socket_t publisher;
};

Manager::Manager(const BT::Tree& tree, unsigned server_port) : _p(new PImpl())
{
  _p->server_port = server_port;

  _p->tree_xml = WriteTreeToXML(tree, true, true);

  //-------------------------------
  // Prepare the status
  _p->status = StatusType::IDLE;

  //-------------------------------
  _p->server_address = BT::StrCat("tcp://0.0.0.0:", std::to_string(server_port));
  _p->publisher_address = BT::StrCat("tcp://0.0.0.0:", std::to_string(server_port + 1));

  _p->server.bind(_p->server_address.c_str());
  _p->publisher.bind(_p->publisher_address.c_str());

  _p->server_thread = std::thread(&Manager::serverLoop, this);
  _p->heartbeat_thread = std::thread(&Manager::heartbeatLoop, this);
}

void Manager::setMaxHeartbeatDelay(std::chrono::milliseconds delay)
{
  _p->max_heartbeat_delay = delay;
}

std::string Manager::getXmlTree() const
{
  return _p->tree_xml;
}

StatusType Manager::getStatus() const
{
  return _p->status;
}

void Manager::setStatus(const StatusType status)
{
  _p->status = status;
}

std::chrono::milliseconds Manager::maxHeartbeatDelay() const
{
  return _p->max_heartbeat_delay;
}

Manager::~Manager()
{
  _p->active_server = false;
  if(_p->server_thread.joinable())
  {
    _p->server_thread.join();
  }

  if(_p->heartbeat_thread.joinable())
  {
    _p->heartbeat_thread.join();
  }

  flush();
}

void Manager::flush()
{
  // nothing to do here...
}

void Manager::serverLoop()
{
  _p->active_server = true;
  auto& socket = _p->server;

  auto sendErrorReply = [&socket](const std::string& msg) {
    zmq::multipart_t error_msg;
    error_msg.addstr("error");
    error_msg.addstr(msg);
    error_msg.send(socket);
  };

  // initialize _p->last_heartbeat
  _p->last_heartbeat = std::chrono::system_clock::now();

  while(_p->active_server)
  {
    zmq::multipart_t requestMsg;
    if(!requestMsg.recv(socket) || requestMsg.size() == 0)
    {
      continue;
    }

    // this heartbeat will help establishing if Groot is connected or not
    _p->last_heartbeat = std::chrono::system_clock::now();

    std::string const request_str = requestMsg[0].to_string();
    if(request_str.size() != RequestHeader::size())
    {
      sendErrorReply("wrong request header: received size " +
                     std::to_string(request_str.size()) + ", expected size " +
                     std::to_string(RequestHeader::size()));
      continue;
    }

    auto request_header = DeserializeRequestHeader(request_str);

    ReplyHeader reply_header;
    reply_header.request = request_header;
    reply_header.request.protocol = kProtocolID;

    zmq::multipart_t reply_msg;
    reply_msg.addstr(SerializeHeader(reply_header));

    switch(request_header.type)
    {
      case RequestType::GET_BTCPP_VERSION: {
        // Use the compile-time constant for the package version
        reply_msg.addstr(BTCPP_LIBRARY_VERSION);
        break;
      }
      case RequestType::GET_MANAGER_VERSION: {
        // TODO: replace with the version of the library if detached from BT.CPP
        // Use the compile-time constant for the package version
        reply_msg.addstr(BTCPP_LIBRARY_VERSION);
        break;
      }
      case RequestType::GET_STATUS: {
        std::string status = toStatusStr(_p->status);
        reply_msg.addstr(status);
        break;
      }
      case RequestType::START: {
        _p->status = StatusType::STARTING;
        break;
      }

      case RequestType::STOP: {
        if(_p->status == StatusType::RUNNING || _p->status == StatusType::PAUSED)
        {
          _p->status = StatusType::STOPPING;
        }
        break;
      }

      case RequestType::PAUSE: {
        if(_p->status == StatusType::RUNNING)
        {
          _p->status = StatusType::PAUSED;
        }
        break;
      }

      case RequestType::RESUME: {
        if(_p->status == StatusType::PAUSED)
        {
          _p->status = StatusType::RUNNING;
        }
        break;
      }

      case RequestType::SET_TREE: {
        if(requestMsg.size() != 2)
        {
          sendErrorReply("must be 2 parts message");
          continue;
        }

        if(_p->status != StatusType::IDLE)
        {
          sendErrorReply("Cannot change tree while running");
          continue;
        }

        _p->tree_xml = requestMsg[1].to_string();
        break;
      }

      case RequestType::GET_TREE: {
        reply_msg.addstr(_p->tree_xml);
        break;
      }

      default: {
        sendErrorReply("Request not recognized");
        continue;
      }
    }
    // send the reply
    reply_msg.send(socket);
  }
}

void Manager::heartbeatLoop()
{
  bool has_heartbeat = true;

  while(_p->active_server)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto now = std::chrono::system_clock::now();
    bool prev_heartbeat = has_heartbeat;

    has_heartbeat = (now - _p->last_heartbeat < _p->max_heartbeat_delay);

    // if we loose or gain heartbeat, disable/enable all breakpoints
    if(has_heartbeat != prev_heartbeat)
    {
      //   enableAllHooks(has_heartbeat);
    }
  }
}
}  // namespace LINFA
