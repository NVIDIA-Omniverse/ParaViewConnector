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

#ifndef OmniConnectDiagnosticMgrDelegate_h
#define OmniConnectDiagnosticMgrDelegate_h

#include "usd.h"

PXR_NAMESPACE_USING_DIRECTIVE

#include "OmniConnectData.h"

class OmniConnectDiagnosticMgrDelegate : public TfDiagnosticMgr::Delegate
{
  public:
    OmniConnectDiagnosticMgrDelegate(void* logUserData,
      OmniConnectLogCallback logCallback)
      : LogUserData(logUserData)
      , LogCallback(logCallback)
    {}

    void IssueError(TfError const& err) override
    {
      LogTfMessage(OmniConnectLogLevel::ERR, err);
    }

    virtual void IssueFatalError(TfCallContext const& context,
      std::string const& msg) override
    {
      std::string message = TfStringPrintf(
        "[USD Internal error]: %s in %s at line %zu of %s",
        msg.c_str(), context.GetFunction(), context.GetLine(), context.GetFile()
        );
      LogMessage(OmniConnectLogLevel::ERR, message);
    }

    virtual void IssueStatus(TfStatus const& status) override
    {
      LogTfMessage(OmniConnectLogLevel::STATUS, status);
    }

    virtual void IssueWarning(TfWarning const& warning) override
    {
      LogTfMessage(OmniConnectLogLevel::WARNING, warning);
    }

    static void SetOutputEnabled(bool enable) { OutputEnabled = enable; }

  protected:

    void LogTfMessage(OmniConnectLogLevel level, TfDiagnosticBase const& diagBase)
    {
      std::string message = TfStringPrintf(
        "[USD Internal Message]: %s with error code %s in %s at line %zu of %s",
        diagBase.GetCommentary().c_str(),
        TfDiagnosticMgr::GetCodeName(diagBase.GetDiagnosticCode()).c_str(),
        diagBase.GetContext().GetFunction(),
        diagBase.GetContext().GetLine(),
        diagBase.GetContext().GetFile()
      );

      LogMessage(level, message);
    }

    void LogMessage(OmniConnectLogLevel level, const std::string& message)
    {
      if(OutputEnabled)
        LogCallback(level, LogUserData, message.c_str());
    }

    void* LogUserData;
    OmniConnectLogCallback LogCallback;
    static bool OutputEnabled;
};

#endif