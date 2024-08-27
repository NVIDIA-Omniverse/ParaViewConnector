/*###############################################################################
#
# Copyright 2024 NVIDIA Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
###############################################################################*/

#pragma once

#include "vtkLogger.h"

#define OMNI_LOG_LOG(verbosity, ...) vtkVLogF(verbosity, __VA_ARGS__);
#define OMNI_LOG_F_LOG(verbosity, format, ...) vtkVLogF(verbosity, format, __VA_ARGS__);

// macros to indent all log messages within a scope.
#define OMNI_LOG_INFO_SCOPE_START(id) \
    pqOmniConnectLogger::logScopeStart(id)

#define OMNI_LOG_INFO_SCOPE_END(id) \
    pqOmniConnectLogger::logScopeEnd(id)

// useful macro to log variable value of different types
#define OMNI_LOG_VAR(varName, value) \
    pqOmniConnectLogger::logVar(varName, value)

// macros for logging info, warning, error
#define OMNI_LOG_INFO(...) \
    OMNI_LOG_LOG(vtkLogger::VERBOSITY_INFO, __VA_ARGS__)

#define OMNI_LOG_WARNING(...) \
    OMNI_LOG_LOG(vtkLogger::VERBOSITY_WARNING, __VA_ARGS__)

#define OMNI_LOG_ERROR(...) \
    OMNI_LOG_LOG(vtkLogger::VERBOSITY_ERROR, __VA_ARGS__)

class pqOmniConnectLogger {
public:
	static void logScopeStart(const char* id);
	static void logScopeEnd(const char* id);

	static void logVar(const char* varName, const char* value);
	static void logVar(const char* varName, int value);
	static void logVar(const char* varName, bool value);
};

