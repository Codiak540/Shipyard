#ifndef SHIP_PORT_H
#define SHIP_PORT_H
#include <string>

#include "json.hpp"
#include "main.h"
#include "iostream"

#define PORTS_DIR (static_cast<std::string>(SHIP_BAY) + "ports/")
#define DEFAULT_PORT "https://raw.githubusercontent.com/Codiak540/Shipyard/refs/heads/main/port.json"
class Port {
public:
	Port();
	explicit Port(std::string url);
	~Port();

	std::string getUrl();
	std::string getPortName();
	std::string getPortFileName();
	nlohmann::json getPackageListJson(bool sync);
	std::string getPackageURL(const std::string& name);
	std::string getPackageVersion(const std::string& name);
	std::string getPackageType(const std::string& name);
	std::string getPackageExecutable(const std::string& name);

private:
	std::string url;
	std::string cachedPortName;
	std::string cachedPortFileName;
};


#endif //SHIP_PORT_H
