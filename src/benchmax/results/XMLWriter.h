#pragma once

#include <fstream>
#include <map>
#include <string>

namespace benchmax {

class XMLWriter {
private:
	std::ofstream mFile;
	
	void replace(std::string& s, const std::string& pattern, const std::string& replacement) const {
		std::size_t pos = 0;
		while ((pos = s.find(pattern, pos)) != std::string::npos) {
			s.replace(pos, pattern.length(), replacement);
			pos += replacement.length();
		}
	}
	std::string sanitize(const std::string& s) const {
		std::string res(s);
		replace(res, "<", "&lt;");
		replace(res, ">", "&gt;");
		return res;
	}
public:
	XMLWriter(const std::string& filename): mFile(filename) {
		mFile << "<?xml version=\"1.0\"?>" << std::endl;
		mFile << "<benchmarksets>" << std::endl;
	}
	
	void write(const std::map<const Tool*, std::size_t>& tools, const std::map<fs::path, std::size_t>& files, const std::map<std::pair<std::size_t,std::size_t>, BenchmarkResult>& results) {
		mFile << "\t<solvers>" << std::endl;
		for (const auto& tool: tools) {
			mFile << "\t\t<solver solver_id=\"" << sanitize(tool.first->binary().native()) << "\" />" << std::endl;
		}
		mFile << "\t</solvers>" << std::endl;
		
		mFile << "\t<benchmarkset name=\"all\">" << std::endl;
		for (const auto& file: files) {
			mFile << "\t\t<benchmarkfile name=\"" << sanitize(file.first.native()) << "\">" << std::endl;
			for (const auto& tool: tools) {
				std::pair<std::size_t, std::size_t> resultID(tool.second, file.second);
				auto it = results.find(resultID);
				if (it == results.end()) continue;
				mFile << "\t\t\t<run solver_id=\"" << sanitize(tool.first->binary().native()) << "\" timeout=\"" << Settings::timeLimit << "\">" << std::endl;
				if (!it->second.additional.empty()) {
					mFile << "\t\t\t\t<runtimestats>" << std::endl;
					mFile << "\t\t\t\t\t<module name=\"All\">" << std::endl;
					for (const auto& stat: it->second.additional) {
						mFile << "\t\t\t\t\t\t<stat name=\"" << sanitize(stat.first) << "\" value=\"" << stat.second << "\" />" << std::endl;
					}
					mFile << "\t\t\t\t\t</module>" << std::endl;
					mFile << "\t\t\t\t</runtimestats>" << std::endl;
				}
				mFile << "\t\t\t\t<results>" << std::endl;
				mFile << "\t\t\t\t\t<result name=\"runtime\" type=\"msec\">" << it->second.time << "</result>" << std::endl;
				mFile << "\t\t\t\t\t<result name=\"answer\" type=\"\">";
				switch (it->second.exitCode) {
					case 2: mFile << "sat"; break;
					case 3: mFile << "unsat"; break;
					case 4: mFile << "unknown"; break;
					case 5: mFile << "error"; break;
					case 11: mFile << "timeout"; break;
					case 12: mFile << "memout"; break;
					default: mFile << "segfault";
				}
				mFile << "</result>" << std::endl;
				mFile << "\t\t\t\t</results>" << std::endl;
				mFile << "\t\t\t</run>" << std::endl;
			}
			mFile << "\t\t</benchmarkfile>" << std::endl;
		}
		mFile << "\t</benchmarkset>" << std::endl;
	}
	
	~XMLWriter() {
		mFile << "</benchmarksets>" << std::endl;
		mFile.close();
	}
};
	
}