#include "client/client.hpp"
#include "client/clientNetwork.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>

namespace py = pybind11;

class PyClient {
public:
    PyClient(std::array<uint8_t, constants::HMAC_SIZE> hmacKey,
             std::array<uint8_t, 16> apiKey, const std::string& serverIP = "127.0.0.1",
             uint16_t port = 12345)
        : client_(hmacKey, apiKey), net_(serverIP, port, client_), running_(true) {}

    bool connect() {
        if (!net_.connectServer()) return false;
        running_ = true;

        // Launch heartbeat loop detached
        hbThread_ = std::thread([this]() { heartbeatLoop(); });
        hbThread_.detach();

        // Launch receive loop detached
        recThread_ = std::thread([this]() { receiveLoop(); });
        recThread_.detach();

        return true;
    }

    void disconnect() {
        running_ = false;
        net_.disconnectServer();
        if (hbThread_.joinable()) hbThread_.join();
        if (recThread_.joinable()) recThread_.join();
    }

    void send_hello() {
        client_.sendHello();
        net_.sendMessage();
    }
    void send_logout() {
        client_.sendLogout();
        net_.sendMessage();
    }

    void send_order(uint64_t qty, double price, bool isBuy, bool isLimit) {
        client_.sendOrder(qty, price, isBuy, isLimit);
        net_.sendMessage();
    }

    void send_cancel(uint64_t orderID) {
        client_.sendCancel(orderID);
        net_.sendMessage();
    }
    void send_modify(uint64_t orderID, uint64_t qty, double price) {
        client_.sendModify(orderID, qty, price);
        net_.sendMessage();
    }

    std::vector<uint8_t> read_recv_buffer() const { return client_.readRecBuffer(); }
    std::vector<uint8_t> read_send_buffer() const { return client_.readSendBuffer(); }
    bool is_running() const { return running_; }

private:
    void heartbeatLoop(int intervalSeconds = 5) {
        while (running_) {
            client_.sendHeartbeat();
            net_.sendMessage();
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        }
    }

    void receiveLoop() {
        while (running_) {
            try {
                net_.receiveMessage();
            } catch (const std::exception& e) {
                std::cerr << "Receive error: " << e.what() << "\n";
                running_ = false;
                return;
            }
            client_.processIncoming();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    Client client_;
    ClientNetwork net_;
    std::atomic<bool> running_;
    std::thread hbThread_;
    std::thread recThread_;
};

PYBIND11_MODULE(miniexchange_client, m) {
    py::class_<PyClient>(m, "MiniExchangeClient")
        .def(py::init<std::array<uint8_t, constants::HMAC_SIZE>, std::array<uint8_t, 16>,
                      const std::string&, uint16_t>(),
             py::arg("hmac_key"), py::arg("api_key"), py::arg("server_ip") = "127.0.0.1",
             py::arg("port") = 12345)
        .def("connect", &PyClient::connect)
        .def("disconnect", &PyClient::disconnect)
        .def("send_hello", &PyClient::send_hello)
        .def("send_logout", &PyClient::send_logout)
        .def("send_order", &PyClient::send_order)
        .def("send_cancel", &PyClient::send_cancel)
        .def("send_modify", &PyClient::send_modify)
        .def("read_recv_buffer", &PyClient::read_recv_buffer)
        .def("read_send_buffer", &PyClient::read_send_buffer)
        .def("is_running", &PyClient::is_running);
}
