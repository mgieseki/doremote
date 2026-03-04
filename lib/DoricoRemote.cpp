// Copyright (C) 2026 Martin Gieseking
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <map>
#include <future>
#include <thread>

#include "DoricoRemote.hpp"
#include "Request.hpp"
#include "WSMessage.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace websocket = beast::websocket;

using boost::asio::ip::tcp;
using net::awaitable;
using net::use_awaitable;
using std::string;

using SocketStream = websocket::stream<tcp::socket>;

// Class providing the actual implementation for DoricoRemote.
// To keep the Boost stuff out of the public interface, we declare it here.
class DoricoRemoteImpl {
    public:
        enum class SendMode {NORMAL, FORCE};

        DoricoRemoteImpl ();
        ~DoricoRemoteImpl ();
        bool connect (const string &clientName, const string &host, const string &port);
        bool connect (const string &clientName, const string &host, const string &port, const string &token);
        void disconnect ();
        const string& sessionToken () const {return sessionToken_;}

        template <class Request>
        Request::Response send (Request &request, SendMode mode=SendMode::NORMAL);

        void send (const Request &request, Response &response, SendMode mode=SendMode::NORMAL);
        WSMessage getMessage (const string &type) const;
        string getMessageValue (const string &type, const string &key) const;
        void setStatusCallback (DoricoRemote::StatusCallback cb) {statusCallback_ = std::move(cb);}
        void setTerminationCallback (DoricoRemote::TerminationCallback cb) {terminationCallback_ = std::move(cb);}
        void setLogLevel (DoricoRemote::LogLevel level) const;

    protected:
        template<typename T>
        T runAwaitableSync(awaitable<T> aw);

        // keep the asynchronous functionality local for now
        awaitable<bool> asyncConnect (const string &clientName, const string &host, const string &port);
        awaitable<bool> asyncConnect (const string &clientName, const string &host, const string &port, const string &token);
        awaitable<void> asyncDisconnect ();
        awaitable<void> asyncSend (const Request &request, Response &response, SendMode mode=SendMode::NORMAL);

        template <class Request>
        awaitable<typename Request::Response> asyncSend (const Request &request, SendMode mode=SendMode::NORMAL);

        awaitable<void> readingLoop ();

    private:
        using WorkGuard = net::executor_work_guard<net::io_context::executor_type>;
        using ReceivedMsgMap = std::map<string,json>;
        using ReceivedMsgMapIt = ReceivedMsgMap::iterator;

        net::io_context ioc_;                 //< provides the basic network I/O functionality
        WorkGuard workGuard_;                 //< keeps the I/O thread running until the connection is closed
        std::jthread ioThread_;               //< the websocket I/O runs in this thread
        SocketStream ws_;                     //< the messages from and to Dorico are sent over this stream
        net::steady_timer responseTimer_;     //< timer used to wait for responses
        string sessionToken_;                 //< token of the current session
        bool connected_ = false;              //< true if connection to Dorico is established
        bool running_ = false;                //< true if the message thread is running
        ReceivedMsgMap receivedMsgs_;         //< last received messages from Dorico separated by message type
        ReceivedMsgMapIt lastReceivedMsgIt_;
        std::shared_ptr<spdlog::logger> logger_;
        DoricoRemote::StatusCallback statusCallback_ = nullptr;
        DoricoRemote::TerminationCallback terminationCallback_ = nullptr;
};

/////////////////////////////////////////////////////////////////////////////////
// Public interface for synchronous communication

DoricoRemote::DoricoRemote ()
    : impl_(std::make_unique<DoricoRemoteImpl>())
{
}

DoricoRemote::~DoricoRemote () =default;

/** Try to establish a connection to Dorico by requesting a new session. This requires
 *  the user to confirm a connection dialog shown in Dorico.
 *  @param[in] clientName name of client trying to connect to Dorico (appears in Dorico's client list)
 *  @param[in] host name or IP address (usually 'localhost' or 127.0.0.1, respectively)
 *  @param[in] port port number given in the Dorico settings (usually 4560)
 *  @return true if the connection has been established */
bool DoricoRemote::connect (const string &clientName, const string &host, const string &port) const {
    return impl_->connect(clientName, host, port);
}

/** Try to establish a connection to Dorico by using a previous session. If the given
 *  session token is valid, the connection is established without further user interaction.
 *  @param[in] clientName name of client trying to connect to Dorico (appears in Dorico's client list)
 *  @param[in] host name or IP address (usually 'localhost' or 127.0.0.1, respectively)
 *  @param[in] port port number given in the Dorico settings (usually 4560)
 *  @param[in] token session token of a previous connection
 *  @return true if the connection has been established */
bool DoricoRemote::connect (const string &clientName, const string &host, const string &port, const string &token) const {
    return impl_->connect(clientName, host, port, token);
}

/** Terminate the connection to Dorico. */
void DoricoRemote::disconnect () const {
    impl_->disconnect();
}

/** Returns the token of the current session or an empty string if there's no session active. */
const string& DoricoRemote::sessionToken () const {
    return impl_->sessionToken();
}

/** Send a message request to Dorico.
 *  @param[in] request the request to be sent
 *  @param[in,out] response the response returned by Dorico */
void DoricoRemote::send (const Request &request, Response &response) const {
    impl_->send(request, response);
}

WSMessage DoricoRemote::getMessage (const string &type) const {
    return impl_->getMessage(type);
}

string DoricoRemote::getMessageValue (const string &type, const string &key) const {
    return impl_->getMessageValue(type, key);
}

void DoricoRemote::setStatusCallback (StatusCallback callback) const {
    impl_->setStatusCallback(std::move(callback));
}

void DoricoRemote::setTerminationCallback (TerminationCallback callback) const {
    impl_->setTerminationCallback(std::move(callback));
}

void DoricoRemote::setLogLevel (LogLevel level) const {
    impl_->setLogLevel(level);
}

/////////////////////////////////////////////////////////////////////////////////
// Actual implementation of the provided functionality.

DoricoRemoteImpl::DoricoRemoteImpl ()
    : workGuard_(ioc_.get_executor())
    , ws_(net::make_strand(ioc_))
    , responseTimer_(ioc_)
    , lastReceivedMsgIt_(receivedMsgs_.end())
    , logger_(spdlog::stderr_color_mt("DoricoRemote"))
{
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S][%^%l%$] %v");
    logger_->set_level(spdlog::level::off);
#ifdef _WIN32
    if (auto sink = dynamic_cast<spdlog::sinks::stderr_color_sink_mt*>(logger_->sinks().back().get()))
        sink->set_color(spdlog::level::trace, FOREGROUND_BLUE|FOREGROUND_GREEN);
#endif
    ioThread_ = std::jthread([this] {
        ioc_.run();
    });
}

DoricoRemoteImpl::~DoricoRemoteImpl () {
    if (running_)
        disconnect();    // send 'disconnect' request to Dorico
    workGuard_.reset();  // no need to keep the I/O context and the I/O thread running any longer
    ioc_.stop();         // stop I/O context which lets the I/O thread finish
}

void DoricoRemoteImpl::setLogLevel (DoricoRemote::LogLevel level) const {
    auto spdlogLevel = spdlog::level::off;
    switch (level) {
        case DoricoRemote::LL_ERROR: spdlogLevel = spdlog::level::err; break;
        case DoricoRemote::LL_WARN:  spdlogLevel = spdlog::level::warn; break;
        case DoricoRemote::LL_INFO:  spdlogLevel = spdlog::level::info; break;
        case DoricoRemote::LL_TRACE: spdlogLevel = spdlog::level::trace; break;
        default: ;
    }
    logger_->set_level(spdlogLevel);
}

/** Try to establish a websocket connection.
 *  @param[in] ws websocket stream used for the communication
 *  @param[in] host name or IP address to connect to
 *  @param[in] port port number as string */
static awaitable<void> establish_connection (SocketStream &ws, string host, string port) {
    auto ex = co_await net::this_coro::executor;
    tcp::resolver resolver(ex);
    auto results = co_await resolver.async_resolve(host, port, use_awaitable);
    auto endpoint = co_await net::async_connect(ws.next_layer(), results, use_awaitable);
    host = endpoint.address().to_string();
    port = std::to_string(endpoint.port());
    co_await ws.async_handshake(host + ":" + port, "/", use_awaitable);
    co_return;
}

/** Try to establish a connection to Dorico by requesting a new session. This requires
 *  the user to confirm a connection dialog shown in Dorico.
 *  @param[in] clientName name of client trying to connect to Dorico (appears in Dorico's client list)
 *  @param[in] host name or IP address (usually 'localhost' or 127.0.0.1, respectively)
 *  @param[in] port port number given in the Dorico settings (usually 4560)
 *  @return true if the connection has been established */
awaitable<bool> DoricoRemoteImpl::asyncConnect (const string &clientName, const string &host, const string &port) {
    logger_->info("connecting '" + clientName + "' to " + host + ":" + port);
    if (connected_) {
        logger_->info("already connected");
        co_return false;
    }
    try {
        co_await establish_connection(ws_, host, port);
    }
    catch (const std::exception &e) {
        logger_->error(string("connection failed: ") + e.what());
        throw ConnectionException();
    }
    net::co_spawn(
        ws_.get_executor(),
        [this]() -> awaitable<void> {co_await readingLoop();},
        net::detached);
    SessionTokenResponse tokenResponse = co_await asyncSend(ConnectRequest(clientName), SendMode::FORCE);
    sessionToken_ = tokenResponse.token();
    if (!sessionToken_.empty()) {
        CodeResponse response = co_await asyncSend(AcceptSessionTokenRequest(sessionToken_), SendMode::FORCE);
        connected_ = (response.code() == "kConnected");
    }
    if (connected_)
        logger_->info("connected");
    else {
        logger_->error("connection failed");
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
    }
    co_return connected_;
}

/** Try to establish a connection to Dorico by using a previous session. If the given
 *  session token is valid, the connection is established without further user interaction.
 *  @param[in] clientName name of client trying to connect to Dorico (appears in Dorico's client list)
 *  @param[in] host name or IP address (usually 'localhost' or 127.0.0.1, respectively)
 *  @param[in] port port number given in the Dorico settings (usually 4560)
 *  @param[in] token session token of a previous connection
 *  @return true if the connection has been established */
awaitable<bool> DoricoRemoteImpl::asyncConnect (const string &clientName, const string &host, const string &port, const string &token) {
    logger_->info("connecting '" + clientName + "' to " + host + ":" + port + " with token " + token);
    if (connected_) {
        logger_->info("already connected");
        co_return false;
    }
    try {
        co_await establish_connection(ws_, host, port);
    }
    catch (const std::exception &e) {
        logger_->error(string("connection failed: ") + e.what());
        throw ConnectionException();
    }
    net::co_spawn(
         ws_.get_executor(),
         [this]() -> awaitable<void> {co_await readingLoop();},
         net::detached);
    CodeResponse response = co_await asyncSend(ReconnectRequest(clientName, token), SendMode::FORCE);
    connected_ = (response.code() == "kConnected");
    if (connected_) {
        sessionToken_ = token;
        logger_->info("connected");
    }
    else {
        logger_->error("connection failed");
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
    }
    co_return connected_;
}

/** Terminate the connection to Dorico. */
awaitable<void> DoricoRemoteImpl::asyncDisconnect () {
    if (connected_) {
        try {
            DisconnectRequest disconnectRequest;
            co_await asyncSend(disconnectRequest, SendMode::FORCE);
        }
        catch (...) {
        }
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
        connected_ = false;
        sessionToken_.clear();
        receivedMsgs_.clear();
        lastReceivedMsgIt_ = receivedMsgs_.end();
        logger_->info("disconnected");
    }
    co_return;
}

/** Send a message request to Dorico.
 *  @param[in] request the request to be sent
 *  @param[in,out] response the response received from Dorico
 *  @param[in] mode allows to send the request even if Dorico hasn't confirmed the connection yet (mode=FORCE) */
awaitable<void> DoricoRemoteImpl::asyncSend (const Request &request, Response &response, SendMode mode) {
    if (!running_ || (!connected_ && mode == SendMode::NORMAL))
        co_return;

    logger_->info("sending " + request.info());
    logger_->trace(string("[\033[035mreq\033[0m] ") + request.toString());
    lastReceivedMsgIt_ = receivedMsgs_.end();
    co_await ws_.async_write(net::buffer(request.toString()), use_awaitable);
    if (response.type().empty() || !running_)
        co_return;

    // wait until reading_loop() receives a response and cancels the timer
    responseTimer_.expires_at(net::steady_timer::time_point::max());
    boost::system::error_code ec;
    co_await responseTimer_.async_wait(net::redirect_error(use_awaitable, ec));
    if (ec && ec != net::error::operation_aborted)
        throw boost::system::system_error(ec);

    if (lastReceivedMsgIt_ != receivedMsgs_.end()) {
        response.assign(lastReceivedMsgIt_->second);
        logger_->trace(string("[\033[036mrsp\033[0m] ") + response.json().dump());
    }
    co_return;
}

/** Send a message request to Dorico.
 *  @param[in] request the request to be sent
 *  @param[in] mode allows to send the request even if Dorico hasn't confirmed the connection yet (mode=FORCE)
 *  @return the response received from Dorico */
template<class Request>
awaitable<typename Request::Response> DoricoRemoteImpl::asyncSend (const Request &request, SendMode mode) {
    typename Request::Response response;
    co_await asyncSend(request, response, mode);
    co_return response;
}

/** Collect the responses retrieved from Dorico.
 *  This method runs in a background coroutine. */
awaitable<void> DoricoRemoteImpl::readingLoop () {
    running_ = true;
    logger_->trace("start collecting responses");

    beast::flat_buffer buffer;
    boost::system::error_code ec;
    while (!ec.failed()) {
        buffer.consume(buffer.size());
        co_await ws_.async_read(buffer, net::redirect_error(use_awaitable, ec));
        if (ec.failed())
            break;
        json response = json::parse(beast::buffers_to_string(buffer.data()));
        logger_->trace(string("[\033[034mrcv\033[0m] ") + response.dump());
        auto msg_type = response["message"];
        if (msg_type == "status") {
            response.erase("message");
            receivedMsgs_["status"] = response;
            if (statusCallback_)
                statusCallback_(receivedMsgs_["status"]);
        }
        else {
            receivedMsgs_[msg_type] = std::move(response);
            lastReceivedMsgIt_ = receivedMsgs_.find(msg_type);
            responseTimer_.cancel();  // wake-up waiting coroutine async_send()
        }
    }
    running_ = false;
    if (terminationCallback_)
        terminationCallback_();
    responseTimer_.cancel();
    if (ec.failed())
        logger_->error(ec.message());
    logger_->trace("stopped collecting responses");
    co_return;
}

/////////////////////////////////////////////////////////////////////////////////
// Methods for synchronous communication provided via the public interface.

template<typename T>
T DoricoRemoteImpl::runAwaitableSync (awaitable<T> aw) {
    std::promise<T> promise;
    auto future = promise.get_future();
    net::co_spawn(
        ioc_,
        [aw = std::move(aw), p = std::move(promise)]() mutable -> awaitable<void> {
            try {
                T result = co_await std::move(aw);
                p.set_value(std::move(result));
            }
            catch (...) {
                p.set_exception(std::current_exception());
            }
            co_return;
        }, net::detached);

    // block caller until coroutine has finished
    return future.get();
}

template<>
void DoricoRemoteImpl::runAwaitableSync (awaitable<void> aw) {
    std::promise<void> promise;
    auto future = promise.get_future();
    net::co_spawn(
        ioc_,
        [aw = std::move(aw), p = std::move(promise)]() mutable -> awaitable<void> {
            try {
                co_await std::move(aw);
                p.set_value();
            }
            catch (...) {
                p.set_exception(std::current_exception());
            }
            co_return;
    }, net::detached);
    // block caller until coroutine has finished
    future.get();
}

/** Try to establish a connection to Dorico by requesting a new session. This requires
 *  the user to confirm a connection dialog shown in Dorico.
 *  @param[in] clientName name of client trying to connect to Dorico (appears in Dorico's client list)
 *  @param[in] host name or IP address (usually 'localhost' or 127.0.0.1, respectively)
 *  @param[in] port port number given in the Dorico settings (usually 4560)
 *  @return true if the connection has been established */
bool DoricoRemoteImpl::connect (const string &clientName, const string &host, const string &port) {
    return runAwaitableSync(asyncConnect(clientName, host, port));
}

/** Try to establish a connection to Dorico by using a previous session. If the given
 *  session token is valid, the connection is established without further user interaction.
 *  @param[in] clientName name of client trying to connect to Dorico (appears in Dorico's client list)
 *  @param[in] host name or IP address (usually 'localhost' or 127.0.0.1, respectively)
 *  @param[in] port port number given in the Dorico settings (usually 4560)
 *  @param[in] token session token of a previous connection
 *  @return true if the connection has been established */
bool DoricoRemoteImpl::connect (const string &clientName, const string &host, const string &port, const string &token) {
    return runAwaitableSync(asyncConnect(clientName, host, port, token));
}

/** Terminate the connection to Dorico. */
void DoricoRemoteImpl::disconnect () {
    runAwaitableSync(asyncDisconnect());
}

void DoricoRemoteImpl::send (const Request &request, Response &response, SendMode mode) {
    runAwaitableSync(asyncSend(request, response, mode));
}

/** Send a message request to Dorico.
 *  @param[in] request the request to be sent
 *  @param[in] mode allows to send the request even if Dorico hasn't confirmed the connection yet (mode=FORCE)
 *  @return the response received from Dorico */
template <class Request>
Request::Response DoricoRemoteImpl::send (Request &request, SendMode mode) {
    return runAwaitableSync(asyncSend(request, mode));
}

WSMessage DoricoRemoteImpl::getMessage (const string &type) const {
    auto it = receivedMsgs_.find(type);
    if (it != receivedMsgs_.end())
        return it->second;
    return json{};
}

string DoricoRemoteImpl::getMessageValue (const string &type, const string &key) const {
    return getMessage(type).getAsString(key);
}
