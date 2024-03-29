# libprinter_settings
Library to interface with CuraEngine.  Automatically calculates dependant settings.

# Purpose

CuraEngine is an excellent slicer for fused deposition modeling (FDM) 3D printers.  It can be used with the Cura GUI, but that GUI does not suit everyone.  Technically CuraEngine can be ran standalone, but it has a daunting 400+ settings.  A config file exists that documents the interdependancies of these settings. Many settings can be calculated from other settings and the config files contain the equations for these calculations.  For example, CuraEngine has settings for speed_infill and speed_wall, but both can be set from the more general speed_print setting.  This library exists to automatically perform the required calculations and slice your stl model.

# Dependancies

There are no compile time dependancies, other than a functional C compiler.  The runtime dependancies are:
1) Ultimaker/CuraEngine (available on github)
2) the resources/definitions and resources/extruders directories from Ultimaker/Cura (available on github).  These are the defintion files that contain the equations mentioned above.  They should be version matched with CuraEngine.

Installing Cura meets both dependancies.

# System requirements

Should run on any POSIX compliant operating system.  Basic windows support.  Windows support was tested with wine.  Should also work under cygwin.

# Example code

See test/printer_settings_test.c for a very rough example of how to use the library.   The general idea is
1) Call PS_GetDefaultSearch() to get the default search path.  For posix operating systems the paths are /usr/share/cura/resources/definitions and /usr/share/cura/resources/extruders.  For windows, it is generally something like C:\Program Files\Ultimaker Cura %VERSION%\share\cura\resources\definitions and C:\Program Files\Ultimaker Cura %VERSION%\share\cura\resources\extruders.
2) Call PS_New() to generate a printer settings.  The two arguments are the name of .def.json file for the specific printer used and a search path. Usually, the search path from step 1 is used.
3) Build your settings with PS_BlankSettings(), PS_AddSetting() and PS_MergeSettings().  Generally only about a dozen settings for your quality level and material are required.
4) Call either PS_SliceFile() or PS_SliceStr() to slice your model, depending if the model is an .stl file saved to disk or a string containin the stl data.  The result is returned as a struct ps_ostream_t.  Usually this will be created PS_NewStrOStream() and then the gcode can be extracted with PS_OStreamContents().
