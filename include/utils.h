#ifndef SHIP_UTILS_H
#define SHIP_UTILS_H

#include <string>
#include <iostream>

inline std::string outputButActuallyInput(const std::string& output) {
	std::string in;
	std::cout << output << std::endl;
	std::cin >> in;
	return in;
}

inline std::string normalizeName(std::string name) {
	// remove path
	size_t pos = name.find_last_of('/');
	if (pos != std::string::npos)
		name = name.substr(pos + 1);

	// remove leading "./"
	if (name.rfind("./", 0) == 0)
		name = name.substr(2);

	return name;
}

// Returns true if the path looks like a tarball we can handle
inline bool isTarball(const std::string& path) {
	// Ordered longest-suffix first so .tar.gz doesn't match as .gz
	static const char* exts[] = {
		".tar.gz", ".tar.bz2", ".tar.xz", ".tar.zst",
		".tgz", ".tbz2", ".txz",
		nullptr
	};
	for (int i = 0; exts[i]; ++i) {
		const std::string ext = exts[i];
		if (path.size() > ext.size() &&
			path.compare(path.size() - ext.size(), ext.size(), ext) == 0)
			return true;
	}
	return false;
}

// Strip all known tarball extensions to get a bare stem, e.g.
// "foo-1.2.tar.gz" -> "foo-1.2"
inline std::string tarballStem(const std::string& filename) {
	static const char* exts[] = {
		".tar.gz", ".tar.bz2", ".tar.xz", ".tar.zst",
		".tgz", ".tbz2", ".txz",
		nullptr
	};
	std::string stem = filename;
	for (int i = 0; exts[i]; ++i) {
		const std::string ext = exts[i];
		if (stem.size() > ext.size() &&
			stem.compare(stem.size() - ext.size(), ext.size(), ext) == 0) {
			stem = stem.substr(0, stem.size() - ext.size());
			break;
		}
	}
	return stem;
}

// Extract a tarball into destDir (which must already exist).
// Returns the path of the top-level directory that was created, or ""
// on failure.  We use `tar --strip-components=0` and then ask `tar -tf`
// to discover the actual top-level directory name.
inline std::string extractTarball(const std::string& tarPath,
                                  const std::string& destDir) {
	// 1. Extract
	const std::string extractCmd =
		"tar -xf \"" + tarPath + "\" -C \"" + destDir + "\"";
	if (system(extractCmd.c_str()) != 0)
		return "";

	// 2. Find the top-level directory name reported by tar
	const std::string listCmd =
		"tar -tf \"" + tarPath + "\" 2>/dev/null | head -1";
	FILE* fp = popen(listCmd.c_str(), "r");
	if (!fp) return "";

	char buf[4096] = {};
	fgets(buf, sizeof(buf), fp);
	pclose(fp);

	std::string topEntry(buf);
	// Strip trailing newline / slash
	while (!topEntry.empty() &&
		   (topEntry.back() == '\n' || topEntry.back() == '\r' ||
		    topEntry.back() == '/'))
		topEntry.pop_back();

	// Only keep the first path component (some archives list "dir/subdir/file")
	const size_t slash = topEntry.find('/');
	if (slash != std::string::npos)
		topEntry = topEntry.substr(0, slash);

	if (topEntry.empty()) return "";

	return destDir + "/" + topEntry;
}

#endif //SHIP_UTILS_H