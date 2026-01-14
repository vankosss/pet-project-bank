#pragma once
#include <string>

struct bank {
	int id;
	std::string username;
	std::string password_hash;
	int64_t balance;
};

struct transactions {
	int id;
	int sender_id;
	int receiver_id;
	int64_t amount;
	std::string transactions_time;
};

struct jars {
	int id;
	int user_id;
	int64_t jar_balance;
	std::string jar_name;
	std::string jar_target;
	int64_t jar_accumulation_amount;
	std::string jar_image;
};