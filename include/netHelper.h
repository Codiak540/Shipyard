#ifndef NETHELPER_H
#define NETHELPER_H

#include <curl/curl.h>
#include <cstdio>
#include <iostream>

inline size_t write_data(const void* ptr, const size_t size, size_t nmemb, FILE* stream) {
	return fwrite(ptr, size, nmemb, stream);
}

inline int progress_callback(void*,
				  const curl_off_t total,
				  const curl_off_t now,
				  curl_off_t,
				  curl_off_t) {
	if (total > 1024 && now <= total) {
		const int percent = static_cast<int>((now * 100) / total);
		printf("\rDownloading: %3d%%", percent);
		fflush(stdout);
	}
	return 0;
}

inline bool download(const std::string& url, const std::string& path) {
	CURL* curl = curl_easy_init();
	if (!curl) return false;

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "ship/1.0");

	FILE* file = fopen(path.c_str(), "wb");
	if (!file) {
		curl_easy_cleanup(curl);
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

	// progress
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	const CURLcode res = curl_easy_perform(curl);

	printf("\n");

	fclose(file);
	curl_easy_cleanup(curl);

	return res == CURLE_OK;
}

#endif