#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

class Logger {
public:
	Logger(const std::string& infilename, const std::string& outfilename, const std::string& idinfilename, const std::string& idoutfilename) :
		infilename_(infilename), outfilename_(outfilename), idinfilename_(idinfilename), idoutfilename_(idoutfilename),
		lastIdIn_(0), lastIdOut_(0) {
		std::ofstream fileout(idoutfilename_, std::ios::trunc);
		if (fileout.is_open()) {
			fileout << lastIdIn_ << std::endl;
			fileout.close();
		}
		readThread_ = std::thread(&Logger::run, this);
	}

	~Logger() {
		readThread_.join();
	}

	void logMessage(const std::string& message) {
		std::lock_guard<std::mutex> lock(mutex_);

		std::vector<json> list;
		long long idhasbeenread = 0;
		std::ifstream fileoutid(idoutfilename_);
		if (fileoutid.is_open()) {
			std::string line;
			try {
				std::getline(fileoutid, line);
				try {
					idhasbeenread = std::stoll(line);
				}
				catch (const std::invalid_argument) {
					cerr << "Invalid argument" << "\n";
				}
			}
			catch (std::ifstream::failure e) {
				std::cerr << "Exception happened: " << e.what() << std::endl;
			}
			//cout << idhasbeenread << endl;
			fileoutid.close();
		}
		

		if (idhasbeenread > idhasbeenread_last) {
			std::ifstream fileout(outfilename_);
			if (fileout.is_open()) {
				std::string line;
				while (std::getline(fileout, line)) {
					try {
						json j = json::parse(line);
						long long id = j["id"];
						if (id > idhasbeenread) {
							list.push_back(j);
						}
					}
					catch (json::parse_error& e) {
						std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
					}
				}
				fileout.close();
			}
		}

		json j;
		j["id"] = ++lastIdOut_;
		j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
		j["message"] = message;

		std::ofstream file;
		if (idhasbeenread > idhasbeenread_last) {
			file.open(outfilename_, std::ios::trunc);
		}
		else {
			file.open(outfilename_, std::ios::app);
		}
		idhasbeenread_last = idhasbeenread;
		if (file.is_open()) {
			for (int i = 0; i < list.size(); i++) {
				file << list[i].dump() << endl;
			}
			file << j.dump() << std::endl;
			file.close();
		}
		else {
			// Обработка ошибки открытия файла
		}
	}

	std::vector<json> getCommands() {
		std::lock_guard<std::mutex> lock(mutex_);
		std::vector<json> commands = commands_;
		commands_.clear();
		return commands;
	}

private:
	void run() {
		while (true) {
			std::ifstream file(infilename_);
			if (file.is_open()) {
				std::lock_guard<std::mutex> lock(mutex_);
				std::string line;
				while (std::getline(file, line)) {
					try {
						json j = json::parse(line);
						long long id = j["id"];
						if (id > lastIdIn_) {
							lastIdIn_ = id;
							commands_.push_back(j);
						}
					}
					catch (json::parse_error& e) {
						std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
					}
				}
				file.close();
			}
			std::ofstream fileout(idoutfilename_, std::ios::trunc);
			if (fileout.is_open()) {
				fileout << lastIdIn_ << std::endl;
				fileout.close();
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
	}

	std::string infilename_, outfilename_;
	std::string idinfilename_, idoutfilename_;
	long long lastIdIn_;
	long long lastIdOut_;
	std::mutex mutex_;
	std::vector<json> commands_;
	std::thread readThread_;
	long long idhasbeenread_last = -1;
};

int main(int argc, char* argv[]) {
	if (argc != 5) {
		return 0;
	}
	Logger logger(argv[1], argv[2], argv[3], argv[4]);

	std::string message;
	int counter = 0;
	while (true) {
		//std::cout << "Enter message: ";
		//std::getline(std::cin, message);
		message = "Hello " + std::to_string(counter);
		counter += 1;
		logger.logMessage(message);

		std::vector<json> commands = logger.getCommands();
		for (const json& command : commands) {
			std::cout << "Command received: " << command["message"] << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	return 0;
}
