import argparse
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="A script to embed files as byte data")
    # Collects one or more items into a list
    parser.add_argument('--files', nargs='+', type=str, help="List of files to embed")
    args = parser.parse_args()


    for item in args.files:
        file_path: Path = Path(item)
        file_size: int = file_path.stat().st_size

        file_embed_directory: Path = file_path.parent / "embed"
        file_embed_directory.mkdir(exist_ok=True)
        file_embed_path: Path = file_embed_directory / (file_path.with_suffix(".c")).name

        chunk_size: int = 1024*1024
        row_length: int = 32

        with open(file_embed_path, "w") as w:
            print("#include <stdint.h>\n", file=w)
            print(f"const static size_t embed_{file_path.stem}_count = {file_size};\n", file=w)
            print(f"const static uint8_t embed_{file_path.stem}[{file_size}] = {{", end="", file=w)
            with open(file_path, "rb") as r:
                while True:
                    data = r.read(chunk_size)
                    if not data:
                        break

                    for i, b in enumerate(data):
                        if i % row_length == 0:
                            print("\n    ", end="", file=w)
                        print(f"0x{b:02x}", end=", ", file=w)
            print("\n};", file=w)

if __name__ == "__main__":
    main()
