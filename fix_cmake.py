with open("CMakeLists.txt", "r") as f:
    lines = f.readlines()

new_lines = []
in_conflict = False
skip = False
for line in lines:
    if line.startswith("<<<<<<< HEAD"):
        in_conflict = True
        skip = True
    elif line.startswith("======="):
        if in_conflict:
            skip = False
        else:
            new_lines.append(line)
    elif line.startswith(">>>>>>> aa02ade"):
        if in_conflict:
            in_conflict = False
            skip = False
        else:
            new_lines.append(line)
    else:
        if not skip:
            new_lines.append(line)

with open("CMakeLists.txt", "w") as f:
    f.writelines(new_lines)
