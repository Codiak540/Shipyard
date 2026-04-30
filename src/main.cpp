#include "../include/main.h"

#include <iostream>
#include <sys/stat.h>

#include "netHelper.h"
#include "port.h"
#include "../include/colors.h"
#include "../include/jsonHelper.h"

Port* port;

int installFromSource(const std::string& inputPath, const std::string& pkgName) {
			std::string srcDir = inputPath;
			std::string tmpExtractDir; // non-empty when we created a temp dir

			// If input is a tarball, extract it first
			if (isTarball(inputPath)) {
				std::cout << CYAN << "Extracting tarball, please be patient!" << RESET << std::endl;

				// Create a temp staging directory inside SHIP_BAY
				tmpExtractDir = static_cast<std::string>(SHIP_BAY) + "tmp/tmp_build_XXXXXX";
				// mkdtemp needs a writable char buffer
				char tmpBuf[PATH_MAX];
				strncpy(tmpBuf, tmpExtractDir.c_str(), sizeof(tmpBuf) - 1);

				if (!directoryExists(static_cast<std::string>(SHIP_BAY) + "tmp"))
					system(("mkdir -p " + static_cast<std::string>(SHIP_BAY) + "tmp").c_str());

				if (!mkdtemp(tmpBuf)) {
					std::cout << BOLDRED << "Failed to create temp directory!" << RESET << std::endl;
					return 1;
				}
				tmpExtractDir = tmpBuf;

				srcDir = extractTarball(inputPath, tmpExtractDir);
				if (srcDir.empty()) {
					std::cout << BOLDRED << "Extraction failed!" << RESET << std::endl;
					system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
					return 1;
				}
				std::cout << GREEN << "Extracted to: " << srcDir << RESET << std::endl;
			}

			// Derive package name from the source directory's basename
			// const std::string pkgName = normalizeName(tarballStem(getDirName(srcDir)));

			// Check for existing installation
			bool alreadyInstalled = isInstalled(pkgName);
			if (alreadyInstalled) {
				std::cout << BOLDRED
					<< "Package '" << pkgName << "' is already installed. "
					"Continuing will reinstall."
					<< RESET << std::endl;
			}

			if (tolower(outputButActuallyInput(
				static_cast<std::string>(YELLOW) +
				"Would you like to build & install '" + pkgName + "'? [Y/n]" +
				RESET)[0]) != 'y') {
				if (!tmpExtractDir.empty())
					system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
				return 0;
			}

			if (alreadyInstalled) {
				std::string mutableName = pkgName;
				if (!uninstallPackage(mutableName)) {
					std::cout << BOLDRED << "Failed to uninstall existing package!" << RESET << std::endl;
					if (!tmpExtractDir.empty())
						system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
					return 1;
				}
			}

			if (!buildAndInstallPackage(srcDir, pkgName)) {
				// Clean up temp dir on failure too
				if (!tmpExtractDir.empty())
					system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
				return 1;
			}

	std::cout << GREEN << "Build & installation complete!" << RESET << std::endl;
	return 0;
}

int main(int argc, char** args) {
	if (!directoryExists(SHIP_BAY)) system(("mkdir " + static_cast<std::string>(SHIP_BAY)).c_str());
	if (!pathExists(PACKAGES)) { system("touch " PACKAGES); }

	if (argc >= 2) {
		std::string cmd = args[1];

		if (cmd == "help") {
			std::cout << GREEN <<
				":'######::'##::::'##:'####:'########::'##:::'##::::'###::::'########::'########::\n"
				"'##... ##: ##:::: ##:. ##:: ##.... ##:. ##:'##::::'## ##::: ##.... ##: ##.... ##:\n"
				" ##:::..:: ##:::: ##:: ##:: ##:::: ##::. ####::::'##:. ##:: ##:::: ##: ##:::: ##:\n"
				". ######:: #########:: ##:: ########::::. ##::::'##:::. ##: ########:: ##:::: ##:\n"
				":..... ##: ##.... ##:: ##:: ##.....:::::: ##:::: #########: ##.. ##::: ##:::: ##:\n"
				"'##::: ##: ##:::: ##:: ##:: ##::::::::::: ##:::: ##.... ##: ##::. ##:: ##:::: ##:\n"
				". ######:: ##:::: ##:'####: ##::::::::::: ##:::: ##:::: ##: ##:::. ##: ########::\n"
				":......:::..:::::..::....::..::::::::::::..:::::..::........:::\n"
				YELLOW << "Shipyard V" << VERSION << "\n"
				MAGENTA << "Commands:\n" << CYAN
				"   help:               " << BLUE << "(usage: ship help)                        " << CYAN <<
				"Show this screen\n"
				"   dock/install        " << BLUE << "(usage: ship dock <path> <exec>)          " << CYAN <<
				"Install a pre-built package (folder or tarball)\n"
				"   build               " << BLUE << "(usage: ship build <path> [cfg-args])     " << CYAN <<
				"Build & install via Makefile/CMake (folder or tarball)\n"
				"   jettison/uninstall: " << BLUE << "(usage: ship jettison <package>)          " << CYAN <<
				"Uninstall a package\n"
				"   service/update      " << BLUE << "(usage: ship service [package])           " << CYAN <<
				"Update the package database/a package\n"
				"   manifest/list       " << BLUE << "(usage: ship manifest)                    " << CYAN <<
				"Displays a list of all installed packages\n"
				"   inspect                " << BLUE << "(usage: ship inspect <package>)              " << CYAN <<
				"Show details for an installed package\n"
				"   port                " << BLUE << "(usage: ship port <url> [command])        " << CYAN <<
				"Use the package list from another port\n"
				<< RESET << std::endl;
			return 0;
		}

		if (cmd == "port") {
			port = new Port(args[2]);
		}
		else
			port = new Port(DEFAULT_PORT);

		if (cmd == "update" || cmd == "service") {
			if (std::tolower(outputButActuallyInput(static_cast<std::string>(YELLOW) + "Are you sure you want to update? [Y/n]" + RESET)[0]) == 'y') {
				auto res = port->getPackageListJson(true);
				if (res == NULL) {
					std::cout << BOLDRED << "FAILED TO UPDATE!" << RESET << std::endl;
				}
				else {
					std::cout << GREEN << "Updated package list for port: \"" << port->getPortName() << "\" Successfully" << RESET << std::endl;
				}
				return 0;
			}
		}

		if (cmd == "manifest" || cmd == "list") {
			listPackages();
			return 0;
		}

		if (cmd == "inspect") {
			if (argc < 3) {
				std::cout << BOLDRED << "Usage: ship info <package>" << RESET << std::endl;
				return 0;
			}

			showPackageInfo(args[2]);
			return 0;
		}

		if (cmd == "jettison" || cmd == "uninstall") {
			if (argc < 3) {
				std::cout << BOLDRED << "Usage: ship jettison <package>" << RESET << std::endl;
				return 1;
			}

			if (outputButActuallyInput(static_cast<std::string>(YELLOW) + "Are you sure you want to uninstall " + static_cast<std::string>(args[2]) + "? [Y/n]" + RESET)[
				0] == 'y') {
				std::cout << BOLDRED << "Uninstalling..." << RESET << std::endl;

				std::string pkgName = args[2];

				if (uninstallPackage(pkgName))
					std::cout << GREEN << "Successfully Uninstalled Package" << RESET << std::endl;
				else {
					std::cout << BOLDRED << "Failed to uninstall package!" << RESET << std::endl;
					return 1;
				}
			}
			return 0;
		}

		// build command
		// Usage: ship build <folder-or-tarball> [configure-args...]
		if (cmd == "build")
			return installFromSource(args[2], args[2]);

		if (cmd == "dock" || cmd == "install") {

		}
	}

	//  dock / install command 
	// Usage: ship dock <folder-or-tarball> <executable>
	if (argc < 3) {
		std::cout << BOLDRED << "Usage: ship dock <folder-or-tarball> <executable>" << RESET << std::endl;
		return 0;
	}

	std::string inputPath = args[2];
	const bool local = inputPath[0] == '.';
	if (!local) {
		download(port->getPackageURL(inputPath), inputPath + ".tar.gz");
		std::string tmpExtractDir = "./";
		tmpExtractDir += inputPath;
		tmpExtractDir += ".tar.gz";
		inputPath = getAbsolutePath(tmpExtractDir);
		std::cout<<inputPath<<std::endl;
	}

	std::string srcPath = inputPath; // may change if we untar
	std::string tmpExtractDir;


	if (isTarball(inputPath)) {
		std::cout << CYAN << "Extracting tarball, please be patient!" << RESET << std::endl;

		tmpExtractDir = static_cast<std::string>(SHIP_BAY) + "tmp/tmp_dock_XXXXXX";
		char tmpBuf[PATH_MAX];
		strncpy(tmpBuf, tmpExtractDir.c_str(), sizeof(tmpBuf) - 1);

		if (!directoryExists(static_cast<std::string>(SHIP_BAY) + "tmp"))
			system(("mkdir -p " + static_cast<std::string>(SHIP_BAY) + "tmp").c_str());

		if (!mkdtemp(tmpBuf)) {
			std::cout << BOLDRED << "Failed to create temp directory!" << RESET << std::endl;
			return 1;
		}
		tmpExtractDir = tmpBuf;

		srcPath = extractTarball(inputPath, tmpExtractDir);
		if (srcPath.empty()) {
			std::cout << BOLDRED << "Extraction failed!" << RESET << std::endl;
			system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
			return 1;
		}
		std::cout << GREEN << "Extracted to: " << srcPath << RESET << std::endl;
	}

	std::string execName;
	if (argc < 4) {
		if (local) {
			std::cout << BOLDRED << "Usage: ship dock <folder-or-tarball> <executable>" << RESET << std::endl;
			if (!tmpExtractDir.empty())
				system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
			return 0;
		}
		else {
			execName = port->getPackageExecutable(args[2]);
		}
	}
	else
		execName = normalizeName(args[3]);


	const std::string dirName = normalizeName(getDirName(srcPath));
	std::string installationDir = SHIP_BAY + execName + "/";
	const std::string execPath = installationDir + execName;

	bool shouldReinstall = directoryExists(installationDir) || isInstalled(execName);
	if (shouldReinstall)
		std::cout << BOLDRED << "Package already installed! Continuing will reinstall!" << RESET << std::endl;

	if (tolower(
			outputButActuallyInput(static_cast<std::string>(YELLOW) + "Would you like to install? [Y/n]" + RESET)[0]) !=
		'y') {
		if (!tmpExtractDir.empty())
			system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
		return 0;
	}

	const std::string ver = local ? std::to_string(std::stoi(getExecutableVersion(execName)) + 1) : port->getPackageVersion(args[2]);

	if (shouldReinstall) {
		std::cout << RED << "Reinstalling " << execName << RESET << std::endl;

		bool success = uninstallPackage(execName);

		if (!success) {
			std::cout << BOLDRED << "Failed to uninstall package!" << RESET << std::endl;
			if (!tmpExtractDir.empty())
				system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
			return 1;
		}
		std::cout << BOLDGREEN << "Successfully uninstalled " << execName << RESET << std::endl;
	}

	if (local || port->getPackageType(args[2]) == "binary") {
		savePackage(execName, ver, installationDir, execPath);
		system(("cp -r \"" + srcPath + "\" \"" + SHIP_BAY + execName + "\"").c_str());
		if (!tmpExtractDir.empty())
			system(("rm -rf \"" + tmpExtractDir + "\"").c_str());
		system(("chmod +x " + execPath).c_str());
		system(("ln -sf " + execPath + " /usr/bin/" + execName).c_str());
	} else {
		// source build path for remote non-binary packages
		installationDir.pop_back();
		system(("rm -rf \"" + installationDir + "\"").c_str());
		system(("mv \"" + tmpExtractDir + "\" \"" + installationDir + "\"").c_str());
		const std::string buildRoot = findBuildRoot(installationDir);
		installFromSource(buildRoot, execName);
	}

	std::cout << GREEN << "Installation complete!" << RESET << std::endl;

	free(port);
	return 0;
}
