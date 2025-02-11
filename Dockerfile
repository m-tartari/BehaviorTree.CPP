# GCC support can be specified at major, minor, or micro version
# (e.g. 8, 8.2 or 8.2.0).
# See https://hub.docker.com/r/library/gcc/ for all supported GCC
# tags from Docker Hub.
# See https://docs.docker.com/samples/library/gcc/ for more on how to use this image
FROM conanio/gcc9:latest

# These commands copy your files into the specified directory in the image
# and set that as the working location
USER conan
COPY --chown=conan:1001 . /workspaces/BehaviorTree.CPP
WORKDIR /workspaces/BehaviorTree.CPP/build
RUN conan install .. --output-folder=. --build=missing --profile:build=default
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake"
RUN cmake --build . --parallel

# This command runs your application, comment out this line to compile only
CMD ["/workspaces/BehaviorTree.CPP/build/examples/t19_linfa_manager"]
