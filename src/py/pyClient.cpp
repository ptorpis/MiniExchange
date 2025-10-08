#include "client/client.hpp"
#include "client/clientNetwork.hpp"
#include <atomic>
#include <condition_variable>
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

    void stop() noexcept {
        running_ = false;

        messagesCv_.notify_all();

        try {
            if (recvThread_.joinable()) recvThread_.join();
        } catch (const std::exception& e) {
            std::cerr << "Exception joining recvThread_: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Unknown exception joining recvThread_\n";
        }

        try {
            if (hbThread_.joinable()) hbThread_.join();
        } catch (const std::exception& e) {
            std::cerr << "Exception joining hbThread_: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Unknown exception joining hbThread_\n";
        }

        try {
            net_.disconnectServer();
        } catch (const std::exception& e) {
            std::cerr << "Exception disconnecting server: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Unknown exception disconnecting server\n";
        }
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
    void sendModify(uint64_t orderID, Qty newQty, Price newPrice) {
        client_.sendModify(orderID, newQty, newPrice);
        net_.sendMessage();
    }

    void receiveLoop_() {
        while (running_) {
            try {
                if (net_.waitReadable(50) && net_.receiveMessage() && running_) {
                    auto msgs = client_.processIncoming();

                    for (auto& msg : msgs) {
                        py::gil_scoped_acquire gil;
                        py::dict pymsg = convertMessage_(msg);

                        py::function cbCopy;
                        {
                            std::lock_guard<std::mutex> lock(cbMutex_);
                            cbCopy = messageCallback_;
                        }

                        {
                            std::lock_guard<std::mutex> lock(msgMutex_);
                            messages_.push_back(pymsg);
                            messagesCv_.notify_one();
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

    py::list wait_for_messages(int timeout_ms = 1000) {
        std::unique_lock<std::mutex> lock(msgMutex_);
        if (messages_.empty()) {
            messagesCv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                 [this] { return !messages_.empty(); });
        }

        py::list out;
        for (auto& msg : messages_) out.append(msg);
        messages_.clear();
        return out;
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
                                    "server_client_id"_a = payload.serverClientID,
                                    "instrument_id"_a = payload.instrumentID,
                                    "server_order_id"_a = payload.serverOrderID,
                                    "status"_a = payload.status,
                                    "accepted_price"_a = payload.acceptedPrice,
                                    "server_time"_a = payload.serverTime,
                                    "latency"_a = payload.latency);
                } else if constexpr (std::is_same_v<T, server::CancelAckPayload>) {
                    return py::dict("type"_a = "CANCEL_ACK",
                                    "server_client_id"_a = payload.serverClientID,
                                    "server_order_id"_a = payload.serverOrderID,
                                    "status"_a = payload.status);
                } else if constexpr (std::is_same_v<T, server::ModifyAckPayload>) {
                    return py::dict("type"_a = "MODIFY_ACK",
                                    "server_client_id"_a = payload.serverClientID,
                                    "old_server_order_id"_a = payload.oldServerOrderID,
                                    "new_server_order_id"_a = payload.newServerOrderID,
                                    "status"_a = payload.status);
                } else if constexpr (std::is_same_v<T, server::TradePayload>) {
                    return py::dict(
                        "type"_a = "TRADE", "server_client_id"_a = payload.serverClientID,
                        "server_order_id"_a = payload.serverOrderID,
                        "trade_id"_a = payload.tradeID, "price"_a = payload.filledPrice,
                        "quantity"_a = payload.filledQty,
                        "server_time"_a = payload.timestamp);
                } else if constexpr (std::is_same_v<T, server::LogoutAckPayload>) {
                    return py::dict("type"_a = "LOGOUT_ACK", "status"_a = payload.status);
                } else if constexpr (std::is_same_v<T, server::SessionTimeoutPayload>) {
                    return py::dict("type"_a = "SESSION_TIMEOUT",
                                    "server_time"_a = payload.serverTime);
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
    std::condition_variable messagesCv_;
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
        .def("send_cancel", &PyClient::sendCancel)
        .def("send_modify", &PyClient::sendModify)
        .def("send_logout", &PyClient::sendLogout)
        .def("on_message",
             [](PyClient& self, py::function cb) {
                 std::lock_guard<std::mutex> lock(self.cbMutex_);
                 self.messageCallback_ = std::move(cb);
             })
        .def("wait_for_messages", &PyClient::wait_for_messages,
             py::arg("timeout_ms") = 1000)
        .def("__enter__",
             [](PyClient& self) {
                 self.connect();
                 self.start();
                 return &self;
             })
        .def("__exit__",
             [](PyClient& self, py::object, py::object, py::object) { self.stop(); });
}
