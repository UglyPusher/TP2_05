#include "pch.h"
#include <string>

#include "include/tp2_crypto/signer.hpp"
#include "include/tp2_crypto/utils.hpp"
#include <nlohmann/json.hpp>


namespace TP2::crypto {
	std::string BybitSigner::sign(std::string_view payload) {
		
		nlohmann::json j = nlohmann::json::parse(payload);

		std::string ts = now_ms_string();


		return bybit_sign(
			j.at("secret").get<std::string>(),
			ts,
			j.at("method").get<std::string>(),
			j.at("path").get<std::string>(),
			j.at("body").get<std::string>()  // stringified json body
		);
	};

	std::string BinanceSigner::sign(std::string_view payload) {

		nlohmann::json j = nlohmann::json::parse(payload);

		std::string ts = now_ms_string();

		return ":";
	}
}