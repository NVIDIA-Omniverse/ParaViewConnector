# ParaView Omniverse Connector

Source of the Omniverse Connector for ParaView

## License

See LICENSE.txt and Third-Party_Notices.txt for all applicable licenses.

## Getting started

Build and install a version of ParaView first, then invoke cmake on this project with the following arguments:

    -G <GENERATOR>
    -D ParaView_DIR=<paraview_installation>/lib/cmake/paraview-<pv_version>
    -D Qt5_DIR=<qt_installation>/lib/cmake/Qt5 
    -D OMNICONNECT_USE_OMNIVERSE=ON/OFF
    -D OMNICONNECT_USE_OPENVDB=ON/OFF
    -D USE_USD_OPENVDB_BUILD=ON/OFF
    -D USD_ROOT_DIR:PATH=<usd_dir>
    -D OMNICLIENT_ROOT_DIR:PATH=<omniclient_dir>
    -D OMNIUSDRESOLVER_ROOT_DIR:PATH=<usdresolver_dir>
    -D Python_ROOT_DIR:PATH=<python-for-usd_dir>
    -D Python_FIND_STRATEGY=LOCATION
    -D OpenVDB_ROOT:PATH=<openvdb_dir>
    -D MaterialX_DIR=<materialx_dir>/lib/cmake/MaterialX
    -D Blosc_ROOT:PATH=<blosc_dir>
    -D ZLIB_ROOT:PATH=<zlib_dir>
    -D ZLIB_LIBRARY=<zlib_lib_file>
    -D TBB_ROOT=<tbb_dir>
    -D BOOST_ROOT=<boost_dir>
    -D OPENVDB_USE_STATIC_LIBS=ON
    -D BLOSC_USE_STATIC_LIBS=ON
    -D ZLIB_USE_STATIC_LIBS=ON
    -D TBB_USE_STATIC_LIBS=OFF
    -D Boost_USE_STATIC_LIBS=ON
    -D INSTALL_USD_DEPS=ON
    -D INSTALL_OMNIVERSE_DEPS=ON

where the Blosc and Zlib installations are typically pointing to the OpenVDB directory, assuming those exist as part of the OpenVDB installation.
In case of `USE_USD_OPENVDB_BUILD=ON`, all of OpenVDB, Blosc and Zlib dependencies can be left out; they will instead be assumed to exist as part of the USD installation.
In case of `OMNICONNECT_USE_OPENVDB=OFF`, the OpenVDB, Blosc and Zlib dependencies can be left out, and OpenVDB support will not be available. 
In case of `OMNICONNECT_USE_OMNIVERSE=OFF`, the client library dependency can be left out, and Omniverse support will not be available.

After the cmake configuration has finished, build and install the generated project files as desired.

Lastly, the installed connector plugin and all of its aforementioned dependency binaries (Python, Qt, Usd, Omniclient, OpenVDB) have to be copied to the the ParaView installation, or made available via the `PATH` environment variable before loading the plugin from within the ParaView UI.