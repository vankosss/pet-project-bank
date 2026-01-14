#include "models.h"
#include "db_pool.h"
#include "crow.h"
#include "course.h"
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

const std::string SECRET_KEY = "secret_token_for_user";

int get_current_user_id(const crow::request& request) {
	auto authorization = request.get_header_value("Authorization");

	if (authorization.empty() || authorization.find("Bearer ") != 0) {
		return -1;
	}

	std::string token_str = authorization.substr(7);
	try {
		auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token_str);
		auto verifier = jwt::verify<jwt::traits::nlohmann_json>().allow_algorithm(jwt::algorithm::hs256{ SECRET_KEY });
		verifier.verify(decoded);
		auto user_id_decode = decoded.get_payload_claim("user_id");
		return std::stoi(user_id_decode.as_string());
	}
	catch (...) {
		return -1;
	}
}

int main() {
	try {
		ConnectionPool pool("config.json");
		crow::SimpleApp app;

		CROW_ROUTE(app, "/users").methods("POST"_method) ([&pool](const crow::request& request) {

			auto data = crow::json::load(request.body);
			if (!data) {
				return crow::response(400, "Invalid JSON");
			}

			if (!data.has("username") || !data.has("password")) {
				return crow::response(400, "Missing username or password");
			}

			std::string username = data["username"].s();
			std::string password = data["password"].s();
			std::string password_hash = password + "_some_secret_method";

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				work.exec_params("INSERT INTO bank (username, password_hash) VALUES ($1, $2)", username, password_hash);
				work.commit();
				return crow::response(201, "User register succesffully");
			}
			catch (const pqxx::unique_violation& e) {
				return crow::response(409, std::string("Username is exists") + e.what());
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Execption: ") + e.what());
			}
			});

		CROW_ROUTE(app, "/tokens").methods("POST"_method) ([&pool](const crow::request& request) {
			auto data = crow::json::load(request.body);
			if (!data || !data.has("username") || !data.has("password")) {
				return crow::response(400, "Bad Request");
			}

			std::string username = data["username"].s();
			std::string password = data["password"].s();
			std::string password_hash = password + "_some_secret_method";

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec_params("SELECT id, password_hash FROM bank WHERE username = $1", username);

				if (result.empty()) {
					return crow::response(401, "Wrong username or password");
				}

				std::string database_hash = result[0]["password_hash"].c_str();

				if (password_hash != database_hash) {
					return crow::response(401, "Wrong username or password");
				}

				int user_id = result[0]["id"].as<int>();

				auto token = jwt::create<jwt::traits::nlohmann_json>().set_type("JWS").set_payload_claim("user_id", std::to_string(user_id)).set_issued_at(std::chrono::system_clock::now()).set_expires_at(std::chrono::system_clock::now() + std::chrono::hours(24)).sign(jwt::algorithm::hs256{ SECRET_KEY });

				crow::json::wvalue response_body;
				response_body["token"] = token;
				response_body["status"] = "success";
				return crow::response(200, response_body);

			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}

			});

		CROW_ROUTE(app, "/transactions").methods("POST"_method) ([&pool](const crow::request& request) {
			int sender_id = get_current_user_id(request);

			if (sender_id == -1) {
				return crow::response(401, "Unauthorized: Invalid token");
			}

			auto data = crow::json::load(request.body);
			if (!data || !data.has("to_username") || !data.has("amount")) {
				return crow::response(400, "Missing to_username or amount");
			}

			std::string receiver_username = data["to_username"].s();
			int64_t amount = data["amount"].i();

			if (amount <= 0) {
				return crow::response(400, "Amount must be positive");
			}

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec_params("SELECT id, is_banned FROM bank WHERE username = $1", receiver_username);

				if (result.empty()) {
					return crow::response(404, "Receiver not found");
				}

				int receiver_id = result[0]["id"].as<int>();
				bool is_banned_receiver = result[0]["is_banned"].as<bool>();

				if (sender_id == receiver_id) {
					return crow::response(400, "Cannot transfer money to yourself");
				}

				if (is_banned_receiver) {
					return crow::response(400, "Receiver is banned! You cannot complete the transaction");
				}

				pqxx::result result_sender = work.exec_params("SELECT balance, is_banned FROM bank WHERE id = $1", sender_id);

				if (result_sender.empty()) {
					return crow::response(401, "Sender not found");
				}

				int64_t sender_balance = result_sender[0]["balance"].as<int64_t>();
				bool is_banned_sender = result_sender[0]["is_banned"].as<bool>();

				if (sender_balance < amount) {
					return crow::response(400, "Insufficient funds");
				}

				if (is_banned_sender) {
					return crow::response(400, "You are blocked! You cannot preform transactions");
				}

				work.exec_params("UPDATE bank SET balance = balance - $1 WHERE id = $2", amount, sender_id);
				work.exec_params("UPDATE bank SET balance = balance + $1 WHERE id = $2", amount, receiver_id);
				work.exec_params("INSERT INTO transactions (sender_id, receiver_id, amount) VALUES ($1, $2, $3)", sender_id, receiver_id, amount);
				work.commit();
				return crow::response(200, "Transfer successful");

			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}

			});

		CROW_ROUTE(app, "/users/me").methods("GET"_method) ([&pool](const crow::request& request) {
			int user_id = get_current_user_id(request);

			if (user_id == -1) {
				return crow::response(401, "Unauthorized: Invalid token");
			}

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec_params("SELECT username, balance, access_rights FROM bank WHERE id = $1", user_id);

				if (result.empty()) {
					return crow::response(404, "User not found");
				}

				int64_t balance_usd = result[0]["balance"].as<int64_t>();
				double rate = Service::getInstance().get_usd_to_euro();
				double balance_euro = balance_usd * rate;
				std::stringstream stream;
				stream << std::fixed << std::setprecision(2) << balance_euro;

				crow::json::wvalue response_body;
				response_body["username"] = result[0]["username"].c_str();
				response_body["balance"] = balance_usd;
				response_body["balance_euro"] = stream.str();
				response_body["avatar_img"] = "https://example.com/default_avatar.png";
				response_body["access_rights"] = result[0]["access_rights"].c_str();
				return crow::response(200, response_body);
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}
			});

		CROW_ROUTE(app, "/main").methods("GET"_method) ([&pool]() {
			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec("SELECT count(*) FROM bank");
				int64_t realtime_count_of_users = result[0][0].as<int64_t>();

				crow::json::wvalue response_body;
				response_body["project"] = "Bank My Pet Project";
				response_body["description"] = "This project developed on C++ and Crow";
				response_body["count_users"] = realtime_count_of_users;

				return crow::response(200, response_body);
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}
			});

		CROW_ROUTE(app, "/transactions").methods("GET"_method) ([&pool](const crow::request& request) {
			int user_id = get_current_user_id(request);

			if (user_id == -1) {
				return crow::response(401, "Unauthorized: Invalid token");
			}

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec_params("SELECT t.amount, b_sender.username AS sender_name, b_receiver.username AS receiver_name, t.sender_id, TO_CHAR(t.transactions_time, 'DD.MM.YYYY') as date, TO_CHAR(t.transactions_time, 'HH24:MI') as time FROM transactions t JOIN bank b_sender ON t.sender_id = b_sender.id JOIN bank b_receiver ON t.receiver_id = b_receiver.id WHERE t.sender_id = $1 OR t.receiver_id = $1 ORDER BY t.id DESC LIMIT 50", user_id);
				std::vector<crow::json::wvalue> transactions_history;
				for (const auto& row : result) {
					crow::json::wvalue work_json;

					std::string sender = row["sender_name"].c_str();
					std::string receiver = row["receiver_name"].c_str();
					std::string date = row["date"].c_str();
					std::string time = row["time"].c_str();
					int64_t amount = row["amount"].as<int64_t>();
					int db_sender_id = row["sender_id"].as<int>();

					if (db_sender_id == user_id) {
						work_json["type"] = "outgoing";
						work_json["amount"] = -amount;
						work_json["receiver"] = receiver;
						work_json["date"] = date;
						work_json["time"] = time;
					}
					else {
						work_json["type"] = "incoming";
						work_json["amount"] = +amount;
						work_json["sender"] = sender;
						work_json["date"] = date;
						work_json["time"] = time;
					}
					transactions_history.push_back(work_json);
				}
				crow::json::wvalue response_body;
				response_body["transactions_history"] = std::move(transactions_history);
				return crow::response(200, response_body);
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}
			});

		CROW_ROUTE(app, "/jars").methods("GET"_method) ([&pool](const crow::request& request) {
			int user_id = get_current_user_id(request);

			if (user_id == -1) {
				return crow::response(401, "Unauthorized: Invalid token");
			}

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec_params("SELECT id, jar_balance, jar_name, jar_target, jar_accumulation_amount, jar_image FROM jars WHERE user_id = $1", user_id);

				std::vector<crow::json::wvalue> jars_list;
				for (const auto& row : result) {
					crow::json::wvalue jar;
					jar["id"] = result[0]["id"].as<int>();
					jar["jar_name"] = result[0]["jar_name"].c_str();
					jar["jar_target"] = result[0]["jar_target"].c_str();
					jar["jar_image"] = result[0]["jar_image"].c_str();
					jar["jar_balance"] = result[0]["jar_balance"].as<int64_t>();
					jar["jar_accumulation_amount"] = result[0]["jar_accumulation_amount"].as<int64_t>();
					jars_list.push_back(jar);
				}
				crow::json::wvalue response_body;
				response_body["jars"] = std::move(jars_list);
				return crow::response(200, response_body);
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}
			});

		CROW_ROUTE(app, "/jars").methods("POST"_method) ([&pool](const crow::request& request) {
			int user_id = get_current_user_id(request);

			if (user_id == -1) {
				return crow::response(401, "Unauthorized: Invalid token");
			}

			auto data = crow::json::load(request.body);
			if (!data || !data.has("name") || !data.has("accumulation_amount") || !data.has("target")) {
				return crow::response(400, "Name, accumulation amount and target required for created jar");
			}

			std::string name = data["name"].s();
			std::string target = data["target"].s();
			std::string image;
			if (data.has("image")) {
				image = data["image"].s();
			}
			else {
				image = "https://example.com/default_jar.png";
			}
			int64_t accumulation_amount = data["accumulation_amount"].i();

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec_params("SELECT is_banned FROM bank WHERE id = $1", user_id);
				bool is_banned_user = result[0]["is_banned"].as<bool>();

				if (is_banned_user) {
					return crow::response(400, "You are blocked! You cannot create a jar");
				}

				work.exec_params("INSERT INTO jars (user_id, jar_balance, jar_name, jar_target, jar_accumulation_amount, jar_image) VALUES ($1, $2, $3, $4, $5, $6)", user_id, 0, name, target, accumulation_amount, image);
				work.commit();
				return crow::response(201, "Jar created");
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}
			});

		CROW_ROUTE(app, "/jars/<int>/transactions").methods("POST"_method) ([&pool](const crow::request& request, int jar_id) {
			int user_id = get_current_user_id(request);

			if (user_id == -1) {
				return crow::response(401, "Unauthorized: Invalid token");
			}

			auto data = crow::json::load(request.body);

			if (!data || !data.has("type") || !data.has("amount")) {
				return crow::response(400, "Type and amount required for operation with jar");
			}

			std::string type = data["type"].s();
			int64_t amount = data["amount"].i();

			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result result = work.exec_params("SELECT is_banned FROM bank WHERE id = $1", user_id);
				bool is_banned_user = result[0]["is_banned"].as<bool>();
				pqxx::result jar_check = work.exec_params("SELECT jar_balance FROM jars WHERE user_id = $1 AND id = $2", user_id, jar_id);

				if (is_banned_user) {
					return crow::response(400, "You are blocked! You cannot conduct transactions with jars");
				}

				if (jar_check.empty()) {
					return crow::response(404, "Jar not found");
				}

				if (type == "withdraw") {
					pqxx::result check_balance = work.exec_params("SELECT jar_balance FROM jars WHERE user_id = $1 AND id = $2", user_id, jar_id);

					if (check_balance.empty()) {
						return crow::response(404, "Jar not found");
					}

					int64_t current_jar_balance = check_balance[0]["jar_balance"].as<int64_t>();

					if (current_jar_balance < amount) {
						return crow::response(400, "Insufficient fund in the jar");
					}

					work.exec_params("UPDATE jars SET jar_balance = jar_balance - $1 WHERE user_id = $2 AND id = $3", amount, user_id, jar_id);
					work.exec_params("UPDATE bank SET balance = balance + $1 WHERE id = $2", amount, user_id);
				}
				else if (type == "deposit") {
					pqxx::result check_balance = work.exec_params("SELECT balance FROM bank WHERE id = $1", user_id);
					int64_t current_user_balance = check_balance[0]["balance"].as<int64_t>();

					if (current_user_balance < amount) {
						return crow::response(400, "Insufficient funds on your balance");
					}

					work.exec_params("UPDATE jars SET jar_balance = jar_balance + $1 WHERE user_id = $2 AND id = $3", amount, user_id, jar_id);
					work.exec_params("UPDATE bank SET balance = balance - $1 WHERE id = $2", amount, user_id);
				}
				else {
					return crow::response(400, "Wrong method");
				}
				work.commit();
				return crow::response(200, "Transfer succesful");
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}
			});

		CROW_ROUTE(app, "/jars/<int>").methods("DELETE"_method) ([&pool](const crow::request& request, int jar_id) {
			int user_id = get_current_user_id(request);

			if (user_id == -1) {
				return crow::response(401, "Unauthorized: Invalid token");
			}
			try {
				DatabaseConnection database(pool);
				pqxx::work work(database.get());
				pqxx::result check_balance = work.exec_params("SELECT jar_balance FROM jars WHERE user_id = $1 AND id = $2", user_id, jar_id);

				if (check_balance.empty()) {
					return crow::response(400, "Jar not found");
				}

				int64_t current_jar_balance = check_balance[0]["jar_balance"].as<int64_t>();

				work.exec_params("UPDATE bank SET balance = balance + $1 WHERE id = $2", current_jar_balance, user_id);
				work.exec_params("DELETE FROM jars WHERE user_id = $1 AND id = $2", user_id, jar_id);
				work.commit();

				return crow::response(200, "Jar deleted");
			}
			catch (const std::exception& e) {
				return crow::response(500, std::string("Exception: ") + e.what());
			}
		});

		CROW_ROUTE(app, "/admintools").methods("GET"_method) ([&pool](const crow::request& request) {
		int user_id = get_current_user_id(request);

		if (user_id == -1){
			return crow::response(401, "Unauthorized: Invalid token");
		}

		try {
			DatabaseConnection database(pool);
			pqxx::work work(database.get());
			pqxx::result result = work.exec_params("SELECT access_rights FROM bank WHERE id = $1", user_id);
			std::string access_rights_user = result[0]["access_rights"].c_str();

			if (access_rights_user != "admin") {
				return crow::response(403, "You do not have sufficient rights to perform this action");
			}
			return crow::response(200, "You have successfully logged into AdminTools");
		}
		catch (const std::exception& e) {
			return crow::response(500, std::string("Exception: ") + e.what());
		}
	});

	CROW_ROUTE(app, "/users/<string>").methods("PATCH"_method) ([&pool](const crow::request& request, std::string username) {
		int user_id = get_current_user_id(request);

		if (user_id == -1) {
			return crow::response(401, "Unauthorized: Invalid token");
		}

		auto data = crow::json::load(request.body);
		if (!data) {
			return crow::response(400, "Invalid JSON");
		}

		try {
			DatabaseConnection database(pool);
			pqxx::work work(database.get());
			pqxx::result result = work.exec_params("SELECT access_rights FROM bank WHERE id = $1", user_id);
			std::string access_rights_user = result[0]["access_rights"].c_str();

			if (access_rights_user != "admin") {
				return crow::response(400, "You do not have sufficient rights to perform this action");
			}
			pqxx::result check_target = work.exec_params("SELECT id FROM bank WHERE username = $1", username);

			if (check_target.empty()) {
				return crow::response(404, "User with this username not found");
			}

			if (data.has("is_banned")) {
				bool should_ban = data["is_banned"].b();

				if (should_ban) {
					std::string reason = data.has("reason") ? data["reason"].s() : std::string("no reason");
					work.exec_params("UPDATE bank SET is_banned = TRUE, ban_reason = $1 WHERE username = $2", reason, username);
				}
				else {
					std::string unban_reason = data.has("unban_reason") ? data["unban_reason"].s() : std::string("no reason");
					work.exec_params("UPDATE bank SET is_banned = FALSE, ban_reason = 'user is not banned', unban_reason = $1 WHERE username = $2", unban_reason, username);
				}
			}
			work.commit();
			return crow::response(200, "Done");
		}
		catch (const std::exception& e) {
			return crow::response(500, std::string("Exception: ") + e.what());
		}
	});

		app.port(18080).multithreaded().run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}