#include <boost/proto/proto_fwd.hpp>
#include <boost/thread/pthread/thread_data.hpp>
#include <TimesliceWriter.hpp>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <getopt.h>
#include <chrono>
#include <memory>
#include <rdma/fabric.h>
#include <sched.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include "CentralManager.hpp"
#include "TsSender.hpp"
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
#include "TsReceiver.hpp"
using namespace std;

Parameters par;



int start_cm() {
    auto node = make_shared<CentralManager>(par.central_manager_listen_addr, par.monitoring_uri);
    cout << "Central Manager listening on: " << par.central_manager_listen_addr << endl;
    while (true) {
        this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}

int start_sender() {
    auto node = make_shared<TsSender>(
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
    auto node = make_shared<TsReceiver>(
        par.node_id,
        par.listen_addr,
        par.shm_uri,
        par.central_manager_listen_addr,
        par.monitoring_uri
    );
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
