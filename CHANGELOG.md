## [201.3.0] - 2026-10-21
* Adds a modern Omniverse Live-Sync workflow, where a user can create a Live session in ParaView, and other Omniverse clients supporting Live Sync can join and see updates to the scene in real time.
* Upgrade the USD resolver to the pinned stable 3.0.0 release for stock USD 25.11 and Python 3.12.
* Package the live plugin's Carbonite runtime explicitly so viewport creation works without a separately installed SDK.
* Replace the Linux .NET installer with a script, removing bundled .NET and OpenSSL runtimes while preserving installation and uninstallation commands.
* Upgrade the host and packaged Python dependencies to the stable 3.12.16 NVIDIA packages, retaining the Python 3.12 ABI used by USD.

## [201.2.0] - 2025-10-03
* Switch to PV 6.0 (only works with versions >= 6.0.1)

## [201.1.0] - 2025-03-11
* Fixed cell array mapping (eg. colors) for polydata with multiple prim types
* Removed error message when Launcher is not installed
* Upgrade to USD 24.05

## [201.0.2] - 2024-11-05
* Fixed issue with shape indices for timesampled glyph

## [201.0.1] - 2024-08-30
* Fixed glyph output for custom pipeline shapes when using UsdGeomPoints for point representations

## [201.0.0] - 2023-12-20
* Switch to PV 5.12
* Support for renaming of USD prims by editing actor names in the pipeline view
* Fix for recognizing timestepped actors in animation cues loaded from state files
* Added an exporter to locally save to a USD file from any type of view using `File -> Export Scene`
* Added support for custom pipeline shapes for the glyph representation

## [200.1.0] - 2023-06-20
* Upgrade to USD 22.11
* Upgrade to OpenVDB 10
* Glyph mapper representation output support for sphere, box, cone, cylinder and arrow shap 
* Animation cues in the animation view now result in timestepped output for affected actors
* Connector UI Nucleus server queries (folders/users/connection state) now initiated from PV server instead of client
* Vertex color fix for colorspace mismatch with textures
* Fix for opacity mapped translucent materials using vertex, texture, or constant opacities
* Connector plugin libraries can be loaded from arbitrary location

## [200.0.0] - 2023-02-20

* Switch to PV 5.11
* Live functionality using Omniverse Client library 2.0
* A global timescale can now be applied per viewport, for scaling all time samples of timeseries data
* Omniverse Pass Arrays filter now works on volumes: adds any selected field array to the OpenVDB file as a separate grid
* Omniverse Pass Arrays filter now converts field arrays of double type to float by default
* Exporting 3D widgets from the PV viewport is now an explicit option
* Various bugs in the UI and installer have been fixed

## [101.2.0] - 2022-10-27

* Switch to PV 5.10
* The `Pass Arrays` filter provides a checkbox to convert arrays of type `double(2/3/4)` to `float(2/3/4)`

## [101.1.0] - 2022-10-19

* Fixed bugs related to wrong per-primitive color output and missing temporal arrays filter output

## [101.0.0] - 2022-08-04

* Revamped UI with new Omniverse toolbar, including sign-in, settings, and open-in-Omniverse-app buttons
* Support for lines and (piecewise linear) curves
* Support for volume output as textured mesh for compatibility with Create
* Choice of different representations for points, lines/curves, wireframe and volumes in settings menu
* Choice of scene up-axis, text/binary output
* Optional output of time-varying textures and material parameters
* Option to force winding order for triangle output
* Any Omniverse View can now be opened into Omniverse application of choice via the click of a button
* Log verbosity selection for Omniverse Connector and Nucleus separately
* Support for continuation of output within an existing scene (session directory) after closing an Omniverse Connector View and creating a new one
* Update of Omniclient library version

## [100.0.1] - 2022-05-04

* Update of Omniclient library version

## [100.0.0] - 2021-04-12

* ParaView 5.9.0 ([repo](https://gitlab-master.nvidia.com/HPCVis/ParaView/-/tree/v5.9.0))
* Client-server setups
* Headless (EGL) environments
* Multi-node (MPI) environments
* Python environments
* Output to an Omniverse Nucleus server
* Convert to USD for local output
* Support for multiple connections/outputs at once
* Support for color-mapped surface meshes
* Animated timesteps
* Point datasets
* Slice representations
* Structured volume representations
* Transfer of any point and/or cell arrays
* Manual specification of uniform arrays over time
