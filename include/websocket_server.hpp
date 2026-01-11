#ifndef WEBSOCKET_SERVER_HPP
#define WEBSOCKET_SERVER_HPP

#include "metrics_collector.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <chrono>
#include <cctype>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace hft_json {

inline std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);

    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    // Control chars -> drop (or could be \u00XX). Keep it simple.
                    out += ' ';
                } else {
                    out += c;
                }
        }
    }

    return out;
}

inline std::string quote(const std::string& s) {
    return std::string("\"") + escape(s) + "\"";
}

inline void append_comma_if_needed(std::ostringstream& oss, bool& first) {
    if (!first) {
        oss << ',';
    }
    first = false;
}

template <typename T>
inline void append_kv_number(std::ostringstream& oss, bool& first, const char* key, T value) {
    append_comma_if_needed(oss, first);
    oss << quote(key) << ':' << value;
}

inline void append_kv_string(std::ostringstream& oss, bool& first, const char* key, const std::string& value) {
    append_comma_if_needed(oss, first);
    oss << quote(key) << ':' << quote(value);
}

inline std::string extract_string_field(const std::string& msg, const char* field_name) {
    // Extremely small JSON helper for messages like: {"command":"get_history"}
    // Not a general JSON parser.
    const std::string needle = std::string("\"") + field_name + "\"";
    std::size_t pos = msg.find(needle);
    if (pos == std::string::npos) return {};

    pos = msg.find(':', pos + needle.size());
    if (pos == std::string::npos) return {};

    // Skip whitespace
    ++pos;
    while (pos < msg.size() && std::isspace(static_cast<unsigned char>(msg[pos]))) ++pos;

    if (pos >= msg.size() || msg[pos] != '"') return {};
    ++pos;

    std::size_t end = msg.find('"', pos);
    if (end == std::string::npos || end <= pos) return {};

    return msg.substr(pos, end - pos);
}

} // namespace hft_json

// WebSocket session for each connected client
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket, MetricsCollector& collector)
        : ws_(std::move(socket)), collector_(collector) {}
    
    void run() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                self->read_message();
            }
        });
    }
    
    void send_metrics(const std::string& json_data) {
        auto msg = std::make_shared<std::string>(json_data);
        ws_.async_write(
            net::buffer(*msg),
            [self = shared_from_this(), msg](beast::error_code ec, std::size_t) {
                // Message sent
            }
        );
    }
    
private:
    void read_message() {
        ws_.async_read(
            buffer_,
            [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
                if (!ec) {
                    self->handle_message();
                    self->read_message();
                }
            }
        );
    }
    
    void handle_message() {
        std::string msg = beast::buffers_to_string(buffer_.data());
        buffer_.consume(buffer_.size());
        
        // Handle client requests (e.g., "get_history", "get_summary")
        // Avoid external JSON dependencies: accept very small JSON messages.
        const std::string cmd = hft_json::extract_string_field(msg, "command");
        if (cmd == "get_history") {
            send_history();
        } else if (cmd == "get_summary") {
            send_summary();
        }
    }
    
    void send_history() {
        auto snapshots = collector_.get_recent_snapshots(1000);

        std::ostringstream oss;
        oss << '[';
        bool first_item = true;

        for (const auto& snap : snapshots) {
            if (!first_item) oss << ',';
            first_item = false;

            oss << '{';
            bool first_kv = true;
            hft_json::append_kv_number(oss, first_kv, "timestamp", snap.timestamp_ns);
            hft_json::append_kv_number(oss, first_kv, "mid_price", snap.mid_price);
            hft_json::append_kv_number(oss, first_kv, "spread", snap.spread_bps);
            hft_json::append_kv_number(oss, first_kv, "pnl", snap.pnl);
            hft_json::append_kv_number(oss, first_kv, "position", snap.position);
            hft_json::append_kv_number(oss, first_kv, "buy_intensity", snap.buy_intensity);
            hft_json::append_kv_number(oss, first_kv, "sell_intensity", snap.sell_intensity);
            hft_json::append_kv_number(oss, first_kv, "latency", snap.cycle_latency_us);
            oss << '}';
        }

        oss << ']';
        send_metrics(oss.str());
    }
    
    void send_summary() {
        auto stats = collector_.get_summary();

        std::ostringstream oss;
        oss << '{';
        bool first_kv = true;
        hft_json::append_kv_string(oss, first_kv, "type", "summary");
        hft_json::append_kv_number(oss, first_kv, "avg_pnl", stats.avg_pnl);
        hft_json::append_kv_number(oss, first_kv, "max_pnl", stats.max_pnl);
        hft_json::append_kv_number(oss, first_kv, "min_pnl", stats.min_pnl);
        hft_json::append_kv_number(oss, first_kv, "avg_latency", stats.avg_latency_us);
        hft_json::append_kv_number(oss, first_kv, "max_latency", stats.max_latency_us);
        hft_json::append_kv_number(oss, first_kv, "total_trades", stats.total_trades);
        hft_json::append_kv_number(oss, first_kv, "fill_rate", stats.fill_rate);
        oss << '}';

        send_metrics(oss.str());
    }
    
    websocket::stream<tcp::socket> ws_;
    MetricsCollector& collector_;
    beast::flat_buffer buffer_;
};

// WebSocket server for dashboard
class DashboardServer {
public:
    DashboardServer(MetricsCollector& collector, int port = 8080)
        : collector_(collector),
          ioc_(),
          acceptor_(ioc_, tcp::endpoint(tcp::v4(), port)),
          running_(false) {}
    
    void start() {
        running_.store(true, std::memory_order_release);
        
        // Start accepting connections
        server_thread_ = std::thread([this]() {
            accept_connection();
            ioc_.run();
        });
        
        // Start metrics broadcast thread
        broadcast_thread_ = std::thread([this]() {
            broadcast_metrics();
        });
        
        std::cout << "Dashboard server started on port " 
                  << acceptor_.local_endpoint().port() << std::endl;
        std::cout << "Open http://localhost:" << acceptor_.local_endpoint().port() 
                  << " in your browser" << std::endl;
    }
    
    void stop() {
        running_.store(false, std::memory_order_release);
        ioc_.stop();
        
        if (server_thread_.joinable()) server_thread_.join();
        if (broadcast_thread_.joinable()) broadcast_thread_.join();
    }
    
private:
    void accept_connection() {
        acceptor_.async_accept([this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<WebSocketSession>(
                    std::move(socket), collector_
                );
                
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_.insert(session);
                session->run();
            }
            
            if (running_.load(std::memory_order_acquire)) {
                accept_connection();
            }
        });
    }
    
    void broadcast_metrics() {
        while (running_.load(std::memory_order_acquire)) {
            // Broadcast every 100ms
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Get current metrics
            auto& metrics = collector_.get_metrics();

            std::ostringstream oss;
            oss << '{';
            bool first_kv = true;
            hft_json::append_kv_string(oss, first_kv, "type", "update");
            hft_json::append_kv_number(oss, first_kv, "timestamp", std::chrono::steady_clock::now().time_since_epoch().count());
            hft_json::append_kv_number(oss, first_kv, "mid_price", metrics.mid_price.load());
            hft_json::append_kv_number(oss, first_kv, "spread", metrics.spread_bps.load());
            hft_json::append_kv_number(oss, first_kv, "pnl", metrics.total_pnl.load());
            hft_json::append_kv_number(oss, first_kv, "position", metrics.current_position.load());
            hft_json::append_kv_number(oss, first_kv, "buy_intensity", metrics.buy_intensity.load());
            hft_json::append_kv_number(oss, first_kv, "sell_intensity", metrics.sell_intensity.load());
            hft_json::append_kv_number(oss, first_kv, "latency", metrics.avg_cycle_latency_us.load());
            hft_json::append_kv_number(oss, first_kv, "orders_sent", metrics.orders_sent.load());
            hft_json::append_kv_number(oss, first_kv, "orders_filled", metrics.orders_filled.load());
            hft_json::append_kv_number(oss, first_kv, "regime", metrics.current_regime.load());
            hft_json::append_kv_number(oss, first_kv, "position_usage", metrics.position_limit_usage.load());
            oss << '}';

            std::string msg = oss.str();
            
            // Send to all connected clients
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            for (auto& session : sessions_) {
                session->send_metrics(msg);
            }
        }
    }
    
    MetricsCollector& collector_;
    net::io_context ioc_;
    tcp::acceptor acceptor_;
    std::atomic<bool> running_;
    
    std::set<std::shared_ptr<WebSocketSession>> sessions_;
    std::mutex sessions_mutex_;
    
    std::thread server_thread_;
    std::thread broadcast_thread_;
};

#endif // WEBSOCKET_SERVER_HPP
