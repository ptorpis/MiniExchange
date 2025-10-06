#include "client/client.hpp"
#include "client/clientNetwork.hpp"
#include <atomic>
#include <list>
#include <mutex>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <thread>

namespace py = pybind11;
using namespace pybind11::literals;

class PyClient {
public:
    PyClient(std::array<uint8_t, constants::HMAC_SIZE> hmacKey,
             std::array<uint8_t, 16> apiKey, const std::string& serverIP = "127.0.0.1",
             uint16_t port = 12345)
        : client_(hmacKey, apiKey), net_(serverIP, port, client_), running_(false) {}
    ~PyClient() { stop(); }

    bool connect() { return net_.connectServer(); }

    void start() {
        if (running_) return;
        running_ = true;
        recvThread_ = std::thread(&PyClient::receiveLoop_, this);
        hbThread_ = std::thread(&PyClient::heartbeatLoop_, this);
    }

    void stop() {
        running_ = false;
        if (recvThread_.joinable()) recvThread_.join();
        if (hbThread_.joinable()) hbThread_.join();
        net_.disconnectServer();
    }

    void sendHello() {
        client_.sendHello();
        net_.sendMessage();
    }
    void sendLogout() {
        client_.sendLogout();
        net_.sendMessage();
    }

    void sendOrder(Qty qty, Price price, bool isBuy, bool isLimit) {
        client_.sendOrder(qty, price, isBuy, isLimit);
        net_.sendMessage();
    }

    void sendCancel(uint64_t orderID) {
        client_.sendCancel(orderID);
        net_.sendMessage();
    }
    void sendModify(uint64_t orderID, uint64_t qty, Price price) {
        client_.sendModify(orderID, qty, price);
        net_.sendMessage();
    }

    void receiveLoop_() {
        while (running_) {
            try {
                if (net_.waitReadable(50)) {
                    if (net_.receiveMessage()) {
                        auto msgs = client_.processIncoming();
                        for (auto& msg : msgs) {
                            py::gil_scoped_acquire gil;
                            py::dict pymsg = convertMessage_(msg);
                            py::function cbCopy;

                            {
                                std::lock_guard<std::mutex> lock(cbMutex_);
                                cbCopy = messageCallback_; // copy while holding lock
                            }

                            if (cbCopy && !cbCopy.is_none()) {
                                try {
                                    cbCopy(pymsg);
                                } catch (py::error_already_set& e) {
                                    std::cerr << "Python callback error: " << e.what()
                                              << "\n";
                                }
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Receive error: " << e.what() << "\n";
                running_ = false;
            }
        }
    }

    void heartbeatLoop_() {
        while (running_) {
            client_.sendHeartbeat();
            net_.sendMessage();
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        }
    }

    py::dict convertMessage_(const client::IncomingMessageVariant& msg) {
        return std::visit(
            [](auto&& payload) -> py::dict {
                using T = std::decay_t<decltype(payload)>;

                if constexpr (std::is_same_v<T, server::HelloAckPayload>) {
                    return py::dict("type"_a = "HELLO_ACK",
                                    "clientId"_a = payload.serverClientID,
                                    "status"_a = payload.status);
                } else if constexpr (std::is_same_v<T, server::OrderAckPayload>) {
                    return py::dict("type"_a = "ORDER_ACK",
                                    "orderId"_a = payload.serverOrderID,
                                    "status"_a = payload.status);
                } else if constexpr (std::is_same_v<T, server::TradePayload>) {
                    return py::dict("type"_a = "TRADE", "price"_a = payload.filledPrice,
                                    "quantity"_a = payload.filledQty);
                } else {
                    return py::dict("type"_a = "UNKNOWN");
                }
            },
            msg);
    }

    Client client_;
    ClientNetwork net_;
    std::atomic<bool> running_{false};
    std::thread recvThread_;
    std::thread hbThread_;
    std::mutex msgMutex_;
    std::vector<py::dict> messages_;

    std::chrono::seconds intervalSeconds{2};

    std::mutex cbMutex_;
    py::function messageCallback_;
};

PYBIND11_MODULE(miniexchange_client, m) {
    py::class_<PyClient>(m, "MiniExchangeClient")
        .def(py::init<std::array<uint8_t, constants::HMAC_SIZE>, std::array<uint8_t, 16>,
                      const std::string&, uint16_t>(),
             py::arg("hmac_key"), py::arg("api_key"), py::arg("server_ip") = "127.0.0.1",
             py::arg("port") = 12345)
        .def("connect", &PyClient::connect)
        .def("start", &PyClient::start)
        .def("stop", &PyClient::stop)
        .def("send_hello", &PyClient::sendHello)
        .def("send_order", &PyClient::sendOrder)
        .def("on_message", [](PyClient& self, py::function cb) {
            std::lock_guard<std::mutex> lock(self.cbMutex_);
            self.messageCallback_ = std::move(cb);
        });
}
