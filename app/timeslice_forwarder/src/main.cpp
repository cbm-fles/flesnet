#include <atomic>
#include <boost/proto/proto_fwd.hpp>
#include <boost/thread/pthread/thread_data.hpp>
#include <TimesliceWriter.hpp>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <future>
#include <getopt.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <rdma/fabric.h>
#include <sched.h>
#include <shared_mutex>
#include <string>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "CentralManager.hpp"
#include "Metric.hpp"
#include "Monitor.hpp"
#include "Sender.hpp"
// #include "TimesliceReader.hpp"
#include "TsclientWriter.hpp"
#include "Tssink.hpp"
#include <sys/stat.h>        /* For mode constants */
#include "Parameters.hpp"
#include <df/WorkItems/WiData.hpp>
#include <df/WorkItems/WorkItem.hpp>
#include <df/EvaluationLogic/EvaluationLogic.hpp>
#include <df/WorkItems/WiConnectorConfig.hpp>
#include <df/Node.hpp>
#include <df/BufferMap/BufferMap.hpp>
#include <df/WorkerThread.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <df/InterfaceFactory.hpp>
#include <df/Connectors/ConnectorInfiniband.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <df/Connectors/ConnectorInterface.hpp>
#include <df/ConnectionManager.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <iostream>
#include <MonitorSinkInflux2.hpp>
using namespace std;

Parameters par;
// constexpr uint64_t BUFFER_MAP_ELEMENTS = 512;
// constexpr uint64_t DATA_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 450;
// constexpr uint64_t WI_BUFFER_SIZE = static_cast<uint64_t>(1024 * 1024) * 5;



int start_cm() {
    auto node = make_shared<CentralManager>(par.monitoring_uri);
    const auto cm_address = par.central_manager_listen_addr;

    // const auto node_connector = connector_factory.get("");
    std::shared_ptr<ConnectorInterface> node_connector = make_shared<ConnectorInfiniband>();
    const auto node_listen_addr = par.central_manager_listen_addr;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    const auto data_buffer = std::shared_ptr<char>(new char[DATA_BUFFER_SIZE], std::default_delete<char[]>());
    const auto data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, DATA_BUFFER_SIZE);

    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, DATA_BUFFER_SIZE);
    node->add_connector(node_connector, node_listen_addr);

    node->start();
    cout << "Central Manager listening on: " << node_listen_addr << endl;
    while (true) {
        this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}

int start_sender() {
    auto node = make_shared<Sender>(
        par.node_id,
        par.listen_addr,
        par.shm_uri,
        par.central_manager_listen_addr,
        par.monitoring_uri
    );
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(2000));
    }
    return 0;
}

int start_receiver() {
    auto monitor = make_shared<cbm::Monitor>();
    if (par.is_monitoring_enabled) {
        monitor->OpenSink(par.monitoring_uri);
    }

    auto node_id = par.node_id;
    auto group_id = par.group_id;
    auto node = make_shared<Node>(node_id, group_id);
    const auto cm_address = par.central_manager_listen_addr;

    // const auto node_connector = connector_factory.get(par.connectors[0].name);
    const auto node_connector = make_shared<ConnectorInfiniband>();

    const auto node_listen_addr = par.listen_addr;
    cout << "Started as receiver (" << node_listen_addr << ")" << endl;

    const auto wi_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, WI_BUFFER_SIZE);
    const auto wi_buffer = std::shared_ptr<char>(new char[WI_BUFFER_SIZE], std::default_delete<char[]>());

    // cout << "tswriter init" << endl;
    shared_ptr<TsSink> ts_sink = nullptr;
    std::shared_ptr<char> data_buffer = nullptr;
    std::shared_ptr<char> data_buffer_size = nullptr;
    uint64_t buffer_size = 0;
    std::function<void(std::string /*address*/, uint64_t /*group_id*/, uint64_t /*node_id*/)> on_new_work_item;
    std::shared_ptr<BufferMap> data_buffer_map = nullptr;

    if (true) { // Hardcoded switch for manual debugging
        ts_sink = make_shared<TsclientWriter>(par.shm_uri);
        data_buffer = ts_sink->get_buffer();
        buffer_size = ts_sink->get_buffer_size();
    } else {
        ts_sink = make_shared<TimesliceWriter>("tsout.tsa");
        buffer_size = DATA_BUFFER_SIZE;
        data_buffer = std::shared_ptr<char>(new char[buffer_size], std::default_delete<char[]>());
        ts_sink->set_buffer(data_buffer);
    }

    data_buffer_map = make_shared<BufferMap>(BUFFER_MAP_ELEMENTS, buffer_size);
    /**
     * The shared memory is represented by boost::managed_shared_memory.
     * Boost seems to store some metadata for its management in the SHM too.
     * It seems to be constant 336 byte. Therefore this needs to be represented in the buffer map too.
     */
    data_buffer_map->insert(0, 336, BufferMap::TAG_UNSET);
    node->set_wi_buffer(wi_buffer, wi_buffer_map, WI_BUFFER_SIZE);
    node->set_data_buffer(data_buffer, data_buffer_map, buffer_size);
    node->add_connector(node_connector, node_listen_addr);

    // Currently unused
    node->on_new_work_item([] (std::string /*address*/, std::shared_ptr<char> /*wi_ptr*/, WorkItem::Type /*wi_type*/, uint64_t group_id, uint64_t node_id) {
        if (group_id == 0 && node_id == 0) { // received new work item from central manager

        }
    });

    node->on_node_connected([&node_listen_addr, node, node_connector, &node_id, &group_id, &cm_address] (string address, uint64_t rem_group_id, uint64_t rem_node_id) {
        cout << "node connected: \n" <<
                "Group ID: " << rem_group_id << '\n' <<
                "Node ID: " << rem_node_id  << endl;

        if (rem_group_id == 0 && rem_node_id == 0) { // connected to central manager - tell it about our connection possibilities
            auto conn_config = make_shared<WiConnectorConfig>();
            conn_config->type = WorkItem::connector_config;
            conn_config->connector_uid = 0;
            conn_config->listen_addr = node_listen_addr;
            conn_config->name = "ConnectorInfiniband";
            node->send_work_item(address, conn_config);
        } else { // Connected to some other node - tell the central manager about it
            auto wi_connection = make_shared<WiConnection>();
            wi_connection->type = WorkItem::connection;
            wi_connection->from_group_id = group_id;
            wi_connection->from_node_id = node_id;
            wi_connection->to_group_id = rem_group_id;
            wi_connection->to_node_id = rem_node_id;
            node->send_work_item(cm_address, wi_connection, [] () {
                cout << "send work item done (WorkItem::connection)" << endl;
            });
        }
    });

    node->on_connection_refused([node, cm_address] (std::string address) {
        cout << "connection refused" << endl;
        if (address == cm_address) {
            node->connect_to_node(cm_address);
        }
    });
    atomic_uint64_t failed_self_locks = 0;
    atomic_uint64_t recv_cnt  = 0;
    node->on_new_data([data_buffer_map, node_connector, ts_sink, &failed_self_locks, monitor, &recv_cnt] (const std::string& /*address*/, uint64_t group_id, uint64_t node_id) {
        // New data has arrived - check buffer map
        monitor->QueueMetric("timeslice_forwarder_state",
                        {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                        {{"recv_cnt", ++recv_cnt}});
        cout << "New data from Node ID: " << node_id << " - Group ID: " << group_id << endl;
        node_connector->lock_buffer_map(data_buffer_map, [data_buffer_map, node_connector, ts_sink, monitor] () {
            auto *el = data_buffer_map->get_oldest_linked_list_element(nullptr, BufferMap::ListElement::IO::RX);
            if (el == nullptr) { // not expected to happen in the current implementation
                node_connector->unlock_buffer_map(data_buffer_map);
                return;
            }
            uint64_t component_size = 0;
            auto component = data_buffer_map->get_elements_of_component(el->compontent_id, component_size);
            std::cout << "component.size: " << component.size() << endl;
            monitor->QueueMetric("timeslice_forwarder_state",
                {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                {{"bytes_received", component_size}});
            data_buffer_map->print_all();
            cout << "write_timeslice start " << endl;
            ts_sink->write_timeslice(component);
            cout << "write_timeslice done" << endl;

            data_buffer_map->remove_elements(component);
            node_connector->unlock_buffer_map(data_buffer_map);
        }, [&failed_self_locks, monitor] () {
            monitor->QueueMetric("timeslice_forwarder_state",
                        {{"host", std::to_string(par.node_id) + " - " + std::to_string(par.group_id)}},
                        {{"failed_self_locks", ++failed_self_locks}});
            cout << "failed self lock: " << failed_self_locks << endl;
            return true;
        });
    });
    node->start();

    cout << "Connecting to CM at: " << cm_address << endl;
    node->connect_to_node(cm_address);
    while (true) {
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
    return 0;
}


int main (int argc, char** argv) {
    par.parse_options(argc, argv);
    if (FI_VERSION(FI_MAJOR_VERSION, FI_MINOR_VERSION) != fi_version()) {
        cerr << "Libfabric: Header version and library version do not match" << endl;
        cerr << "Header: " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION << endl;
        cerr << "Lib: " << FI_MAJOR(fi_version()) << "." << FI_MINOR(fi_version()) << endl;
        exit(-1);
    } else if (FI_MAJOR_VERSION != 2 || FI_MINOR_VERSION < 2) {
        cerr << "Libfabric: invalid version." << endl;
        cerr << "Minimum version required 2.2 - found " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION << endl;
    }

    if (par.group_id == 0) {
        start_cm();
    } else if (par.group_id == 1) {
        start_sender();
    } else if (par.group_id == 2) {
        start_receiver();
    } else { // Should never happen
        cerr << "! Was unable to determine if node is sender, receiver or central manager" << endl;
    }

    return 0;
}
