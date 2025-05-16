#include <cstdint>
#include <cstdlib>
#include <getopt.h>
#include <chrono>
#include <memory>
#include <ratio>
#include <thread>
#include <unistd.h>
#include <vector>
// #include <df/Scheduler/SchedulerA.hpp>
// #include <df/SchedulerInstructionDecoder/SidSchedulerA.hpp>

#include "ConnectorFromFlesnet.hpp"
#include <df/CentralManagers/CentralManager.hpp>
#include <df/Connectors/ConnectorInfinibandMsg.hpp>
#include <df/WorkItems/WiData.hpp>
#include <df/WorkItems/WorkItem.hpp>
// #include <df/
#include <df/WorkItems/WiConnectorConfig.hpp>
#include <df/Buffers/BufferFifo.hpp>
#include <df/Node.hpp>
#include <df/Buffers/BufferFifo.hpp>
#include <df/Connectors/ConnectorFile.hpp>
#include <df/Connectors/ConnectorEthernetTCP.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <df/InterfaceFactory.hpp>
#include <iostream>
using namespace std;

int main (int argc, char** argv) {
    InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet> connector_factory;
    // if (p.is_node) { // we start as a node
    //     shared_ptr<Node> node(new Node(p.node_id,p.group_id));

    //     // set buffer - Currently not set using program options
    //     shared_ptr<BufferFifo> buffer(new BufferFifo);
    //     node->set_buffer(buffer);

    //     // set node connectors
    //     std::vector<WiConnectorConfig> connector_configs = p.connectors;
    //     for (auto &c : connector_configs) {
    //         shared_ptr<ConnectorInterface> connector = connector_factory.get(c.name);
    //         if (!connector) {
    //             std::cerr << "Unable to find connector: " << c.name << std::endl;
    //             return -1;
    //         }
    //         node->add_connector(connector, c.listen_addr, c.connector_uid);
    //     }


    //     // set central manager connector if it was given
    //     shared_ptr<SidSchedulerA> sic_scheduler_a(new SidSchedulerA);
    //     if (p.central_manager.name.length() > 0) {
    //         shared_ptr<ConnectorInterface> cm_connector = connector_factory.get(p.central_manager.name);

    //         if (!cm_connector) {
    //             std::cerr << "Unable to find central manager connector: " << p.output.name << std::endl;
    //             return -1;
    //         }
    //         string cm_address = split(p.central_manager.listen_addr, ';')[0];
    //         node->set_cm_connector(cm_connector,  cm_address);
    //         sic_scheduler_a->set_cm_connector(cm_connector,  cm_address);
    //         sic_scheduler_a->set_node(node);
    //         sic_scheduler_a->set_buffer(buffer);

    //         cm_connector->connect_to_server(cm_address);
    //     }

    //     this_thread::sleep_for(std::chrono::milliseconds(500));
    //     // set input if it was given
    //     if (p.input.name.length() > 0) {
    //         shared_ptr<ConnectorInterface> input_connector = connector_factory.get(p.input.name);
    //         if (!input_connector) {
    //             std::cerr << "Unable to find input connector: " << p.input.name << std::endl;
    //             return -1;
    //         }
    //         node->set_input_connector(input_connector, p.input.listen_addr);
    //     }

    //     // set output if it was given
    //     if (p.output.name.length() > 0) {
    //         shared_ptr<ConnectorInterface> output_connector = connector_factory.get(p.output.name);
    //         if (!output_connector) {
    //             std::cerr << "Unable to find output connector: " << p.output.name << std::endl;
    //             return -1;
    //         }
    //         node->set_output_connector(output_connector, p.output.listen_addr);
    //     }

    //     while(true) {
    //         this_thread::sleep_for(std::chrono::milliseconds(1000));
    //     }
    // } else { // we as the CentralManager
    //     shared_ptr<ConnectorInterface> cm_connector = connector_factory.get(p.central_manager.name);
    //     cm_connector->listen_for_clients(p.central_manager.listen_addr);
    //     if (!cm_connector) {
    //         std::cerr << "Unable to find central manager connector: " << p.central_manager.name << std::endl;
    //         return -1;
    //     }
    //     shared_ptr<SchedulerA> scheduler(new SchedulerA);
    //     CentralManager cm(cm_connector, scheduler);
    //     while (cm_connector->get_listen_address().empty()) { // Wait till started ...
    //         this_thread::sleep_for(std::chrono::milliseconds(100));
    //     }

    //     cout << "Central Manager running at: " << cm_connector->get_listen_address() << endl;

    //     while (true) {// block to keep the CentralManager alive
    //         this_thread::sleep_for(std::chrono::milliseconds(1000));
    //     }
    // }


    return 0;
}