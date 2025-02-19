import gzip
import json
import os


def add_date_to_scheme_ref_urls(schema, fn):
    """$ref needs cache bust vals"""
    if isinstance(schema, dict):
        for k, v in schema.items():
            if isinstance(v, dict):
                add_date_to_scheme_ref_urls(v, fn)
            elif isinstance(v, list):
                add_date_to_scheme_ref_urls(v, fn)

            if k == "$ref":
                schema[k] = f"{schema[k]}?cache={os.path.getmtime(fn)}"

    elif isinstance(schema, list):
        for i in schema:
            add_date_to_scheme_ref_urls(i, fn)


def process_json(s, fn):
    j = json.loads(s)
    add_date_to_scheme_ref_urls(j, fn)
    return json.dumps(j)


def file_to_c_array(filename: str) -> str:
    with open(filename, "rb") as f:
        data = f.read()
    if filename.endswith(".json"):
        data = process_json(data.decode("utf-8"), filename).encode("utf-8")
    data = gzip.compress(data, mtime=os.path.getmtime(filename))

    vn = os.path.basename(filename).replace(".", "_") + "_gz"
    br = "{"
    c_array = f"const unsigned char {vn}[] = {br}\n"

    for i in range(0, len(data), 16):
        line = ", ".join(f"0x{b:02X}" for b in data[i : i + 16])
        c_array += "    " + line + ",\n"
    c_array += "};\n"

    return c_array


d = os.path.dirname(os.path.abspath(__file__))
d2 = os.path.join(d, "builtinfiles")

opd = os.path.join(d, "src/web/generated_data")
os.makedirs(opd, exist_ok=True)

if __name__ == "__main__":
    for fn in os.listdir(d2):
        fn2 = os.path.basename(fn).replace(".", "_") + "_gz.h"
        c_array = file_to_c_array(os.path.join(d2, fn))
        with open(os.path.join(opd, os.path.basename(fn2)), "w") as f:
            f.write(c_array)
