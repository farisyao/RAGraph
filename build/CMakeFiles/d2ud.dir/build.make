# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.10

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/yaof/geograph/datu

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/yaof/geograph/datu/build

# Include any dependencies generated for this target.
include CMakeFiles/d2ud.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/d2ud.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/d2ud.dir/flags.make

CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o: CMakeFiles/d2ud.dir/flags.make
CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o: ../examples/analytical_apps/flags.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/yaof/geograph/datu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o -c /home/yaof/geograph/datu/examples/analytical_apps/flags.cc

CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/yaof/geograph/datu/examples/analytical_apps/flags.cc > CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.i

CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/yaof/geograph/datu/examples/analytical_apps/flags.cc -o CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.s

CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.requires:

.PHONY : CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.requires

CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.provides: CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.requires
	$(MAKE) -f CMakeFiles/d2ud.dir/build.make CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.provides.build
.PHONY : CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.provides

CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.provides.build: CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o


CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o: CMakeFiles/d2ud.dir/flags.make
CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o: ../examples/analytical_apps/d2ud.cc
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/yaof/geograph/datu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o -c /home/yaof/geograph/datu/examples/analytical_apps/d2ud.cc

CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/yaof/geograph/datu/examples/analytical_apps/d2ud.cc > CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.i

CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/yaof/geograph/datu/examples/analytical_apps/d2ud.cc -o CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.s

CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.requires:

.PHONY : CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.requires

CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.provides: CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.requires
	$(MAKE) -f CMakeFiles/d2ud.dir/build.make CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.provides.build
.PHONY : CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.provides

CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.provides.build: CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o


# Object files for target d2ud
d2ud_OBJECTS = \
"CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o" \
"CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o"

# External object files for target d2ud
d2ud_EXTERNAL_OBJECTS =

d2ud: CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o
d2ud: CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o
d2ud: CMakeFiles/d2ud.dir/build.make
d2ud: libgrape-lite.so
d2ud: /usr/local/openmpi/lib/libmpi.so
d2ud: /usr/local/lib/libglog.so
d2ud: /usr/local/lib/libgflags.so
d2ud: /usr/local/lib/libglog.so
d2ud: CMakeFiles/d2ud.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/yaof/geograph/datu/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable d2ud"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/d2ud.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/d2ud.dir/build: d2ud

.PHONY : CMakeFiles/d2ud.dir/build

CMakeFiles/d2ud.dir/requires: CMakeFiles/d2ud.dir/examples/analytical_apps/flags.cc.o.requires
CMakeFiles/d2ud.dir/requires: CMakeFiles/d2ud.dir/examples/analytical_apps/d2ud.cc.o.requires

.PHONY : CMakeFiles/d2ud.dir/requires

CMakeFiles/d2ud.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/d2ud.dir/cmake_clean.cmake
.PHONY : CMakeFiles/d2ud.dir/clean

CMakeFiles/d2ud.dir/depend:
	cd /home/yaof/geograph/datu/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/yaof/geograph/datu /home/yaof/geograph/datu /home/yaof/geograph/datu/build /home/yaof/geograph/datu/build /home/yaof/geograph/datu/build/CMakeFiles/d2ud.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/d2ud.dir/depend
