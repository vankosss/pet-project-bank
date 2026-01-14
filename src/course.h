#pragma once
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <cpr/cpr.h>
#include "crow.h"

class Service {
private:
	double usd_to_euro = 0.0;
	std::chrono::steady_clock::time_point last_update;
	std::mutex mtx;

	Service() = default;

public:
	static Service& getInstance() {
		static Service instance;
		return instance;
	}

	double get_usd_to_euro() {
		std::lock_guard<std::mutex> lock(mtx);
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count();

		if (usd_to_euro > 0 && elapsed < 600) {
			return usd_to_euro;
		}

		cpr::Response response = cpr::Get(cpr::Url{ "https://api.frankfurter.app/latest" }, cpr::Parameters{ { "from", "USD" }, { "to", "EUR" } });

		if (response.status_code == 200) {
			auto json = crow::json::load(response.text);
			if (json && json.has("rates") && json["rates"].has("EUR")) {
				usd_to_euro = json["rates"]["EUR"].d();
				last_update = now;
				return usd_to_euro;
			}
		}
		std::cerr << "Error fetching rates" << std::endl;
		return (usd_to_euro > 0) ? usd_to_euro : 0.0;
	}

};