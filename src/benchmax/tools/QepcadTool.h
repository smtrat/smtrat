/**
 * @file   QepcadTool.h
 *		 Created on April 14, 2013, 6:09 PM
 * @author: Sebastian Junges
 * @author: Ulrich Loup
 * @version 2013-04-24
 *
 */

#ifndef QEPCADTOOL_H
#define QEPCADTOOL_H

#include "../Tool.h"

class QepcadTool:
	public Tool
{
	public:
		QepcadTool(const std::string& pathToTool);
		virtual std::string getCallToTool(const std::string& extraArguments = "");
		virtual BenchmarkResult getAnswer(const std::string& output);
		#ifdef BENCHMAX_USE_SMTPARSER
		virtual bool convertInput(Smt2Input* input);
		#endif
	private:

};

#endif   /* QEPCADTOOL_H */