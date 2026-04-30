#ifndef SHIP_MAIN_H
#define SHIP_MAIN_H

#define VERSION "0.1"
#define SHIP_BAY "/usr/shipped/"
#include <string>
#include <vector>
#include <sys/stat.h>
#include <climits>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <set>
#include <algorithm>
#include <dirent.h>

#include "jsonHelper.h"

inline std::string cleanupUrl(const std::string& url) {
	std::string clean;
	for (int i = url.length(); i > 0; i--) {
		if (url[i] == '/') {
			if (clean.empty()) continue;
			break;
		}
		clean += url[i];
	}
	std::reverse(clean.begin(), clean.end());
	return clean;
}

inline bool uninstallPackage(std::string& package) {
	if (package.empty()) return false;
	const std::string name = normalizeName(package);
	if (name.empty()) return false;

	// --- 1. Remove files tracked in the manifest (build/cmake installs) -----
	std::vector<std::string> manifest = getInstalledFiles(name);
	if (!manifest.empty()) {
		for (const auto& file : manifest) {
			if (!file.empty())
				::remove(file.c_str());
		}
		// Attempt to prune now-empty directories left behind
		// (best-effort; ignore failures)
		std::set<std::string> dirs;
		for (const auto& file : manifest) {
			size_t pos = file.find_last_of('/');
			if (pos != std::string::npos)
				dirs.insert(file.substr(0, pos));
		}
		// Sort longest-first so we remove deepest dirs first
		std::vector<std::string> sortedDirs(dirs.begin(), dirs.end());
		std::sort(sortedDirs.begin(), sortedDirs.end(),
			[](const std::string& a, const std::string& b){ return a.size() > b.size(); });
		for (const auto& dir : sortedDirs)
			::rmdir(dir.c_str()); // silently fails if non-empty — that's fine
	}

	// --- 2. Remove the package's own install directory (dock installs) -------
	std::string dir = getExecutableInstallDir(name);
	if (!dir.empty() && dir != "/" && dir.rfind(SHIP_BAY, 0) == 0) {
		// Only nuke paths that live under SHIP_BAY for safety
		system(("rm -rf \"" + dir + "\"").c_str());
	}

	// --- 3. Remove the /usr/bin symlink (dock installs) ----------------------
	system(("unlink /usr/bin/" + name + " 2>/dev/null").c_str());

	// --- 4. Remove from JSON registry ----------------------------------------
	removePackage(name);

	return true;
}

inline std::string getDirName(const std::string& path) {
	size_t pos = path.find_last_of('/');
	if (pos == std::string::npos) return path;
	return path.substr(pos + 1);
}

inline std::string getInstallationDirectory(const std::string& path) {
	return SHIP_BAY + getDirName(path) + "/";
}

inline std::string getAbsolutePath(const std::string& path) {
	char buffer[PATH_MAX];
	if (!realpath(path.c_str(), buffer)) return path; // return original on failure
	return buffer;
}

inline bool pathExists(const std::string& path) {
	return access(path.c_str(), F_OK) == 0;
}

// If srcDir doesn't contain build files but has exactly one subdirectory,
// descend into it (handles tarballs that extract as dir/dir/).
inline std::string findBuildRoot(const std::string& srcDir) {
	if (pathExists(srcDir + "/configure") ||
		pathExists(srcDir + "/CMakeLists.txt") ||
		pathExists(srcDir + "/Makefile"))
		return srcDir;

	DIR* dp = opendir(srcDir.c_str());
	if (!dp) return srcDir;

	std::string candidate;
	int dirCount = 0;
	dirent* entry;
	while ((entry = readdir(dp)) != nullptr) {
		std::string n = entry->d_name;
		if (n == "." || n == "..") continue;
		std::string full = srcDir + "/" + n;
		struct stat st{};
		if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
			candidate = full;
			dirCount++;
		}
	}
	closedir(dp);

	if (dirCount == 1) return findBuildRoot(candidate); // recurse
	return srcDir;
}

inline bool directoryExists(const std::string& path) {
	struct stat info{};
	if (stat(path.c_str(), &info) != 0) return false;
	return (info.st_mode & S_IFDIR) != 0;
}

// ---------------------------------------------------------------------------
// collectFiles.  recursively walk `dir` and return every regular file path.
// Used after a DESTDIR install to build the manifest of installed files.
// ---------------------------------------------------------------------------
inline void collectFilesHelper(const std::string& dir,
								std::vector<std::string>& out) {
	DIR* dp = opendir(dir.c_str());
	if (!dp) return;

	struct dirent* entry;
	while ((entry = readdir(dp)) != nullptr) {
		const std::string name = entry->d_name;
		if (name == "." || name == "..") continue;

		const std::string fullPath = dir + "/" + name;

		struct stat st{};
		if (stat(fullPath.c_str(), &st) != 0) continue;

		if (S_ISDIR(st.st_mode)) {
			collectFilesHelper(fullPath, out);
		} else if (S_ISREG(st.st_mode)) {
			out.push_back(fullPath);
		}
	}

	closedir(dp);
}

inline std::vector<std::string> collectFiles(const std::string& dir) {
	std::vector<std::string> files;
	collectFilesHelper(dir, files);
	return files;
}

// ---------------------------------------------------------------------------
// buildAndInstallPackage
//
// Strategy:
//   - Install into a DESTDIR staging area so we can enumerate every file that
//     `make install` / `cmake --install` drops.
//   - Strip the staging prefix from each path to get the real destination.
//   - Copy files from staging → real destination.
//   - Record the full list in the JSON manifest so uninstall can reverse it.
//
// Supports both classic Makefile (./configure + make + make install) and
// CMake (cmake + make + make install), chosen automatically.
//
// Parameters:
//   srcDir        - absolute path to the unpacked source directory
//   pkgName       - logical package name stored in the registry
//   configureArgs - extra flags forwarded to ./configure (may be "")
//
// Returns true on full success, false if any mandatory step fails.
// ---------------------------------------------------------------------------
inline bool buildAndInstallPackage(std::string& srcDir,
								   const std::string& pkgName,
								   const std::string& configureArgs = "") {

	srcDir = findBuildRoot(srcDir);

	// Staging directory: files land here first so we can catalogue them
	const std::string stageDir = static_cast<std::string>(SHIP_BAY) + "staging/" + pkgName;
	system(("mkdir -p \"" + stageDir + "\"").c_str());

	bool built = false;

	//  CMake path 
	if (pathExists(srcDir + "/CMakeLists.txt")) {
		std::cout << CYAN << "Detected CMake project." << RESET << std::endl;

		const std::string buildDir = srcDir + "/build";
		system(("mkdir -p \"" + buildDir + "\"").c_str());

		// Configure
		std::string cmakeCmd = "cd \"" + buildDir + "\" && cmake .. -DCMAKE_INSTALL_PREFIX=/usr";
		if (!configureArgs.empty()) cmakeCmd += " " + configureArgs;

		std::cout << CYAN << "Running cmake configure..." << RESET << std::endl;
		if (system(cmakeCmd.c_str()) != 0) {
			std::cout << BOLDRED << "cmake configure failed!" << RESET << std::endl;
			return false;
		}

		// Build
		std::cout << CYAN << "Running cmake --build..." << RESET << std::endl;
		if (system(("cmake --build \"" + buildDir + "\" --parallel").c_str()) != 0) {
			std::cout << BOLDRED << "cmake build failed!" << RESET << std::endl;
			return false;
		}

		// Install into staging
		std::cout << CYAN << "Running cmake --install (staging)..." << RESET << std::endl;
		const std::string installCmd =
			"cmake --install \"" + buildDir + "\" --prefix \"" + stageDir + "\"";
		if (system(installCmd.c_str()) != 0) {
			std::cout << BOLDRED << "cmake install failed!" << RESET << std::endl;
			return false;
		}

		built = true;
	}

	//  Makefile path (also runs if no CMakeLists.txt) 
	if (!built) {
		// Optional ./configure step
		const std::string configurePath = srcDir + "/configure";
		if (pathExists(configurePath)) {
			std::cout << CYAN << "Running ./configure..." << RESET << std::endl;
			std::string configureCmd =
				"cd \"" + srcDir + "\" && ./configure --prefix=/usr";
			if (!configureArgs.empty()) configureCmd += " " + configureArgs;

			if (system(configureCmd.c_str()) != 0) {
				std::cout << BOLDRED << "./configure failed!" << RESET << std::endl;
				return false;
			}
		} else {
			std::cout << YELLOW << "No ./configure found — skipping." << RESET << std::endl;
		}

		// make
		std::cout << CYAN << "Running make..." << RESET << std::endl;
		if (system(("cd \"" + srcDir + "\" && make").c_str()) != 0) {
			std::cout << BOLDRED << "make failed!" << RESET << std::endl;
			return false;
		}

		// make install DESTDIR=<staging>
		std::cout << CYAN << "Running make install (staging)..." << RESET << std::endl;
		const std::string installCmd =
			"cd \"" + srcDir + "\" && make install DESTDIR=\"" + stageDir + "\"";
		if (system(installCmd.c_str()) != 0) {
			std::cout << BOLDRED << "make install failed!" << RESET << std::endl;
			return false;
		}

		built = true;
	}

	//  Collect manifest & deploy from staging → real system 
	std::vector<std::string> stagedFiles = collectFiles(stageDir);
	if (stagedFiles.empty()) {
		std::cout << BOLDRED
			<< "Install produced no files in staging dir — aborting."
			<< RESET << std::endl;
		system(("rm -rf \"" + stageDir + "\"").c_str());
		return false;
	}

	std::vector<std::string> installedFiles;
	bool copyFailed = false;

	for (const auto& stagedPath : stagedFiles) {
		// Real destination = strip the stageDir prefix
		std::string realDest = stagedPath.substr(stageDir.size());
		if (realDest.empty()) continue;

		// Ensure parent directory exists
		size_t lastSlash = realDest.find_last_of('/');
		if (lastSlash != std::string::npos) {
			const std::string parentDir = realDest.substr(0, lastSlash);
			system(("mkdir -p \"" + parentDir + "\"").c_str());
		}

		const std::string copyCmd =
			"cp -p \"" + stagedPath + "\" \"" + realDest + "\"";
		if (system(copyCmd.c_str()) != 0) {
			std::cout << YELLOW << "Warning: failed to copy " << realDest << RESET << std::endl;
			copyFailed = true;
			continue;
		}
		installedFiles.push_back(realDest);
	}

	// Clean up staging area
	system(("rm -rf \"" + stageDir + "\"").c_str());

	if (copyFailed && installedFiles.empty()) {
		std::cout << BOLDRED << "No files were installed!" << RESET << std::endl;
		return false;
	}

	//  Register in the Shipyard database 
	const std::string verStr = getExecutableVersion(pkgName);
	int newVer;
	try { newVer = std::stoi(verStr) + 1; } catch (...) {}

	savePackage(pkgName, std::to_string(newVer), srcDir + "/", "", installedFiles);

	std::cout << GREEN
		<< "Installed " << installedFiles.size() << " file(s)."
		<< RESET << std::endl;
	return true;
}

#endif //SHIP_MAIN_H