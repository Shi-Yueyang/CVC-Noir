import argparse
import os
import struct
import sys


def main():
    parser = argparse.ArgumentParser(
        description="Create dataplug.bin with specified wheel diameters."
    )
    parser.add_argument(
        "--diameter", "-d",
        type=int,
        nargs="+",
        default=[1050, 1050, 1050],
        help="Wheel diameter values (default: 1050 1050 1050)"
    )
    parser.add_argument(
        "--output-dir", "-o",
        default=".",
        help="Output directory (default: current directory)"
    )

    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    out_path = os.path.join(args.output_dir, "dataplug.bin")

    with open(out_path, "wb") as f:
        for value in args.diameter:
            f.write(struct.pack("<H", value))

    print(f"Written {args.diameter} to {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())