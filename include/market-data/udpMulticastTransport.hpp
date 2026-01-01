#include <arpa/inet.h>
#include <cstdint>
#include <netinet/in.h>
#include <span>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

namespace market_data {

struct UDPConfig {
    std::string multicastGroup = "239.0.0.1";
    std::uint16_t port = 9001;
    std::string interfaceIP = "0.0.0.0";
    int ttl = 1;
};

class UDPMulticastTransport {
public:
    explicit UDPMulticastTransport(const UDPConfig& config = UDPConfig{})
        : config_(config), sockfd_(-1) {
        initialize();
    }

    ~UDPMulticastTransport() {
        if (sockfd_ >= 0) {
            close(sockfd_);
        }
    }

    UDPMulticastTransport(const UDPMulticastTransport&) = delete;
    UDPMulticastTransport& operator=(const UDPMulticastTransport&) = delete;
    UDPMulticastTransport(UDPMulticastTransport&& other) noexcept = delete;

    void send(std::span<const std::byte> data);

private:
    void initialize();
    UDPConfig config_;
    int sockfd_;
    sockaddr_in addr_;
};

} // namespace market_data
