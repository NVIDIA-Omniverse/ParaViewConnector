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

#include "OmniConnectUtilsExternal.h"

#include <cstring>

namespace ocutils
{
  const char underScore = '_';
  auto isLetter = [](unsigned c) { return ((c - 'A') < 26) || ((c - 'a') < 26); };
  auto isNumber = [](unsigned c) { return (c - '0') < 10; };
  auto isUnder = [](unsigned c) { return c == underScore; };

  void FormatUsdName(char* name)
  {
    if(STRNLEN_PORTABLE(name, MAX_USER_STRLEN) == 0)
      return;

    unsigned x = *name;
    if (!isLetter(x) && !isUnder(x)) { *name = underScore; }
    x = *(++name);
    while (x != '\0') 
    {
      if(!isLetter(x) && !isNumber(x) && !isUnder(x))
        *name = underScore;
      x = *(++name);
    };
  }
  
  bool UsdNamesEqual(const char* name0, const char* name1, size_t len)
  {
    size_t name0Len = STRNLEN_PORTABLE(name0, MAX_USER_STRLEN);
    size_t name1Len = STRNLEN_PORTABLE(name1, MAX_USER_STRLEN);
    if(len != 0)
    {
      name0Len = (name0Len > len) ? len : name0Len;
      name1Len = (name1Len > len) ? len : name1Len;
    }

    if(name0Len != name1Len)
      return false;

    // Compare the first character
    if(name0Len > 0)
    {
      const char& nm0 = name0[0];
      const char& nm1 = name1[0];
      char char0 = (!isLetter(nm0) && !isUnder(nm0)) ? underScore : nm0;
      char char1 = (!isLetter(nm1) && !isUnder(nm1)) ? underScore : nm1;
      if(char0 != char1)
        return false;
    }

    // Compare the rest
    for(size_t pos = 1; pos < name0Len; ++pos)
    {
      const char& nm0 = name0[pos];
      const char& nm1 = name1[pos];
      char char0 = (!isLetter(nm0) && !isNumber(nm0) && !isUnder(nm0)) ? underScore : nm0;
      char char1 = (!isLetter(nm1) && !isNumber(nm1) && !isUnder(nm1)) ? underScore : nm1;
      if(char0 != char1)
        return false;
    }
    return true;
  }
}