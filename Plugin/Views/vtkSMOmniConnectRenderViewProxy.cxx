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

#include "vtkSMOmniConnectRenderViewProxy.h"

#include "vtkObjectFactory.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"

vtkStandardNewMacro(vtkSMOmniConnectRenderViewProxy);

vtkSMOmniConnectRenderViewProxy::vtkSMOmniConnectRenderViewProxy()
{
}

vtkSMOmniConnectRenderViewProxy::~vtkSMOmniConnectRenderViewProxy()
{
}

void vtkSMOmniConnectRenderViewProxy::Update()
{
  // If we have a 3D glyph representation with custom pipeline connection,
  // make sure the input proxy has been made visible at least once.
  vtkSMPVRepresentationProxy* repr = nullptr;
  vtkSMPropertyHelper helper(this, "Representations");

  for (unsigned int i = 0; i < helper.GetNumberOfElements(); i++)
  {
    vtkSMPVRepresentationProxy* repr =
      vtkSMPVRepresentationProxy::SafeDownCast(helper.GetAsProxy(i));

    if (repr && repr->GetProperty("Representation"))
    {
      const char* repStr = vtkSMPropertyHelper(repr, "Representation").GetAsString();
      if(strcmp(repStr, "3D Glyphs") == 0)
      {
        if (repr->GetProperty("GlyphType"))
        {
          vtkSMProxy* glyphProxy = vtkSMPropertyHelper(repr, "GlyphType").GetAsProxy();
          if(glyphProxy->GetProperty("Input"))
          {
            vtkSMSourceProxy* glyphProxyIn = vtkSMSourceProxy::SafeDownCast(vtkSMPropertyHelper(glyphProxy, "Input").GetAsProxy());

            // The glyph proxy having been made visible once is equivalent to it having a representation proxy.
            // If no representation exists, make sure that one is created.
            if(glyphProxyIn && !this->FindRepresentation(glyphProxyIn, 0))
            {
              vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
              controller->Show(glyphProxyIn, 0, this);
            }
          }
        }
      }
    }
  }

  // Call the parent class's Update function
  this->Superclass::Update();
}