#include "crossdoor_nodes.h"
#include "behaviortree_cpp/bt_factory.h"
// #include "behaviortree_cpp/loggers/bt_file_logger_v2.h"
// #include "behaviortree_cpp/loggers/bt_minitrace_logger.h"
#include "behaviortree_cpp/loggers/groot2_publisher.h"
#include "behaviortree_cpp/xml_parsing.h"
#include "behaviortree_cpp/json_export.h"
#include "linfa/manager.h"

const unsigned GROOT_PORT = 1667;
const unsigned MANAGER_PORT = 1670;

/** We are using the same example in Tutorial 5,
 *  But this time we also show how to connect
 */

// A custom struct  that I want to visualize in Groot2
struct Position2D
{
  double x;
  double y;
};

// This macro will generate the code that is needed to convert
// the object to/from JSON.
// You still need to call BT::RegisterJsonDefinition<Position2D>()
// in main()
BT_JSON_CONVERTER(Position2D, pos)
{
  add_field("x", &pos.x);
  add_field("y", &pos.y);
}

// Simple Action that updates an instance of Position2D in the blackboard
class UpdatePosition : public BT::SyncActionNode
{
public:
  UpdatePosition(const std::string& name, const BT::NodeConfig& config)
    : BT::SyncActionNode(name, config)
  {}

  BT::NodeStatus tick() override
  {
    _pos.x += 0.2;
    _pos.y += 0.1;
    setOutput("pos", _pos);
    return BT::NodeStatus::SUCCESS;
  }

  static BT::PortsList providedPorts()
  {
    return { BT::OutputPort<Position2D>("pos") };
  }

private:
  Position2D _pos = { 0, 0 };
};

// clang-format off

static const char* xml_text = R"(
<root BTCPP_format="4">

  <BehaviorTree ID="MainTree">
    <Sequence>
      <Script code="door_open:=false" />
      <UpdatePosition pos="{pos_2D}" />
      <Fallback>
        <Inverter>
          <IsDoorClosed/>
        </Inverter>
        <SubTree ID="DoorClosed" _autoremap="true" door_open="{door_open}"/>
      </Fallback>
      <PassThroughDoor/>
    </Sequence>
  </BehaviorTree>

  <BehaviorTree ID="DoorClosed">
    <Fallback name="tryOpen" _onSuccess="door_open:=true">
      <OpenDoor/>
        <RetryUntilSuccessful num_attempts="5">
          <PickLock/>
        </RetryUntilSuccessful>
      <SmashDoor/>
    </Fallback>
  </BehaviorTree>

</root>
 )";

// clang-format on

int main()
{
  BT::BehaviorTreeFactory factory;

  // Nodes registration, as usual
  CrossDoor cross_door;
  cross_door.registerNodes(factory);
  factory.registerNodeType<UpdatePosition>("UpdatePosition");

  // Groot2 editor requires a model of your registered Nodes.
  // You don't need to write that by hand, it can be automatically
  // generated using the following command.
  const std::string xml_models = BT::writeTreeNodesModelXML(factory);

  factory.registerBehaviorTreeFromText(xml_text);

  // Add this to allow Groot2 to visualize your custom type
  BT::RegisterJsonDefinition<Position2D>();

  auto tree = factory.createTree("MainTree");

  // Connect the LinfaManager and Groot2Publisher.
  LINFA::Manager* manager = new LINFA::Manager(tree, MANAGER_PORT);
  BT::Groot2Publisher* publisher = new BT::Groot2Publisher(tree, GROOT_PORT);

  // TODO: add logging
  // // Logging with lightweight serialization
  // BT::FileLogger2 logger2(tree, "t19_logger2.btlog");
  // BT::MinitraceLogger minilog(tree, "minitrace.json");

  while(1)
  {
    switch(manager->getStatus())
    {
      case LINFA::StatusType::IDLE:
      case LINFA::StatusType::PAUSED:
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        break;

      case LINFA::StatusType::STARTING:
        std::cout << "Starting" << std::endl;

        factory.registerBehaviorTreeFromText(manager->getXmlTree());
        tree = factory.createTree("MainTree");

        std::cout << "----------- XML file  ----------\n"
                  << BT::WriteTreeToXML(tree, false, false)
                  << "--------------------------------\n";

        // reset the publisher
        delete publisher;
        publisher = new BT::Groot2Publisher(tree, GROOT_PORT);

        manager->setStatus(LINFA::StatusType::RUNNING);

        // TODO: update logging
        // logger2 = BT::FileLogger2(tree, "t19_logger2.btlog");
        // minilog = BT::MinitraceLogger(tree, "minitrace.json");

        break;

      case LINFA::StatusType::RUNNING:
        std::cout << "Running" << std::endl;

        cross_door.reset();
        tree.tickWhileRunning();
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        break;

      case LINFA::StatusType::STOPPING:
        std::cout << "Stopping" << std::endl;

        // TODO: flush the logs
        // logger2.flush();
        // minilog.flush();

        manager->setStatus(LINFA::StatusType::IDLE);

        break;

      default:
        std::cout << "Unhandled Linfa Manager Status: " << manager->getStatus()
                  << std::endl;
    }
  }

  return 0;
}
