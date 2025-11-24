#!/usr/bin/env python3
"""
Generate spi_dict.txt file with random HMAC keys.

This script generates a spi_dict.txt file with a configurable number of
random HMAC keys. Each key is 32 hex characters (16 bytes) long.
"""

import argparse
import os
import secrets
import sys


def generate_random_key() -> str:
    """Generate a random 32-character hex key (16 bytes)."""
    return secrets.token_hex(16)


def generate_spi_dict(num_keys: int, output_path: str) -> tuple[str, str]:
    """
    Generate spi_dict.txt file with random keys.

    Args:
        num_keys: Number of keys to generate
        output_path: Path to output file

    Returns:
        Tuple of (first_key, first_key_with_0x_prefix)
    """
    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    with open(output_path, "w") as f:
        # Write header
        f.write("# spi  authType  key\n")

        # Generate keys
        first_key = None
        for i in range(1, num_keys + 1):
            key = generate_random_key()
            if first_key is None:
                first_key = key

            # Format SPI as _XXXX where XXXX is hex (e.g., _0001, _000a, _0010)
            spi_hex = f"_{i:04x}"
            f.write(f"{spi_hex}  HMAC  {key}\n")

    return first_key, f"0x{first_key}"


def extract_first_key_from_file(file_path: str) -> tuple[str, str]:
    """
    Extract the first key from an existing spi_dict.txt file.

    Args:
        file_path: Path to spi_dict.txt file

    Returns:
        Tuple of (first_key, first_key_with_0x_prefix)
    """
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"spi_dict.txt not found at {file_path}")

    with open(file_path, "r") as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if line and not line.startswith("#"):
                parts = line.split()
                if len(parts) >= 3:
                    key = parts[2]
                    return key, f"0x{key}"

    raise ValueError("No valid key found in spi_dict.txt")


def main():
    parser = argparse.ArgumentParser(
        description="Generate spi_dict.txt with random HMAC keys"
    )
    parser.add_argument(
        "--output",
        type=str,
        default="UploadsFIlesystem/AuthenticateFiles/spi_dict.txt",
        help="Output path for spi_dict.txt (default: UploadsFIlesystem/AuthenticateFiles/spi_dict.txt)",
    )
    parser.add_argument(
        "--num-keys",
        type=int,
        default=10,
        help="Number of keys to generate (default: 10)",
    )
    parser.add_argument(
        "--skip-generation",
        action="store_true",
        help="Skip generation, only extract key (requires --print-first-key)",
    )

    args = parser.parse_args()

    if not args.skip_generation:
        try:
            generate_spi_dict(args.num_keys, args.output)
        except (FileNotFoundError, ValueError) as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
