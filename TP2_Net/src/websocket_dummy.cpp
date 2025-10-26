#include "pch.h"
#include "include/tp2_net/websocket.hpp"

namespace TP2::net {
	void DummyWebSocket::connect(const std::string& url) { url_ = url; }
	void DummyWebSocket::send(std::string_view data) { if (on_msg_) on_msg_(data); }
	void DummyWebSocket::on_message(OnMsg cb) { on_msg_ = std::move(cb); }
	void DummyWebSocket::close() { on_msg_ = nullptr; }
} // namespace TP2::net
