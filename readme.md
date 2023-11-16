# allolib_mv

I saw a pretty cool music visualizer on [YouTube](https://www.youtube.com/watch?v=SmwKw3aXHzQ) so I wanted to run it for myself. The original music
visualizer is [Mitchell Lewis's](https://github.com/Mitchell57/MAT201B-mblewis).

## Windows with WSL2/WSLg

I am running it on Windows using WSL2/WSLg. After following standard prerequisites for allolib, here are some notable things I've done to get it to work.

### WSLg PulseAudio alsa
This is for the sound to output correctly when developing. WSLg supports pulseaudio out of the box. To get it to work with ALSA install `libasound2` and then add
```
pcm.default pulse
ctl.default pulse
```
to the `~/.asoundrc`. [Source](https://github.com/microsoft/wslg/issues/864)

### Graphics Card mesa-utils
To display graphics correctly I had to install `mesa-utils` and select my NVIDIA GPU `export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA`. [Source](https://github.com/microsoft/wslg/wiki/GPU-selection-in-WSLg)

## Running it
```
./run.sh mblewis/final_project/seeing-sound.cpp
```

# Allolib playground
[![Build Status](https://travis-ci.org/AlloSphere-Research-Group/allolib_playground.svg?branch=master)](https://travis-ci.org/AlloSphere-Research-Group/allolib_playground)

This repo provides a quick and simple way to build allolib applications. It also contains a set of tutorials and cookbook examples in addition to the examples in the allolib library.

## Setup

Get all dependencies and tools required by allolib. See the [readme file for allolib](https://github.com/AlloSphere-Research-Group/allolib/blob/master/readme.md).

Run the init.sh script to get the allolib and al_ext libraries or execute in
a bash shell:

    git submodule update --recursive --init

## Building and running applications

The allolib playground provides basic facilties for building and debugging 
single file applications. On a bash shell on Windows, Linux and OS X do:

    ./run.sh path/to/file.cpp

This will build allolib, and create an executable for the file.cpp called 'file' inside the '''path/to/bin''' directory. It will then run the application.

You can add a file called '''flags.cmake''' in the '''path/to/''' directory which will be added to the build scripts. Here you can add dependencies, include directories, linking and anything else that cmake could be used for. See the example in '''examples/user_flags'''.

For more complex projects follow the template provided in allotemplate
[https://github.com/AlloSphere-Research-Group/allotemplate](). This requires 
some knowledge of Cmake but allows more complex workflows and multifile
applications or multiple target binaries.

## Allolib API documentation

You can find the allolib API doxygen documentation at:
[https://allosphere-research-group.github.io/allolib-doc/]()
