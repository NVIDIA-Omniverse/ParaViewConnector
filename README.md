# ParaView Omniverse Connector

Source of the Omniverse Connector for ParaView.

This project can be used purely to output USD files to local disk without any further run- or buildtime dependencies, and does therefore not require the use of Omniverse if not desired. If Omniverse output is enabled, it can be configured to transfer USD data to an Omniverse service of choice.

## License

See LICENSE.txt and Third-Party_Notices.txt for all applicable licenses.

## Prerequisites

On Windows and Linux this library builds against:
    - Kitware ParaView (corresponding to git tagged version of this repo)
    - USD version 23.xx or 24.xx, with (if enabled) OpenVDB 10 or 11.

USD can be built/installed in any of the following ways (depending on desired capabilities):
    - Get prebuilt USD and optional Omniverse packages according to [Downloading the Omniverse libraries](#downloading-the-omniverse-libraries)
    - Build USD from source (https://github.com/PixarAnimationStudios/USD/), optionally with OpenVDB support

Note that on Linux, GCC only guarantees forward ABI-compatibility, so libraries downloaded from external sources built with newer versions of GCC than the ParaView Connector may not link to it properly.

## Building the ParaView Connector

Build and install a version of ParaView first, then invoke cmake on this project with the following arguments:

    -G <GENERATOR>
    -D ParaView_DIR=<paraview_installation>/lib/cmake/paraview-<pv_version>
    -D Qt5_DIR=<qt_installation>/lib/cmake/Qt5
    -D CMAKE_PREFIX_PATH=<freetype_installation>
    -D FREETYPE_LIBRARY=<freetype_library>

Where `<freetype_library>` is the full path to either `freetype.lib` or `libfreetype.so`, depending on platform.

Choose whether you want to build the Omniverse Connector with Omniverse support (`OFF` means USD-only), and/or with OpenVDB support and whether OpenVDB has already been built as part of the USD source tree:

    -D OMNICONNECT_USE_OMNIVERSE=ON/OFF
    -D OMNICONNECT_USE_OPENVDB=ON/OFF
    -D USE_USD_OPENVDB_BUILD=ON/OFF

Depending on which components have been turned on or off (see below), this will trigger a number of dependency searches that can be resolved using:

    -D USD_ROOT_DIR=<usd_dir>
    -D OMNICLIENT_ROOT_DIR=<omniclient_dir>
    -D OMNIUSDRESOLVER_ROOT_DIR=<usdresolver_dir>
    -D Python3_ROOT_DIR=<python-for-usd_dir>
    -D Python3_FIND_STRATEGY=LOCATION
    -D OpenVDB_ROOT=<openvdb_dir>
    -D Blosc_ROOT=<blosc_dir>
    -D BLOSC_LIBRARYDIR=<blosc_lib_dir>
    -D ZLIB_ROOT=<zlib_dir>
    -D ZLIB_LIBRARY=<zlib_lib_file>
    -D TBB_ROOT=<tbb_dir>
    -D BOOST_ROOT=<boost_dir>

where the Blosc and Zlib installations are typically pointing to the OpenVDB directory, assuming those exist as part of the OpenVDB installation. 

In case of `USE_USD_OPENVDB_BUILD=ON`, all of OpenVDB, Blosc and Zlib dependencies can be left out; they will instead be assumed to exist as part of the USD installation.
In case of `OMNICONNECT_USE_OPENVDB=OFF`, the OpenVDB, Blosc and Zlib dependencies can be left out, and OpenVDB support will not be available. 
In case of `OMNICONNECT_USE_OMNIVERSE=OFF`, the client library and omni resolver dependency can be left out, and Omniverse support will not be available.

If `OMNICONNECT_USE_OPENVDB=ON`, the following dependencies are best linked statically, to not cause potential clashes with dynamic libraries included with ParaView:

    -D OPENVDB_USE_STATIC_LIBS=ON
    -D BLOSC_USE_STATIC_LIBS=ON
    -D ZLIB_USE_STATIC_LIBS=ON
    -D Boost_USE_STATIC_LIBS=ON

It is also recommended to install the USD and Omniverse dependencies along with the plugin binaries, to avoid runtime link issues:

    -D INSTALL_USD_DEPS=ON
    -D INSTALL_OMNIVERSE_DEPS=ON

After the cmake configuration has finished, build and install the generated project files as desired.

Lastly, the installed connector plugin and all of its aforementioned dependency binaries (Python, Qt, Usd, Omniclient/resolver, OpenVDB) have to be copied to the the ParaView installation, or made available via the `PATH` environment variable before loading the plugin from within the ParaView UI.

## VTK-only usage

It is possible to build only the VTK part of the PV Connector's code along with the VTK source,
which can be set up and used completely within Python:

- Copy the subdirectory `vtkOmniverseConnector` into the VTK source tree, underneath `VTK/Rendering`
- In `vtkOmniverseConnector/vtk.module`, change the`OmniverseConnector::vtkOmniverseConnector` identifier to the VTK namespace; `VTK::vtkOmniverseConnector`
- Set `VTK_MODULE_ENABLE_VTK_vtkOmniverseConnector=YES` during the Cmake configuration step
- Set `VTK_WRAP_PYTHON=ON` for the Python wrapping
- Now continue setting the variables from [Building the ParaView Connector](#building-the-paraview-connector) as desired, starting at `OMNICONNECT_USE_OMNIVERSE` and `OMNICONNECT_USE_OPENVDB` to indicate which dependencies to include.
- After building VTK with the connector, an example VTK Python program is included in `vtkOmniverseConnector/Examples`, which can be run as follows:
    - Remember to set `PYTHONPATH` to `lib/python3.10/site-packages` on Linux, `lib/site-packages` on Windows, and on Linux set `LD_LIBRARY_PATH` to the `lib` dir as well.
    - Simply run the script with `python(3) connector_example.py <usd_output_dir>`

## Downloading the Omniverse libraries

- Clone the Connect Sample on Github: https://github.com/NVIDIA-Omniverse/connect-samples
- Make sure you check out a tag using a supported USD version for this project: inspect the `deps/target-deps.packman.xml` file, and make sure that the `omni_connect_sdk` entry contains a major version of the `pxr-<version>` substring corresponding to what is listed at the top of this `README.md`.
- Build the connect sample by running `repo.sh/.bat build` in its folder
- Locate the `usd`, `omni_usd_resolver`, `omni_client_library` and `python` folders in the `_build/target-deps` subfolder
- The location of the previous four subfolders respectively can directly be set as `USD_ROOT_DIR`, `OMNIUSDRESOLVER_ROOT_DIR`, `OMNICLIENT_ROOT_DIR`, `Python3_ROOT_DIR` in the ANARI CMake configuration, as demonstrated in [Building the ParaView Connector](#building-the-paraview-connector).