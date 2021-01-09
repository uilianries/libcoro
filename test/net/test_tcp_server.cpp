#include "catch.hpp"

#include <coro/coro.hpp>

TEST_CASE("tcp_server no on connection throws")
{
    REQUIRE_THROWS(coro::net::tcp_server{coro::net::tcp_server::options{.on_connection = nullptr}});
}

static auto tcp_server_echo(
    const std::variant<coro::net::hostname, coro::net::ip_address> address,
    const std::string msg
) -> void
{
    auto on_connection = [&msg](coro::net::tcp_server& scheduler, coro::net::socket sock) -> coro::task<void> {
        std::string in(64, '\0');

        auto [rstatus, rbytes] = co_await scheduler.read(sock, std::span<char>{in.data(), in.size()});
        REQUIRE(rstatus == coro::poll_status::event);

        in.resize(rbytes);
        REQUIRE(in == msg);

        auto [wstatus, wbytes] = co_await scheduler.write(sock, std::span<const char>(in.data(), in.length()));
        REQUIRE(wstatus == coro::poll_status::event);
        REQUIRE(wbytes == in.length());

        co_return;
    };

    coro::net::tcp_server scheduler{coro::net::tcp_server::options{
        .address       = coro::net::ip_address::from_string("0.0.0.0"),
        .port          = 8080,
        .backlog       = 128,
        .on_connection = on_connection,
        .io_options    = coro::io_scheduler::options{.thread_strategy = coro::io_scheduler::thread_strategy_t::spawn}}};

    coro::net::dns_client dns_client{scheduler, std::chrono::seconds{5}};

    auto make_client_task = [&scheduler, &dns_client, &address, &msg]() -> coro::task<void> {
        coro::net::tcp_client client{
            scheduler,
            coro::net::tcp_client::options{
                .address = address,
                .port = 8080,
                .domain = coro::net::domain_t::ipv4,
                .dns = &dns_client}};

        auto cstatus = co_await client.connect();
        REQUIRE(cstatus == coro::net::connect_status::connected);

        auto [wstatus, wbytes] = co_await client.send(std::span<const char>{msg.data(), msg.length()});

        REQUIRE(wstatus == coro::poll_status::event);
        REQUIRE(wbytes == msg.length());

        std::string response(64, '\0');

        auto [rstatus, rbytes] = co_await client.recv(std::span<char>{response.data(), response.length()});

        REQUIRE(rstatus == coro::poll_status::event);
        REQUIRE(rbytes == msg.length());
        response.resize(rbytes);
        REQUIRE(response == msg);

        co_return;
    };

    REQUIRE(scheduler.schedule(make_client_task()));

    // Shutting down the scheduler will cause it to stop accepting new connections, to avoid requiring
    // another scheduler for this test the main thread can spin sleep until the tcp scheduler reports
    // that it is empty.  tcp schedulers do not report their accept task as a task in its size/empty count.
    while (!scheduler.empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }

    scheduler.shutdown();
    REQUIRE(scheduler.empty());
}

TEST_CASE("tcp_server echo server")
{
    const std::string msg{"Hello from client"};

    tcp_server_echo(coro::net::ip_address::from_string("127.0.0.1"), msg);
    tcp_server_echo(coro::net::hostname{"localhost"}, msg);
}
