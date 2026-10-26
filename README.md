# ParaView Omniverse Connector

Source of the Omniverse Connector for ParaView.

This project can be used purely to output USD files to local disk without any further run- or buildtime dependencies, and does therefore not require the use of Omniverse if not desired. If Omniverse output is enabled, it can be configured to transfer USD data to an Omniverse service of choice.

## License

NVIDIA-attributed connector source is available under [MIT](LICENSE.txt).
Third-party components retain their own terms; see
[Third-Party_Notices.txt](Third-Party_Notices.txt) and the component map below.

## Contributing

This project is currently not accepting contributions.

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Components and dependency licenses

Builds consume externally installed SDKs and libraries. Versions and installed
binaries depend on the selected configuration. This map describes the source
and notices in this repository; it is not a bill of materials for every build.

### Source and direct dependencies

| Component | License and notice path | Use and modifications |
|---|---|---|
| Connector source in `Plugin/` | [MIT](LICENSE.txt) | ParaView/VTK integration, USD export and optional Omniverse connectivity. Upstream-attributed files retain their own terms. |
| ParaView | [ParaView license and notices](LICENSES/paraview-LICENSE.txt) | External host application and SDK. No ParaView source patch set is included. |
| VTK | VTK BSD-style terms in [LICENSES/paraview-LICENSE.txt](LICENSES/paraview-LICENSE.txt) | External rendering/pipeline API with connector-specific integration classes. Example provenance needs confirmation; see [review notes](docs/licensing-provenance.md). |
| Qt | [Qt License Agreement](LICENSES/qt-LICENSE.txt); [LGPL-3.0 text](LICENSES/LICENSE.LGPLv3) | External UI library. Apply the terms of the actual Qt distribution; both notices being present does not identify that choice. Qt source is not vendored. |
| OpenUSD | Modified Apache 2.0 and bundled notices: `usd-license.txt`, `usd.py312.windows-x86_64.stock.debug-LICENSE.txt`, `usd.py312.windows-x86_64.stock.release-LICENSE.txt` in [archive][license-archive] | External scene-description SDK. Local CMake adjusts imported include/link interfaces; no OpenUSD source patch set is included. |
| Python / CPython | PSF and historical notices: `python-LICENSE.txt`, `cpython-LICENSE.txt` in [archive][license-archive] | External development/link dependencies for USD and interpreter for the optional VTK example. No Python source is vendored. |
| Omniverse Client Library | Package terms and dependency notices: `omniverse client library-LICENSES.txt` and nested `omniverse client library-LICENSES.zip` in [archive][license-archive] | External Omniverse connectivity. The index is not a license grant; obtain the applicable library terms with the package. No client-library source is vendored. |
| Omniverse USD Resolver | NVIDIA agreements identified by `omni_usd_resolver-LICENSE.md` in [archive][license-archive] | External resolver for Omniverse paths. Local CMake adjusts imported targets; resolver source is not vendored. |
| OpenVDB | MPL-2.0: OpenVDB section in [Third-Party_Notices.txt](Third-Party_Notices.txt), `openvdb-LICENSE.txt` in [archive][license-archive] | Optional external volume-output library. Copied [FindOpenVDB](Plugin/vtkOmniverseConnector/OmniConnect/Volume/cmake/FindOpenVDB.cmake), [OpenVDBUtils](Plugin/vtkOmniverseConnector/OmniConnect/Volume/cmake/OpenVDBUtils.cmake) and [FindTBB](Plugin/vtkOmniverseConnector/OmniConnect/cmake/FindTBB.cmake) helpers retain upstream attribution. FindOpenVDB has local history changes; exact import revisions and patch scope need maintainer confirmation. |
| Boost | BSL-1.0: `boost-LICENSE.md`, `boost-LICENSE_1_0.txt` in [archive][license-archive] | External USD/OpenVDB dependency, including iostreams. Local CMake adjusts link settings; Boost source is not vendored. |
| TBB | Apache-2.0: `tbb-LICENSE.txt` in [archive][license-archive] | External USD/OpenVDB threading dependency. Located by the copied OpenVDB helper; TBB library source is not vendored. |
| Blosc | MIT: `c-blosc-LICENSE.txt` in [archive][license-archive] | Optional external OpenVDB compression dependency; no source is vendored. |
| zlib | zlib license: `zlib-LICENSE.txt` in [archive][license-archive] | Optional external OpenVDB compression dependency; no source is vendored. |
| Imath | BSD-3-Clause: `imath-LICENSE.md` in [archive][license-archive] | External math dependency located through USD; no source is vendored. |
| MaterialX | Apache-2.0: `materialx-LICENSE.txt` in [archive][license-archive] | External dependency located through USD; no source is vendored. |
| FreeType | [Upstream FTL / GPLv2 choices](https://freetype.org/license.html); notice from the selected package | External font dependency referenced in build instructions. No FreeType source or separate notice is included here; confirm terms and notices before redistributing the package. |

### Other package notices

[LICENSES/omniverse-paraview-connector-LICENSES.txt](LICENSES/omniverse-paraview-connector-LICENSES.txt)
indexes the [license archive][license-archive]. The additional entries below
are package notices, not proof that each component is linked or shipped.
Dependency source and downstream patch histories are not included here;
release maintainers must verify the selected package versions and modifications.

| Component and archive entry | License described by notice | Use evidenced here |
|---|---|---|
| Alembic — `alembic-LICENSE.txt` | BSD-style Alembic terms and Boost notices | Package notice only; no direct connector target identified. |
| Draco — `draco-LICENSE.txt` | Apache-2.0 | Package notice only; no direct connector target identified. |
| FastLZ — `fastlz-LICENSE.txt` | MIT | Compression-library notice; no direct connector target identified. |
| fmt — `fmt-LICENSE.txt` | MIT | Formatting-library notice; no direct connector target identified. |
| h5py — `h5py-LICENSE.txt` | BSD-3-Clause | Python/HDF5 notice; no direct connector target identified. |
| libffi — `libffi-LICENSE.txt` | MIT | Foreign-function-interface notice; no direct connector target identified. |
| libjpeg-turbo — `libjpeg-turbo-LICENSE.md` | IJG, BSD-3-Clause and zlib terms by component | Image-codec notice; no direct connector target identified. |
| libpng — `libpng-LICENSE.txt` | PNG Reference Library license terms | Image-codec notice; no direct connector target identified. |
| libtiff — `libtiff-LICENSE.md` | libtiff permissive license | Image-codec notice; no direct connector target identified. |
| LZ4 — `lz4-LICENSE.txt` | BSD-2-Clause | Compression-library notice; no direct connector target identified. |
| OpenEXR — `openexr-LICENSE.md` | BSD-3-Clause | Image-library notice; no direct connector target identified. |
| OpenImageIO — `openimageio-LICENSE.md` | Apache-2.0 | Image-library notice; no direct connector target identified. |
| OpenSSL — `openssl-LICENSE.txt` | Apache-2.0 | TLS/cryptography notice; no direct connector target identified. |
| OpenSubdiv — `opensubdiv-LICENSE.txt` | Modified Apache 2.0 terms | Subdivision-library notice; no direct connector target identified. |
| pybind11 — `pybind11-LICENSE.txt` | BSD-3-Clause | Python-binding notice; no direct connector target identified. |
| Snappy — `snappy-LICENSE.txt` | BSD-3-Clause | Compression-library notice; no direct connector target identified. |
| stdint — `stdint-LICENSE.txt` | BSD-style terms in the included notice | Compatibility-header notice; no direct connector target identified. |
| XZ Utils — `xz-LICENSE.txt`, `PACKAGE-LICENSE.txt` | 0BSD for liblzma; additional per-file terms in the notices | Compression notices; selected components determine applicable terms. |
| repo_nv — `repo_nv-LICENSE.txt` | NVIDIA proprietary terms | Build-tool notice; no direct connector runtime target identified. |
| Connector binary package — `omniverse-paraview-connector-LICENSE.txt` | NVIDIA proprietary package notice | Existing binary-package notice retained verbatim. It does not replace root MIT terms for public source; reconcile it with the actual binary distribution terms. |

Preserve third-party notices with redistributed source or binaries. Record
selected versions, original revisions and local patches in the release
inventory. [Provenance notes](docs/licensing-provenance.md) list the unresolved
file-origin questions.

[license-archive]: LICENSES/omniverse-paraview-connector-LICENSES.zip

## Prerequisites

On Windows and Linux this library builds against:
    - Kitware ParaView (corresponding to git tagged version of this repo)
    - USD version 25.xx, with (if enabled) OpenVDB 10 or 11.

USD can be built/installed in any of the following ways (depending on desired capabilities):
    - Get prebuilt USD and optional Omniverse packages according to [Downloading the Omniverse libraries](#downloading-the-omniverse-libraries)
    - Build USD from source (https://github.com/PixarAnimationStudios/USD/), optionally with OpenVDB support

Note that on Linux, GCC only guarantees forward ABI-compatibility, so libraries downloaded from external sources built with newer versions of GCC than the ParaView Connector may not link to it properly.

## Building the ParaView Connector - Simple Example

For a minimal build with USD support only (on Linux):

```bash
# Configure with CMake
cmake -S /path/to/source -B /path/to/build \
  -D ParaView_DIR=/path/to/paraview/lib/cmake/paraview-<pv_version> \
  -D USD_ROOT_DIR=/path/to/usd \
  -D OMNICONNECT_USE_OMNIVERSE=OFF \
  -D OMNICONNECT_USE_OPENVDB=OFF \
  -D Qt6_DIR=/path/to/qt/lib/cmake/Qt6 \
  -D Python3_EXECUTABLE=/path/to/python3/bin/python3 \
  -D Python3_INCLUDE_DIR=/path/to/python3/include \
  -D Python3_LIBRARY=/path/to/python3/lib/libpython3.x.so \
  -D CMAKE_PREFIX_PATH=/path/to/freetype \
  -D FREETYPE_LIBRARY=/path/to/freetype/lib/libfreetype.so \
  -D FREETYPE_INCLUDE_DIRS=/path/to/freetype/include \
  -D FREETYPE_INCLUDE_DIR_ft2build=/path/to/freetype/include \
  -D FREETYPE_INCLUDE_DIR_freetype2=/path/to/freetype/include \
  -D INSTALL_USD_DEPS=ON

# Build
cmake --build /path/to/build --parallel

# Install
cmake --install /path/to/build --prefix /path/to/install
```

Replace the paths (`/path/to/paraview`, `/path/to/usd`, `/path/to/qt`, `/path/to/python3`, `/path/to/freetype`) with your actual dependency installation paths, and (`/path/to/source`, `/path/to/build`, `/path/to/install`) with your project paths.

Template build scripts for this simple example are included under `build-template.bat/sh`.

## Building the ParaView Connector - Full Details

Build and install a version of ParaView first, then invoke cmake on this project with the following arguments:

    -G <GENERATOR>
    -D ParaView_DIR=<paraview_installation>/lib/cmake/paraview-<pv_version>
    -D Qt6_DIR=<qt_installation>/lib/cmake/Qt6
    -D Python3_EXECUTABLE=<python_executable>
    -D Python3_INCLUDE_DIR=<python_include_dir>
    -D Python3_LIBRARY=<python_library>
    -D CMAKE_PREFIX_PATH=<freetype_installation>
    -D FREETYPE_LIBRARY=<freetype_library>
    -D FREETYPE_INCLUDE_DIRS=<freetype_installation>/include
    -D FREETYPE_INCLUDE_DIR_ft2build=<freetype_installation>/include
    -D FREETYPE_INCLUDE_DIR_freetype2=<freetype_installation>/include
    -D USD_ROOT_DIR=<usd_dir>

Where `<python_library>` and `<freetype_library>` is the full path to either `python<version>.lib`/`freetype.lib` or `libpython<version>.so`/`libfreetype.so`, depending on platform.

Choose whether you want to build the Omniverse Connector with Omniverse support (`OFF` means USD-only), and/or with OpenVDB support and whether OpenVDB has already been built as part of the USD source tree:

    -D OMNICONNECT_USE_OMNIVERSE=ON/OFF
    -D OMNICONNECT_USE_OPENVDB=ON/OFF
    -D USE_USD_OPENVDB_BUILD=ON/OFF

Depending on which components have been turned on or off (see below), this will trigger a number of dependency searches that can be resolved using:

    -D OMNICLIENT_ROOT_DIR=<omniclient_dir>
    -D OMNIUSDRESOLVER_ROOT_DIR=<usdresolver_dir>
    -D BOOST_ROOT=<boost_dir>
    -D OpenVDB_ROOT=<openvdb_dir>
    -D Blosc_ROOT=<blosc_dir>
    -D BLOSC_LIBRARYDIR=<blosc_lib_dir>
    -D ZLIB_ROOT=<zlib_dir>
    -D ZLIB_LIBRARY=<zlib_lib_file>
    -D TBB_ROOT=<tbb_dir>

where the Blosc and Zlib installations are typically pointing to the OpenVDB directory, assuming those exist as part of the OpenVDB installation. Boost refers to the dependency of OpenVDB, not USD.

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