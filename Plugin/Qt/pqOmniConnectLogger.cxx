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

#include "pqOmniConnectLogger.h"

#include <cstdarg>

static constexpr int BUFFER_SIZE = 128;

void pqOmniConnectLogger::logScopeStart(const char* id) {
	// we use this logScopeStart wrapper function to hide the actual file name and line no. from the log
	// after vtkLogStartScope macro expansion
	vtkLogStartScope(INFO, id);
}

void pqOmniConnectLogger::logScopeEnd(const char* id) {
	// we use this logScopeEnd wrapper function to hide the actual file name and line no. from the log
	// after vtkLogEndScope macro expansion
	vtkLogEndScope(id);
}

void pqOmniConnectLogger::logVar(const char* varName, const char* value) {
	OMNI_LOG_F_LOG(vtkLogger::VERBOSITY_INFO, "%s = %s", varName, value);
}

void pqOmniConnectLogger::logVar(const char* varName, int value) {
	OMNI_LOG_F_LOG(vtkLogger::VERBOSITY_INFO, "%s = %d", varName, value);
}

void pqOmniConnectLogger::logVar(const char* varName, bool value) {
	OMNI_LOG_F_LOG(vtkLogger::VERBOSITY_INFO, "%s = %s", varName, value ? "true" : "false");
}