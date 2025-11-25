#include "pch.h"
#include <string>

#include "include/tp2_crypto/signer.hpp"
#include "include/tp2_crypto/utils.hpp"
#include <nlohmann/json.hpp>


namespace TP2::crypto {
	std::string BybitSigner::sign(std::string_view payload) {
		
		nlohmann::json j = nlohmann::json::parse(payload);

		// std::string ts = now_ms_string();
		uint64_t exp = TP2::crypto::now_ms() + 10000;
		std::string ts = std::to_string(exp);


		return bybit_sign(
			j.at("api_key").get<std::string>(),
			ts,
			j.at("recvWindow").get<std::string>(),
			j.at("body").get<std::string>()  // stringified json body
		);
	};

	std::string BinanceSigner::sign(std::string_view payload) {

		nlohmann::json j = nlohmann::json::parse(payload);

		std::string ts = now_ms_string();

		return ":";
	}
}