#ifndef SHIP_JSONHELPER_H
#define SHIP_JSONHELPER_H

#include <fstream>
#include <vector>
#include <string>

#include "colors.h"
#include "json.hpp"
#include "utils.h"

#define PACKAGES "/usr/shipped/packages.json"
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// savePackage
//
// Writes (or updates) a package entry in the registry.
//
// Parameters:
//   name         - logical package name (already normalized)
//   version      - integer version counter
//   instDir      - source/install directory (used by dock installs)
//   execPath     - path to the primary executable (dock installs; "" for build)
//   files        - list of every file installed on the real system (build/cmake
//                  installs); empty for dock installs that use instDir instead
// ---------------------------------------------------------------------------
inline void savePackage(const std::string& name,
						std::string ver,
						const std::string& instDir,
						const std::string& execPath,
						const std::vector<std::string>& files = {}) {

	json j;

	std::ifstream in(PACKAGES);
	if (in) {
		try { in >> j; } catch (...) { /* start fresh if malformed */ }
	}

	const std::string key = normalizeName(name);

	json entry = {
		{"version",    ver},
		{"instDir",    instDir},
		{"executable", execPath}
	};

	if (!files.empty()) {
		entry["files"] = files;
	}

	j["installed"][key] = entry;

	std::ofstream out(PACKAGES);
	out << j.dump(4);
}

// ---------------------------------------------------------------------------
// getInstalledFiles  -  returns the file manifest for a build/cmake package,
// or an empty vector for dock-installed packages (they use instDir instead).
// ---------------------------------------------------------------------------
inline std::vector<std::string> getInstalledFiles(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) return {};

	json j;
	try { in >> j; } catch (...) { return {}; }

	if (!j.contains("installed") || !j["installed"].contains(name))
		return {};

	auto& pkg = j["installed"][name];
	if (!pkg.contains("files")) return {};

	std::vector<std::string> files;
	for (const auto& f : pkg["files"])
		files.push_back(f.get<std::string>());
	return files;
}

inline bool isInstalled(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) return false;

	json j;
	try { in >> j; } catch (...) { return false; }

	return j.contains("installed") && j["installed"].contains(name);
}

inline std::string getExecPath(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) return "";

	json j;
	try { in >> j; } catch (...) { return ""; }

	if (!j.contains("installed") || !j["installed"].contains(name)) return "";
	return j["installed"][name].value("executable", "");
}

inline void listPackages() {
	std::ifstream in(PACKAGES);
	if (!in) {
		std::cout << RED << "No packages installed." << RESET << std::endl;
		return;
	}

	json j;
	try { in >> j; } catch (...) {
		std::cout << RED << "Package database is malformed." << RESET << std::endl;
		return;
	}

	if (!j.contains("installed") || j["installed"].empty()) {
		std::cout << RED << "No packages installed." << RESET << std::endl;
		return;
	}

	std::cout << YELLOW << "Installed packages:" << CYAN << std::endl;

	for (auto& [name, data] : j["installed"].items()) {
		const bool hasBuildFiles = data.contains("files") && !data["files"].empty();
		std::cout << "  - " << name
				  << " (v" << data.value("version", "?") << ")"
				  << (hasBuildFiles ? "  [build]" : "  [dock]")
				  << "\n";
	}

	std::cout << RESET;
}

inline void showPackageInfo(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) {
		std::cout << BOLDRED << "No package database found." << RESET << std::endl;
		return;
	}

	json j;
	try { in >> j; } catch (...) {
		std::cout << BOLDRED << "Package database is malformed." << RESET << std::endl;
		return;
	}

	if (!j.contains("installed") || !j["installed"].contains(name)) {
		std::cout << BOLDRED << "Package not found: " << name << RESET << std::endl;
		return;
	}

	auto& pkg = j["installed"][name];

	std::cout << CYAN;
	std::cout << "Package:     " << name << "\n";
	std::cout << "Version:     " << pkg.value("version", "?") << "\n";
	std::cout << "Install Dir: " << pkg.value("instDir", "(none)") << "\n";
	std::cout << "Executable:  " << pkg.value("executable", "(none)") << "\n";

	if (pkg.contains("files") && !pkg["files"].empty()) {
		std::cout << "Files (" << pkg["files"].size() << "):\n";
		for (const auto& f : pkg["files"])
			std::cout << "  " << f.get<std::string>() << "\n";
	}

	std::cout << RESET;
}

inline std::string getExecutableName(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) return "";

	json j;
	try { in >> j; } catch (...) { return ""; }

	if (!j.contains("installed") || !j["installed"].contains(name)) {
		std::cout << BOLDRED << "Package not found: " << name << RESET << std::endl;
		return "";
	}

	return j["installed"][name].value("executable", "");
}

inline std::string getExecutableInstallDir(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) return "";

	json j;
	try { in >> j; } catch (...) { return ""; }

	if (!j.contains("installed") || !j["installed"].contains(name))
		return "";

	return j["installed"][name].value("instDir", "");
}

inline std::string getExecutableVersion(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) return "0";

	json j;
	try { in >> j; } catch (...) { return "0"; }

	if (!j.contains("installed") || !j["installed"].contains(name))
		return "0";

	return j["installed"][name].value("version", "0");
}

inline void removePackage(const std::string& name) {
	std::ifstream in(PACKAGES);
	if (!in) {
		std::cout << BOLDRED << "No package database." << RESET << std::endl;
		return;
	}

	json j;
	try { in >> j; } catch (...) {
		std::cout << BOLDRED << "Package database is malformed." << RESET << std::endl;
		return;
	}

	if (!j.contains("installed") || !j["installed"].contains(name)) {
		std::cout << YELLOW << "Package not in registry: " << name << RESET << std::endl;
		return;
	}

	j["installed"].erase(name);

	std::ofstream out(PACKAGES);
	out << j.dump(4);

	std::cout << GREEN << "Removed package from registry: " << name << RESET << std::endl;
}

#endif //SHIP_JSONHELPER