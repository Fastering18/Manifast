with open("CMakeLists.txt", "r") as f:
    content = f.read()

# We need to make sure the original tests block is kept.
# Wait, let's look at the original CMakeLists.txt on master.
