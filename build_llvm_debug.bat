pushd
cd __build\llvm
cmake --build . --config Debug --target install -j 8
cmake --install . --config Debug
popd