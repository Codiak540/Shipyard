#include "port.h"

#include <fstream>
#include <string>
#include <utility>

#include "main.h"
#include "netHelper.h"
#define FILE (PORTS_DIR + getPortFileName() + ".json")
Port::Port(std::string url) : url(std::move(url)) {}

Port::Port() : Port("https://raw.githubusercontent.com/Codiak540/Shipyard/refs/heads/main/port.json") {};

Port::~Port() = default;

std::string Port::getUrl() { return url; }

std::string Port::getPortName() {
	if (!cachedPortName.empty()) return cachedPortName;

	const std::string bootstrapFile = PORTS_DIR + cleanupUrl(this->url);
	cachedPortName = cleanupUrl(this->url);

	json j = this->getPackageListJson(false);
	if (j.contains("port_name") && j["port_name"].is_string()) {
		cachedPortName = j["port_name"].get<std::string>();
		cachedPortFileName = ""; // clear so getPortFileName rebuilds cleanly

		// Build real filename inline without touching cachedPortFileName
		std::string realFileName;
		for (char c : cachedPortName)
			realFileName += (c == ' ') ? '_' : std::tolower(c);

		const std::string realFile = PORTS_DIR + realFileName + ".json";
		if (bootstrapFile != realFile)
			system(("rm -f " + bootstrapFile).c_str());
	}

	return cachedPortName;
}

std::string Port::getPortFileName() {
	if (!cachedPortFileName.empty()) return cachedPortFileName;

	for (char c : this->getPortName())
		cachedPortFileName += (c == ' ') ? '_' : std::tolower(c);

	return cachedPortFileName;
}

std::string Port::getPackageType(const std::string& name) {
	json j = this->getPackageListJson(false);
	if (!j.contains(name) || j[name]["type"].is_null()) return "";
	return j[name]["type"];
}

nlohmann::json Port::getPackageListJson(const bool sync) {
	nlohmann::json list;

	if (!directoryExists(PORTS_DIR)) {
		system(("mkdir -p " + PORTS_DIR).c_str());
	}

	try {
		for (int attempts = 0; attempts <= 3; attempts++) {

			if (pathExists(FILE) && !sync) {
				std::ifstream in(FILE);
				if (in) {
					in >> list;
					return list;
				}
			} else {
				system(("rm -f " + FILE).c_str());
			}

			if (download(getUrl(), FILE)) {
				std::ifstream in(FILE);
				if (in) {
					in >> list;
					return list;
				}
			}
		}
	}
	catch (nlohmann::detail::parse_error err) {
		std::cerr << "Failed to parse downloaded list!" << std::endl;
	}

	return nullptr;
}

std::string Port::getPackageURL(const std::string& name) {
	json j = this->getPackageListJson(false);
	if (!j.contains(name) || j[name]["link"].is_null()) return "";
	return j[name]["link"];
}

std::string Port::getPackageVersion(const std::string& name) {
	json j = this->getPackageListJson(false);
	if (!j.contains(name) || j[name]["version"].is_null()) return "0";
	return j[name]["version"];
}

std::string Port::getPackageExecutable(const std::string& name) {
	json j = this->getPackageListJson(false);
	if (!j.contains(name) || j[name]["name"].is_null()) return "";
	return j[name]["name"];
}
