# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.5

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
CMAKE_SOURCE_DIR = /home/daniele/tesla/tesla-static-analysis

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/daniele/tesla/tesla-static-analysis

# Include any dependencies generated for this target.
include libtesla/test/CMakeFiles/match.cpp.test.dir/depend.make

# Include the progress variables for this target.
include libtesla/test/CMakeFiles/match.cpp.test.dir/progress.make

# Include the compile flags for this target's objects.
include libtesla/test/CMakeFiles/match.cpp.test.dir/flags.make

libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o: libtesla/test/CMakeFiles/match.cpp.test.dir/flags.make
libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o: libtesla/test/match.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/daniele/tesla/tesla-static-analysis/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o"
	cd /home/daniele/tesla/tesla-static-analysis/libtesla/test && /usr/bin/newclang++   $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/match.cpp.test.dir/match.cpp.o -c /home/daniele/tesla/tesla-static-analysis/libtesla/test/match.cpp

libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/match.cpp.test.dir/match.cpp.i"
	cd /home/daniele/tesla/tesla-static-analysis/libtesla/test && /usr/bin/newclang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/daniele/tesla/tesla-static-analysis/libtesla/test/match.cpp > CMakeFiles/match.cpp.test.dir/match.cpp.i

libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/match.cpp.test.dir/match.cpp.s"
	cd /home/daniele/tesla/tesla-static-analysis/libtesla/test && /usr/bin/newclang++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/daniele/tesla/tesla-static-analysis/libtesla/test/match.cpp -o CMakeFiles/match.cpp.test.dir/match.cpp.s

libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.requires:

.PHONY : libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.requires

libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.provides: libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.requires
	$(MAKE) -f libtesla/test/CMakeFiles/match.cpp.test.dir/build.make libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.provides.build
.PHONY : libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.provides

libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.provides.build: libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o


# Object files for target match.cpp.test
match_cpp_test_OBJECTS = \
"CMakeFiles/match.cpp.test.dir/match.cpp.o"

# External object files for target match.cpp.test
match_cpp_test_EXTERNAL_OBJECTS =

libtesla/test/match.cpp.test: libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o
libtesla/test/match.cpp.test: libtesla/test/CMakeFiles/match.cpp.test.dir/build.make
libtesla/test/match.cpp.test: libtesla/src/libtesla.so
libtesla/test/match.cpp.test: libtesla/test/CMakeFiles/match.cpp.test.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/daniele/tesla/tesla-static-analysis/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable match.cpp.test"
	cd /home/daniele/tesla/tesla-static-analysis/libtesla/test && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/match.cpp.test.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
libtesla/test/CMakeFiles/match.cpp.test.dir/build: libtesla/test/match.cpp.test

.PHONY : libtesla/test/CMakeFiles/match.cpp.test.dir/build

libtesla/test/CMakeFiles/match.cpp.test.dir/requires: libtesla/test/CMakeFiles/match.cpp.test.dir/match.cpp.o.requires

.PHONY : libtesla/test/CMakeFiles/match.cpp.test.dir/requires

libtesla/test/CMakeFiles/match.cpp.test.dir/clean:
	cd /home/daniele/tesla/tesla-static-analysis/libtesla/test && $(CMAKE_COMMAND) -P CMakeFiles/match.cpp.test.dir/cmake_clean.cmake
.PHONY : libtesla/test/CMakeFiles/match.cpp.test.dir/clean

libtesla/test/CMakeFiles/match.cpp.test.dir/depend:
	cd /home/daniele/tesla/tesla-static-analysis && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/daniele/tesla/tesla-static-analysis /home/daniele/tesla/tesla-static-analysis/libtesla/test /home/daniele/tesla/tesla-static-analysis /home/daniele/tesla/tesla-static-analysis/libtesla/test /home/daniele/tesla/tesla-static-analysis/libtesla/test/CMakeFiles/match.cpp.test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : libtesla/test/CMakeFiles/match.cpp.test.dir/depend

