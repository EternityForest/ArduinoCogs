import os
import gzip

def file_to_c_array(filename: str) -> str:
    with open(filename, "rb") as f:
        data = f.read()
    data = gzip.compress(data)

    vn = os.path.basename(filename).replace('.', '_')+"_gz"
    br = '{'
    c_array = f"const unsigned char {vn}[] = {br}\n"

    for i in range(0, len(data), 16):
        line = ", ".join("0x{:02X}".format(b) for b in data[i:i+16])
        c_array += "    " + line + ",\n"
    c_array += "};\n"

    return c_array

d = os.path.dirname(os.path.abspath(__file__))
d2 = os.path.join(d, "builtinfiles")

opd = os.path.join(d, "src/web/generated_data")
os.makedirs(opd, exist_ok=True)

if __name__ == "__main__":
    for fn in os.listdir(d2):
        fn2 = os.path.basename(fn).replace('.', '_')+"_gz.h"
        c_array = file_to_c_array(os.path.join(d2, fn))
        with open(os.path.join(opd, os.path.basename(fn2)), "w") as f:
            f.write(c_array)