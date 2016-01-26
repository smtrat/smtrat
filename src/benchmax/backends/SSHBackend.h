/**
 * @file SSHBackend.h
 * @author Gereon Kremer <gereon.kremer@cs.rwth-aachen.de>
 */

#pragma once

#include <future>
#include <thread>

#ifdef USE_BOOST_REGEX
#include "../../cli/config.h"
#ifdef __VS
#pragma warning(push, 0)
#include <boost/regex.hpp>
#pragma warning(pop)
#else
#include <boost/regex.hpp>
#endif
using boost::regex;
using boost::regex_match;
#else
#include <regex>
using std::regex;
using std::regex_match;
#endif

#include "BackendData.h"
#include "../newssh/SSHScheduler.h"

namespace benchmax {

#define USE_STD_ASYNC

class SSHBackend: public Backend {
private:
#ifdef USE_STD_ASYNC
	std::queue<std::future<bool>> jobs;
#else
	std::queue<std::thread> jobs;
#endif
	ssh::SSHScheduler* scheduler;
	
protected:
	virtual void startTool(const Tool* tool) {
		scheduler->uploadTool(tool);
	}
	virtual void execute(const Tool* tool, const fs::path& file) {
		//BENCHMAX_LOG_WARN("benchmax", "Executing...");
#if 1
#ifdef USE_STD_ASYNC
		jobs.push(std::async(std::launch::async, &ssh::SSHScheduler::executeJob, scheduler, tool, file, std::ref(mResults)));
#else
		jobs.push(std::thread(&ssh::SSHScheduler::executeJob, scheduler, tool, file, std::ref(mResults)));
#endif
#else
		scheduler->executeJob(tool, file, mResults);
#endif
	}
public:
	SSHBackend(): Backend() {
		scheduler = new ssh::SSHScheduler();
	}
	~SSHBackend() {
		while (!jobs.empty()) {
#ifdef USE_STD_ASYNC
			jobs.front().wait();
#else
			jobs.front().join();
#endif
			madeProgress();
			jobs.pop();
		}
	}
};

}
