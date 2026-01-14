#pragma once
#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <fstream>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

class ConnectionPool {
private:
	std::queue<std::shared_ptr<pqxx::connection>> connections;
	std::mutex mtx;
	std::condition_variable conditional_variable;
	size_t pool_size;

public:
	ConnectionPool(const std::string& config) {
		std::ifstream file(config);
		if (!file.is_open()) {
			throw std::runtime_error("Config file not open");
		}

		nlohmann::json data = nlohmann::json::parse(file);
		auto database_config = data["database"];

		std::string connection_string = "host=" + database_config["host"].get<std::string>() + " port=" + std::to_string(database_config["port"].get<int>()) + " dbname=" + database_config["dbname"].get<std::string>() + " user=" + database_config["user"].get<std::string>() + " password=" + database_config["password"].get<std::string>();
		pool_size = database_config["pool_size"].get<int>();

		for (size_t i = 0; i < pool_size; ++i) {
			try {
				auto connection = std::make_shared<pqxx::connection>(connection_string);
				if (connection->is_open()) {
					connections.push(connection);
				}
				else {
					std::cerr << "Error! Failed to open connection" << std::endl;
				}
			}
			catch (std::exception& e) {
				std::cerr << "Exception: " << e.what() << std::endl;
			}
		}
	}

	std::shared_ptr<pqxx::connection> get_connection() {
		std::unique_lock<std::mutex> lock(mtx);
		while (connections.empty()) {
			conditional_variable.wait(lock);
		}

		auto connection = connections.front();
		connections.pop();
		return connection;
	}

	void return_connection(std::shared_ptr<pqxx::connection> connection) {
		std::unique_lock<std::mutex> lock(mtx);
		connections.push(connection);
		lock.unlock();
		conditional_variable.notify_one();
	}
};

class DatabaseConnection {
private:
	ConnectionPool& pool;
	std::shared_ptr<pqxx::connection> connection;

public:
	DatabaseConnection(ConnectionPool& object) : pool(object), connection(object.get_connection()) {

	}

	pqxx::connection& get() {
		return *connection;
	}

	~DatabaseConnection() {
		pool.return_connection(connection);
	}
};