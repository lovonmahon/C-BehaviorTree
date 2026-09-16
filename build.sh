rm -rf build_directory
cmake -S . -B build_directory
cmake --build build_directory

# Run the program
./build_directory/BehaviorTree