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

# Utility rule file for tesla-test.

# Include the progress variables for this target.
include tesla/test/CMakeFiles/tesla-test.dir/progress.make

tesla/test/CMakeFiles/tesla-test:
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold --progress-dir=/home/daniele/tesla/tesla-static-analysis/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Testing TESLA"
	cd /home/daniele/tesla/tesla-static-analysis/tesla/test && /home/daniele/llvm/build/bin/llvm-lit -qv /home/daniele/tesla/tesla-static-analysis/tesla/test --param=build_dir=/home/daniele/tesla/tesla-static-analysis --param=source_dir=/home/daniele/tesla/tesla-static-analysis --param=output_dir=/home/daniele/tesla/tesla-static-analysis --param=extra_cxxflags=-std=c++14\ -pthread\ -ltinfo\ -fPIC\ -fvisibility-inlines-hidden\ -std=c++11\ -w\ -ffunction-sections\ -fdata-sections\ -Wall\ -fvisibility=hidden\ -D\ GOOGLE_PROTOBUF_NO_RTTI\ -std=c++14 --param=extra_cflags=-resource-dir:/home/daniele/llvm/build/lib/clang/6.0.0

tesla-test: tesla/test/CMakeFiles/tesla-test
tesla-test: tesla/test/CMakeFiles/tesla-test.dir/build.make

.PHONY : tesla-test

# Rule to build all files generated by this target.
tesla/test/CMakeFiles/tesla-test.dir/build: tesla-test

.PHONY : tesla/test/CMakeFiles/tesla-test.dir/build

tesla/test/CMakeFiles/tesla-test.dir/clean:
	cd /home/daniele/tesla/tesla-static-analysis/tesla/test && $(CMAKE_COMMAND) -P CMakeFiles/tesla-test.dir/cmake_clean.cmake
.PHONY : tesla/test/CMakeFiles/tesla-test.dir/clean

tesla/test/CMakeFiles/tesla-test.dir/depend:
	cd /home/daniele/tesla/tesla-static-analysis && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/daniele/tesla/tesla-static-analysis /home/daniele/tesla/tesla-static-analysis/tesla/test /home/daniele/tesla/tesla-static-analysis /home/daniele/tesla/tesla-static-analysis/tesla/test /home/daniele/tesla/tesla-static-analysis/tesla/test/CMakeFiles/tesla-test.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tesla/test/CMakeFiles/tesla-test.dir/depend

