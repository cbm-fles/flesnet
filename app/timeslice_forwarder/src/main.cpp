
#include "CentralManager.hpp"
#include "TsSender.hpp"
#include "TsReceiver.hpp"
#include "Parameters.hpp"

#include <getopt.h>
#include <chrono>
#include <memory>
#include <rdma/fabric.h>
#include <sched.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>

using namespace std;


Parameters par;

int start_cm(string hostname = "") {
    auto node = make_shared<CentralManager>(par.central_manager_listen_addr, par.monitoring_uri, hostname);
    L_(info) << "Central Manager listening on: " << par.central_manager_listen_addr;
    while (true) {
        this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 0;
}

int start_sender(string hostname = "") {
    auto node = make_shared<TsSender>(
        par.node_id,
        par.listen_addr,
        par.input_uri,
        par.central_manager_listen_addr,
        par.monitoring_uri,
        hostname
    );
    L_(info) << "Started TS sender (" << par.listen_addr << ")";

    while (true) {
        this_thread::sleep_for(chrono::milliseconds(2000));
    }
    return 0;
}

int start_receiver(string hostname = "") {
    L_(info) << "Starting receiver...";
    auto node = make_shared<TsReceiver>(
        par.node_id,
        par.listen_addr,
        par.output_uri,
        par.timeslice_size,
        par.central_manager_listen_addr,
        par.monitoring_uri,
        hostname
    );
    L_(info) << "Started TS receiver (" << par.listen_addr << ")";

    while (true) {
        this_thread::sleep_for(chrono::milliseconds(3000));
    }
    return 0;
}


/**
 * @brief Signal handler for profiling.
 * Profiling data is only written if the program exits gracefully, that is why this minimal signal handler is here.
 */
void signalHandler(int sig) {
    exit(sig);
}

int main (int argc, char** argv) {
    signal(SIGINT, signalHandler);

    par.parse_options(argc, argv);
    if (FI_VERSION(FI_MAJOR_VERSION, FI_MINOR_VERSION) != fi_version()) {
        L_(fatal) << "Libfabric: Header version and library version do not match";
        L_(fatal) << "Header: " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION;
        L_(fatal) << "Lib: " << FI_MAJOR(fi_version()) << "." << FI_MINOR(fi_version());
        exit(-1);
    } else if (FI_MAJOR_VERSION != 2 || FI_MINOR_VERSION < 2) {
        L_(fatal) << "Libfabric: invalid version.";
        L_(fatal) << "Minimum version required 2.2 - found " << FI_MAJOR_VERSION << "." << FI_MINOR_VERSION;
        exit(-1);
    }
    const uint16_t HOSTNAME_LEN = 50;
    char c_hostname[HOSTNAME_LEN] = {0};
    gethostname(&c_hostname[0], HOSTNAME_LEN);
    string hostname(&c_hostname[0]);
    if (par.role == Parameters::CentralManager) {
        start_cm(hostname);
    } else if (par.role == Parameters::Sender) {
        start_sender(hostname);
    } else if (par.role == Parameters::Receiver) {
        start_receiver(hostname);
    } else { // Should never happen
        L_(fatal) << "! Was unable to determine if node is sender, receiver or central manager";
    }

    return 0;
}
