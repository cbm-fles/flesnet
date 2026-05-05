#pragma once
#include "Monitor.hpp"
#include <cstdint>
#include <df/ConnectionManager.hpp>
#include <df/Node.hpp>
#include <df/WorkItems/WiConnection.hpp>
#include <df/WorkItems/WiTransmission.hpp>
#include <memory>
using namespace std::placeholders;
using namespace std;

class Sender : public Node {
private:
    std::mutex mtx;
    shared_ptr<cbm::Monitor> monitor{make_shared<cbm::Monitor>()};

public:
    Sender(uint64_t node_id, std::string monitoring_uri = "") : Node(node_id, 1) {

    }
 };