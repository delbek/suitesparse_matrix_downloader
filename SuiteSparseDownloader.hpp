#pragma once

#include <curl/curl.h>

#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <iostream>

class SuiteSparseDownloader
{
public:
	struct MatrixFilter
	{
		std::optional<std::string> group;
		std::optional<std::string> name;
		std::optional<std::string> groupContains;
		std::optional<std::string> nameContains;
		std::optional<std::string> kindContains;

		std::optional<unsigned long long> minRows;
		std::optional<unsigned long long> maxRows;
		std::optional<unsigned long long> minCols;
		std::optional<unsigned long long> maxCols;
		std::optional<unsigned long long> minNonzeros;
		std::optional<unsigned long long> maxNonzeros;

		std::optional<bool> isReal;
		std::optional<bool> isBinary;
		std::optional<bool> is2d3d;
		std::optional<bool> isPosDef;

		std::optional<double> minPatternSymmetry;
		std::optional<double> minNumericSymmetry;

		std::optional<bool> isSquare;
	};

private:
	struct MatrixInfo
	{
		unsigned id;
		std::string groupName;
		std::string name;
		unsigned long long rows;
		unsigned long long cols;
		unsigned long long nonzeros;
		bool isReal;
		bool isBinary;
		bool is2d3d;
		bool isPosDef;
		double patternSymmetry;
		double numericSymmetry;
		std::string kind;
	};

public:
	SuiteSparseDownloader()
	{
		auto code = curl_global_init(CURL_GLOBAL_ALL);
		if (code != 0)
		{
			throw std::runtime_error("curl_global_init failed");
		}
	}

	~SuiteSparseDownloader()
	{
		curl_global_cleanup();
	}

	std::vector<std::string> getMatrices(const MatrixFilter& filter)
	{
		std::string csvText = httpGetToString("https://sparse.tamu.edu/files/ssstats.csv");
		std::vector<MatrixInfo> matrices = parseCSV(csvText);

		std::vector<std::string> out;
		out.reserve(matrices.size());

		for (const auto& m: matrices)
		{
			if (!matchesFilter(m, filter))
			{
				continue;
			}
			out.push_back(buildDownloadLink(m));
		}

		return out;
	}

	std::vector<std::string> downloadMatrices(const std::vector<std::string>& links, const std::string& downloadFolder)
	{
		if (links.empty())
		{
			return {};
		}

		std::filesystem::path folderPath = downloadFolder;
		if (!std::filesystem::exists(folderPath))
		{
			std::filesystem::create_directories(folderPath);
		}

		std::vector<std::string> savedPaths;
		savedPaths.reserve(links.size());

		for (const std::string& url: links)
		{
			std::string fileNameTarGz = fileNameFromUrl(url);
			std::filesystem::path tarPath = folderPath / fileNameTarGz;

			std::string baseName;
			{
				std::string tmp = fileNameTarGz;
				std::string suffix = ".tar.gz";
				if (tmp.size() >= suffix.size() && tmp.substr(tmp.size() - suffix.size()) == suffix)
				{
					baseName = tmp.substr(0, tmp.size() - suffix.size());
				}
				else
				{
					baseName = tmp;
				}
			}
			std::filesystem::path mtxDestPath = folderPath / (baseName + ".mtx");
			std::filesystem::path absPath = std::filesystem::absolute(mtxDestPath);
			savedPaths.push_back(absPath.string());
			if (std::filesystem::exists(absPath.string()))
			{
				continue;
			}
			httpGetToFile(url, tarPath.string());

			std::string extractCmd = "tar -xvf \"" + tarPath.string() + "\" -C \"" + folderPath.string() + "\"";
			int extractStatus = std::system(extractCmd.c_str());
			if (extractStatus != 0)
			{
				throw std::runtime_error("tar extraction failed for " + tarPath.string());
			}

			std::filesystem::path extractedDirPath = folderPath / baseName;
			std::filesystem::path mtxSourcePath = extractedDirPath / (baseName + ".mtx");

			std::error_code ecMove;
			std::filesystem::rename(mtxSourcePath, mtxDestPath, ecMove);
			if (ecMove)
			{
				throw std::runtime_error("failed to move mtx file: " + ecMove.message());
			}

			std::error_code ecRmDir;
			std::filesystem::remove_all(extractedDirPath, ecRmDir);

			std::error_code ecRmTar;
			std::filesystem::remove(tarPath, ecRmTar);
		}

		return savedPaths;
	}

private:
	static unsigned curlWriteToString(void* contents, unsigned size, unsigned nmemb, void* userp)
	{
		unsigned realSize = size * nmemb;
		std::string* mem = static_cast<std::string*>(userp);
		mem->append(static_cast<const char*>(contents), realSize);
		return realSize;
	}

	std::string httpGetToString(const std::string& url)
	{
		CURL* curl = curl_easy_init();
		if (!curl)
		{
			throw std::runtime_error("curl_easy_init failed");
		}

		std::string buffer;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &SuiteSparseDownloader::curlWriteToString);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "suitesparse-downloader-cpp/1.0");

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			std::string err = curl_easy_strerror(res);
			curl_easy_cleanup(curl);
			throw std::runtime_error("curl_easy_perform failed: " + err);
		}

		long httpCode = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_easy_cleanup(curl);

		if (httpCode != 200)
		{
			throw std::runtime_error("HTTP " + std::to_string(httpCode) + " for URL: " + url);
		}

		return buffer;
	}

	void httpGetToFile(const std::string& url, const std::string& outPath)
	{
		CURL* curl = curl_easy_init();
		if (!curl)
		{
			throw std::runtime_error("curl_easy_init failed");
		}

		FILE* fp = std::fopen(outPath.c_str(), "wb");
		if (!fp)
		{
			curl_easy_cleanup(curl);
			throw std::runtime_error("failed to open file for writing: " + outPath);
		}

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "suitesparse-downloader-cpp/1.0");

		CURLcode res = curl_easy_perform(curl);
		std::fclose(fp);

		if (res != CURLE_OK)
		{
			::remove(outPath.c_str());
			std::string err = curl_easy_strerror(res);
			curl_easy_cleanup(curl);
			throw std::runtime_error("curl_easy_perform failed: " + err);
		}

		long httpCode = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_easy_cleanup(curl);

		if (httpCode != 200)
		{
			::remove(outPath.c_str());
			throw std::runtime_error("HTTP " + std::to_string(httpCode) + " for URL: " + url);
		}
	}

	std::vector<std::string> splitCSVLine(const std::string& line)
	{
		std::vector<std::string> out;
		std::stringstream ss(line);
		std::string cell;

		while (std::getline(ss, cell, ','))
		{
			unsigned start = 0;
			while (start < cell.size() && std::isspace(static_cast<unsigned char>(cell[start])))
			{
				start++;
			}
			auto end = cell.size();
			while (end > start && std::isspace(static_cast<unsigned char>(cell[end - 1])))
			{
				end--;
			}
			out.push_back(cell.substr(start, end - start));
		}
		return out;
	}

	std::vector<MatrixInfo> parseCSV(const std::string& csvText)
	{
		std::vector<MatrixInfo> matrices;
		std::istringstream in(csvText);
		std::string line;
		unsigned lineNo = 0;

		while (std::getline(in, line))
		{
			if (line.empty())
			{
				continue;
			}
			lineNo++;

			if (lineNo <= 2)
			{
				continue;
			}

			std::vector<std::string> cols = splitCSVLine(line);
			if (cols.size() < 12)
			{
				continue;
			}

			MatrixInfo m;
			m.id = lineNo - 2;
			m.groupName = cols[0];
			m.name = cols[1];
			m.rows = std::stoull(cols[2]);
			m.cols = std::stoull(cols[3]);
			m.nonzeros = std::stoull(cols[4]);
			m.isReal = (cols[5] == "1");
			m.isBinary = (cols[6] == "1");
			m.is2d3d = (cols[7] == "1");
			m.isPosDef = (cols[8] == "1");
			m.patternSymmetry = std::atof(cols[9].c_str());
			m.numericSymmetry = std::atof(cols[10].c_str());
			m.kind = cols[11];

			matrices.push_back(m);
		}

		return matrices;
	}

	bool matchesFilter(const MatrixInfo& m, const MatrixFilter& f)
	{
		if (f.group && m.groupName != *f.group)
		{
			return false;
		}
		if (f.name && m.name != *f.name)
		{
			return false;
		}
		if (f.groupContains && m.groupName.find(*f.groupContains) == std::string::npos)
		{
			return false;
		}
		if (f.nameContains && m.name.find(*f.nameContains) == std::string::npos)
		{
			return false;
		}
		if (f.kindContains && m.kind.find(*f.kindContains) == std::string::npos)
		{
			return false;
		}

		if (f.minRows && m.rows < *f.minRows)
		{
			return false;
		}
		if (f.maxRows && m.rows > *f.maxRows)
		{
			return false;
		}
		if (f.minCols && m.cols < *f.minCols)
		{
			return false;
		}
		if (f.maxCols && m.cols > *f.maxCols)
		{
			return false;
		}
		if (f.minNonzeros && m.nonzeros < *f.minNonzeros)
		{
			return false;
		}
		if (f.maxNonzeros && m.nonzeros > *f.maxNonzeros)
		{
			return false;
		}

		if (f.isReal && m.isReal != *f.isReal)
		{
			return false;
		}
		if (f.isBinary && m.isBinary != *f.isBinary)
		{
			return false;
		}
		if (f.is2d3d && m.is2d3d != *f.is2d3d)
		{
			return false;
		}
		if (f.isPosDef && m.isPosDef != *f.isPosDef)
		{
			return false;
		}

		if (f.minPatternSymmetry && m.patternSymmetry < *f.minPatternSymmetry)
		{
			return false;
		}
		if (f.minNumericSymmetry && m.numericSymmetry < *f.minNumericSymmetry)
		{
			return false;
		}

		if (f.isSquare && ((*f.isSquare && m.rows != m.cols) || (!*f.isSquare && m.rows == m.cols)))
		{
			return false;
		}

		return true;
	}

	std::string buildDownloadLink(const MatrixInfo& m)
	{
		return "https://sparse.tamu.edu/MM/" + m.groupName + "/" + m.name + ".tar.gz";
	}

	std::string fileNameFromUrl(const std::string& url)
	{
		auto pos = url.find_last_of('/');
		if (pos == std::string::npos)
		{
			return url;
		}
		return url.substr(pos + 1);
	}
};
