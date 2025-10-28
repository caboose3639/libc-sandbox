import subprocess

command = "nm -D /usr/lib/x86_64-linux-gnu/libc.so.6 | grep -E ' (T|W) ' | awk '{print $3}' | cut -d@ -f1 | sort -u"

result = subprocess.run(command, shell = True, capture_output = True, text = True)

callnames = result.stdout

header = []
header.append("#pragma once")
header.append("#include <unordered_set>")
header.append("#include <string>")
header.append("")
header.append("static const std::unordered_set<std::string> libc_callnames = {")

for line in callnames.splitlines():
    header.append(f"    \"{line}\",")

header.append("};")
header.append("")

out_path = "llvm-passes/include/CallNames.h"
with open(out_path, "w") as f:
    f.write("\n".join(header))