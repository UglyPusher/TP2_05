#include "pch.h"
#include "include/tp2_net/websocket.hpp"
#include <utility>

namespace TP2::net {
	NetErr DummyWebSocket::connect(const std::string & url,
		const WebSocketOptions & opt,
		const WebSocketHandlers & h)
	{
		// Сохраняем состояние заглушки
		url_ = url;
		options_ = opt;
		handlers_ = h;
		
		// Считаем, что соединение всегда успешно
		if (handlers_.on_open) handlers_.on_open();
		return NetErr::Ok;
	}
	
	NetErr DummyWebSocket::send(std::string_view text)
	{
		// Если "соединение" не установлено — считаем закрытым
		if (url_.empty()) return NetErr::Closed;
		
		/// Новый обработчик (handlers_.on_text) и legacy (on_msg_)
		if (handlers_.on_text) handlers_.on_text(text);
		if (on_msg_)           on_msg_(text);
		return NetErr::Ok;
	}
	
	void DummyWebSocket::on_message(OnMsg cb)
	{
		on_msg_ = std::move(cb);
	}
	
	void DummyWebSocket::close(unsigned short code, std::string_view reason) noexcept
	{
		// Помечаем как “закрыто”
		const bool was_open = !url_.empty();
		url_.clear();
		
		if (was_open && handlers_.on_close) {
			handlers_.on_close(code, reason);
		}
	}
} // namespace TP2::net
