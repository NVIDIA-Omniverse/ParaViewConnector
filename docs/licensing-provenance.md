# Licensing provenance review

These findings map the public ParaViewConnector audit at `775c6f6` to the corresponding files in this GitLab source repository. Creation-commit links below refer to the public repository's history.

The following files have no original copyright or SPDX header. Their creation commits are attributed to Kees Van Kooten, but an import commit alone does not establish the origin of every line. Confirm authorship or upstream provenance before adding ownership or license assertions. These files are intentionally unchanged by the header update.

| File | Creation commit | Review needed |
|---|---|---|
| [build-template.bat](../build-template.bat) | [9dbfefc](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/9dbfefc) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [build-template.sh](../build-template.sh) | [9dbfefc](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/9dbfefc) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/paraview.plugin](../Plugin/paraview.plugin) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Qt/BuildVersion.h](../Plugin/Qt/BuildVersion.h) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Qt/paraview.sub.plugin](../Plugin/Qt/paraview.sub.plugin) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Qt/Resources/omniComponents.qrc](../Plugin/Qt/Resources/omniComponents.qrc) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Qt/Resources/UI/pqOmniConnectAboutDialog.ui](../Plugin/Qt/Resources/UI/pqOmniConnectAboutDialog.ui) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Qt/Resources/UI/pqOmniConnectAccountDialog.ui](../Plugin/Qt/Resources/UI/pqOmniConnectAccountDialog.ui) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Qt/Resources/UI/pqOmniConnectFolderPickerDialog.ui](../Plugin/Qt/Resources/UI/pqOmniConnectFolderPickerDialog.ui) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Qt/Resources/UI/pqOmniConnectSettingsDialog.ui](../Plugin/Qt/Resources/UI/pqOmniConnectSettingsDialog.ui) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/Views/vtk.module](../Plugin/Views/vtk.module) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/vtkOmniverseConnector/vtk.module](../Plugin/vtkOmniverseConnector/vtk.module) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |
| [Plugin/vtkOmniverseConnector/Examples/connector_example.py](../Plugin/vtkOmniverseConnector/Examples/connector_example.py) | [ae6e9ac](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/ae6e9ac) | The introductory comment and rendering setup resemble the upstream VTK CylinderExample. Confirm source/revision and retained upstream terms before adding a header. |
| [Plugin/vtkOmniverseConnector/OmniConnect/usdhelpers.cmake](../Plugin/vtkOmniverseConnector/OmniConnect/usdhelpers.cmake) | [be74ba9](https://github.com/NVIDIA-Omniverse/ParaViewConnector/commit/be74ba9) | Confirm original connector configuration/code versus adapted upstream material, then add the appropriate header. |

Potential example reference: [VTK CylinderExample](https://examples.vtk.org/site/Python/GeometricObjects/CylinderExample/). Similarity is a provenance lead, not a confirmed derivation or license determination.

## OpenVDB build helpers

FindOpenVDB.cmake, OpenVDBUtils.cmake and FindTBB.cmake retain original OpenVDB copyright and MPL-2.0 headers. FindOpenVDB.cmake changed in commit 9dbfefc after its initial import; the others were introduced as source imports. Record precise upstream revisions and local changes, and confirm any additional attribution needed for modified portions. This update preserves all three files and their license declarations.

## External packages

The public tree carries package notices rather than full dependency sources or patch histories. The README maps included notices and direct build use. Reconcile exact versions, modifications, binary distribution terms and missing notices against the selected release artifacts, including FreeType if redistributed.
