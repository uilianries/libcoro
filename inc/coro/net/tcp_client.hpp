#pragma once

#include "coro/net/dns_client.hpp"
#include "coro/net/ip_address.hpp"
#include "coro/net/hostname.hpp"
#include "coro/net/socket.hpp"
#include "coro/net/connect.hpp"
#include "coro/poll.hpp"
#include "coro/task.hpp"

#include <chrono>
#include <optional>
#include <variant>
#include <memory>
#include <chrono>

namespace coro
{
class io_scheduler;
} // namespace coro

namespace coro::net
{

class tcp_client
{
public:
    struct options
    {
        /// The hostname or ip address to connect to.  If using hostname then a dns client must be provided.
        std::variant<net::hostname, net::ip_address> address{net::ip_address::from_string("127.0.0.1")};
        /// The port to connect to.
        int16_t       port{8080};
        /// The protocol domain to connect with.
        net::domain_t domain{net::domain_t::ipv4};
        /// If using a hostname to connect to then provide a dns client to lookup the host's ip address.
        /// This is optional if using ip addresses directly.
        net::dns_client*   dns{nullptr};
    };

    tcp_client(io_scheduler& scheduler, options opts = options{
        .address = {net::ip_address::from_string("127.0.0.1")},
        .port = 8080,
        .domain = net::domain_t::ipv4,
        .dns = nullptr});
    tcp_client(const tcp_client&) = delete;
    tcp_client(tcp_client&&)      = default;
    auto operator=(const tcp_client&) noexcept -> tcp_client& = delete;
    auto operator=(tcp_client&&) noexcept -> tcp_client& = default;
    ~tcp_client()                                        = default;

    auto connect(std::chrono::milliseconds timeout = std::chrono::milliseconds{0}) -> coro::task<net::connect_status>;

    auto recv(
        std::span<char> buffer,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0}) -> coro::task<std::pair<poll_status, ssize_t>>;
    auto send(
        const std::span<const char> buffer,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{0}) -> coro::task<std::pair<poll_status, ssize_t>>;

private:
    /// The scheduler that will drive this tcp client.
    io_scheduler& m_io_scheduler;
    /// Options for what server to connect to.
    options m_options;
    /// The tcp socket.
    net::socket m_socket;
    /// Cache the status of the connect in the event the user calls connect() again.
    std::optional<net::connect_status> m_connect_status{std::nullopt};
};

} // namespace coro::net
