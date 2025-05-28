#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <getopt.h>
#include <chrono>
#include <memory>
#include <ratio>
#include <rdma/fabric.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>
// #include <df/Scheduler/SchedulerA.hpp>
// #include <df/SchedulerInstructionDecoder/SidSchedulerA.hpp>
#include <sys/stat.h>        /* For mode constants */
#include "ConnectorFromFlesnet.hpp"
#include "Parameters.hpp"
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
#include <df/Connectors/ConnectorInfinibandRmaV.hpp>

#include <iostream>
// #define DEFAULT_CONNECTOR_CLASSES ConnectorEthernetTCP, ConnectorInfinibandMsg, ConnectorFile, ConnectorInfinibandMsgRmaNew
#define DEFAULT_CM_CLASSES CentralManager

using namespace std;
// Parameters par;


int main (int argc, char** argv) {
    Parameters par(argc, argv);
    if (FI_VERSION(FI_MAJOR_VERSION, FI_MINOR_VERSION) != fi_version()) {
        cerr << "Libfabric: Header version and library version do not match" << endl;
        cerr << "Header: " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION << endl;
        cerr << "Lib: " << FI_MAJOR(fi_version()) << "." << FI_MINOR(fi_version()) << endl;
        exit(-1);
    } else if (FI_MAJOR_VERSION != 2 || FI_MINOR_VERSION < 1) {
        cerr << "Libfabric: invalid version." << endl;
        cerr << "Minimum version required 2.1 - found " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION << endl;
    }
    const uint64_t cm_buffer_size = static_cast<uint64_t>(1024) * 1024 * 5;
    std::shared_ptr<char> cm_buffer = std::shared_ptr<char>(new char[cm_buffer_size]);
    memset(cm_buffer.get(), 0, cm_buffer_size);

    if (par.is_node) { // start data node
        if (!par.shm_name.empty()) {  // input node
            cout <<"Preparing input node ..." << endl;

            // PREPARE NODE
            std::shared_ptr<Node> node = std::make_shared<Node>(par.node_id, par.group_id);
            std::shared_ptr<uint64_t> node_uid = std::make_shared<uint64_t>(MAKE_UID(par.group_id, par.node_id));
            InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet, ConnectorInfinibandRmaV> connector_factory;

            // PREPARE FLESNET INPUT CONNECTOR
            auto connector_fles_in = connector_factory.get("ConnectorFromFlesnet");
            connector_fles_in->listen_for_clients(par.shm_name);
            std::shared_ptr<char >buffer_ptr = nullptr;
            cout << "Waiting for Buffer to get initialized (" << par.shm_name << ") ..." << endl;
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // to initialize the buffer inside the connector it first has to receive a TS - best guess is to wait 500ms for now
                buffer_ptr = connector_fles_in->get_global_buffer();
            } while (buffer_ptr == nullptr);
            cout << "Buffer initialized!" << endl;

            auto buffer_size = connector_fles_in->get_global_buffer_size();
            // uint64_t buffer_size = 1024 * 1024;
            // std::string shm_id = "tsforwarder";
            // auto shm_fd = shm_open(shm_id.c_str(), O_RDWR | O_CREAT, 0666);
            // if (shm_fd == 0) {
            //     cerr << "failed to create andaccess shm" << endl;
            //     exit(-1);
            // }
            // ftruncate(shm_fd, buffer_size);
            // auto buffer_ptr = std::shared_ptr<char>(reinterpret_cast<char*>(mmap(nullptr, buffer_size, PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0)));
            // if (buffer_ptr == nullptr) {
            //     cerr << "mmap failed" << endl;
            //     exit(-1);
            // }
            // PREPARE BUFFER MAP
            auto buffer_map = make_shared<BufferMap>(64, buffer_size);

            // PREPARE NODE CONNECTORS
            for (auto c : par.connectors) {
                std::shared_ptr<ConnectorInterface> connector = connector_factory.get(c.name);
                if (!connector) {
                    std::cerr << "Invalid connector name: " << c.name << std::endl <<
                        "Did you add it to the template parameter if the InterfaceFactory?" << std::endl;
                    return -1;
                }
                // connector->set_memory_manager(mem_mng);
                connector->set_global_buffer(buffer_ptr, buffer_size);
                connector->set_global_buffer_map(buffer_map);

                node->add_connector(
                    connector,
                    c.connector_uid,
                    c.listen_addr,
                    std::reinterpret_pointer_cast<char>(node_uid),
                    sizeof(uint64_t)
                );
            }

            std::string cm_connector_name = par.central_manager.name;
            std::shared_ptr<ConnectorInterface> cm_connector = connector_factory.get(cm_connector_name);
            if (cm_connector == nullptr) {
                std::cerr << "Invalid CM Connector name: " << cm_connector_name << std::endl <<
                    "Did you add it to the template parameter if the InterfaceFactory?" << std::endl;
                exit(-1);
            }

            // const uint64_t cm_buffer_size = static_cast<uint64_t>(1024) * 1024;
            std::shared_ptr<char> cm_buffer = std::shared_ptr<char>(new char[cm_buffer_size]);
            memset(cm_buffer.get(), 0, cm_buffer_size);
            if (cm_connector->set_global_buffer(cm_buffer, cm_buffer_size) != 0) {
                cerr << "CM connector: unable to set global buffer" <<  endl;
                exit(-1);
            }

            auto cm_buffer_map = make_shared<BufferMap>(64, cm_buffer_size);
            cm_connector->set_global_buffer_map(cm_buffer_map);

            std::string cm_address = par.central_manager.listen_addr;
            node->set_cm_connector(cm_connector, cm_address);

            // PREPARE FLESNET INPUT CONNECTOR
            // connector_fles_in->set_global_buffer(buffer_ptr, buffer_size);
            node->set_input_connector(connector_fles_in, "");
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            connector_fles_in->set_global_buffer_map(buffer_map);
            cout << "Input node ready" << endl;
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else { // output node
            cout <<"Preparing output node ..." << endl;

            // PREPARE NODE
            std::shared_ptr<Node> node = std::make_shared<Node>(par.node_id, par.group_id);
            std::shared_ptr<uint64_t> node_uid = std::make_shared<uint64_t>(MAKE_UID(par.group_id, par.node_id));
            InterfaceFactory<ConnectorInterface, ConnectorFromFlesnet, ConnectorInfinibandRmaV> connector_factory;

            // PREPARE FLESNET INPUT CONNECTOR
            uint64_t buffer_size = static_cast<long>(1024 * 1024) * 16;
            auto buffer_ptr = std::shared_ptr<char>(new char[buffer_size]);
            auto buffer_map = make_shared<BufferMap>(64, buffer_size);

            // auto buffer_size = connector_fles_in->get_global_buffer_size();
            // uint64_t buffer_size = 1024 * 1024;
            // std::string shm_id = "tsforwarder";
            // auto shm_fd = shm_open(shm_id.c_str(), O_RDWR | O_CREAT, 0666);
            // if (shm_fd == 0) {
            //     cerr << "failed to create andaccess shm" << endl;
            //     exit(-1);
            // }
            // ftruncate(shm_fd, buffer_size);
            // auto buffer_ptr = std::shared_ptr<char>(reinterpret_cast<char*>(mmap(nullptr, buffer_size, PROT_WRITE | PROT_READ, MAP_SHARED, shm_fd, 0)));
            // if (buffer_ptr == nullptr) {
            //     cerr << "mmap failed" << endl;
            //     exit(-1);
            // }
            // PREPARE BUFFER MAP

            // PREPARE NODE CONNECTORS
            for (auto c : par.connectors) {
                std::shared_ptr<ConnectorInterface> connector = connector_factory.get(c.name);
                if (!connector) {
                    std::cerr << "Invalid connector name: " << c.name << std::endl <<
                        "Did you add it to the template parameter if the InterfaceFactory?" << std::endl;
                    return -1;
                }
                // connector->set_memory_manager(mem_mng);
                connector->set_global_buffer(buffer_ptr, buffer_size);
                connector->set_global_buffer_map(buffer_map);

                node->add_connector(
                    connector,
                    c.connector_uid,
                    c.listen_addr,
                    std::reinterpret_pointer_cast<char>(node_uid),
                    sizeof(uint64_t)
                );
            }

            std::string cm_connector_name = par.central_manager.name;
            std::shared_ptr<ConnectorInterface> cm_connector = connector_factory.get(cm_connector_name);
            if (cm_connector == nullptr) {
                std::cerr << "Invalid CM Connector name: " << cm_connector_name << std::endl <<
                    "Did you add it to the template parameter if the InterfaceFactory?" << std::endl;
                exit(-1);
            }

            // const uint64_t cm_buffer_size = static_cast<uint64_t>(1024) * 1024;
            std::shared_ptr<char> cm_buffer = std::shared_ptr<char>(new char[cm_buffer_size]);
            memset(cm_buffer.get(), 0, cm_buffer_size);
            if (cm_connector->set_global_buffer(cm_buffer, cm_buffer_size) != 0) {
                cerr << "CM connector: unable to set global buffer" <<  endl;
                exit(-1);
            }

            auto cm_buffer_map = make_shared<BufferMap>(64, cm_buffer_size);
            cm_connector->set_global_buffer_map(cm_buffer_map);

            std::string cm_address = par.central_manager.listen_addr;
            node->set_cm_connector(cm_connector, cm_address);

            // PREPARE FLESNET INPUT CONNECTOR
            // connector_fles_in->set_global_buffer(buffer_ptr, buffer_size);
            cout << "Output node ready" << endl;
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    } else { // start Central Manager (CM)
        cout << "Starting Central Manager ..." << endl;

        InterfaceFactory<CentralManagerInterface, DEFAULT_CM_CLASSES> central_manager_factory;
        //! @todo get the type of central manager from options
        std::string cm_name = "CentralManager";
        std::shared_ptr<CentralManagerInterface> central_manager = central_manager_factory.get(cm_name);
        if (!central_manager) {
            std::cerr << "Invalid Central Manager name: " << cm_name << std::endl <<
                "Did you add it to the template parameter if the InterfaceFactory?";
            return -1;
        }

        // InterfaceFactory<ConnectorInterface, ConnectorInfinibandRmaV> connector_factory;
        // std::shared_ptr<ConnectorInterface> connector = connector_factory.get(par.central_manager.name);
        auto connector =  make_shared<ConnectorInfinibandRmaV>();

        if (!connector) {
            std::cerr << "Invalid Connector name: " << par.central_manager.name << std::endl <<
                "Did you add it to the template parameter if the InterfaceFactory?" << std::endl;
            return -1;
        }

        connector->set_global_buffer(cm_buffer, cm_buffer_size);

        auto cm_buffer_map = make_shared<BufferMap>(64, cm_buffer_size);
        connector->set_global_buffer_map(cm_buffer_map);
        central_manager->add_connector(connector, par.central_manager.listen_addr);
        while (connector->get_listen_address().empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Running Central Manager: " << par.central_manager.listen_addr << std::endl;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }


    return 0;
}