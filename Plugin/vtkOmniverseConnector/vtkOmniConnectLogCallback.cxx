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

#include "vtkOmniConnectLogCallback.h"
#include "vtkErrorCode.h"
#include "vtkOutputWindow.h"

#include "OmniConnect.h"

static void ConnectLogCallback(OmniConnectLogLevel level, void* userData, const char* message) noexcept
{
  bool logEnabled = vtkOmniConnectLogCallback::EnableDebug;
  const char* preamble = "Omniverse log: ";
    
  if (level == OmniConnectLogLevel::ERR)
  {
    logEnabled = vtkOmniConnectLogCallback::EnableError;
    preamble = "Omniverse error: ";
  }

  if (logEnabled && message)
  {
    size_t msglen = strlen(message);
    if (msglen > 0)
    {
      const char* lastcharptr = message + msglen - 1;
  
      vtkOStreamWrapper::EndlType endl;
      vtkOStreamWrapper::UseEndl(endl);
      vtkOStrStreamWrapper vtkmsg;
      if (*message != ' ')
        vtkmsg << preamble;
      if (*lastcharptr != ' ')
        vtkmsg << message << "\n\n";
      else
        vtkmsg << message;
      
      if (level == OmniConnectLogLevel::ERR)
        vtkOutputWindowDisplayErrorText(vtkmsg.str());
      else if (level == OmniConnectLogLevel::WARNING)
        vtkOutputWindowDisplayWarningText(vtkmsg.str());
      else
        vtkOutputWindowDisplayDebugText(vtkmsg.str());

      vtkmsg.rdbuf()->freeze(0);
    }
  }
}

int vtkOmniConnectLogCallback::EnableDebug = true;
int vtkOmniConnectLogCallback::EnableError = true;
OmniConnectLogCallback vtkOmniConnectLogCallback::Callback = &ConnectLogCallback;

void vtkOmniConnectLogCallback::SetConnectionLogLevel(int logLevel)
{
  OmniConnect::SetConnectionLogLevel(logLevel);
}
