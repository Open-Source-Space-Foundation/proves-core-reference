#!/usr/bin/env python3
"""
Generate AuthDefaultKey.h file with a random HMAC key.

This script generates AuthDefaultKey.h directly without needing spi_dict.txt.
"""

import argparse
import os
import secrets
import sys


def generate_random_key() -> str:
    """Generate a random 32-character hex key (16 bytes)."""
    return secrets.token_hex(16)


def generate_auth_key_header(key: str, output_path: str, template_path: str) -> None:
    """
    Generate AuthDefaultKey.h file from template.

    Args:
        key: The authentication key (hex string without 0x prefix)
        output_path: Path to output header file
        template_path: Path to template file
    """
    # Ensure output directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)

    # Read template
    with open(template_path, "r") as f:
        template = f.read()

    # Replace placeholder with actual key
    content = template.replace("@AUTH_DEFAULT_KEY@", key)

    # Write output file
    with open(output_path, "w") as f:
        f.write(content)


def extract_key_from_header(header_path: str) -> str:
    """
    Extract the key from an existing AuthDefaultKey.h file.

    Args:
        header_path: Path to AuthDefaultKey.h file

    Returns:
        The authentication key (hex string without 0x prefix)
    """
    if not os.path.exists(header_path):
        raise FileNotFoundError(f"AuthDefaultKey.h not found at {header_path}")

    with open(header_path, "r") as f:
        for line in f:
            if "AUTH_DEFAULT_KEY" in line and '"' in line:
                # Extract key from line like: #define AUTH_DEFAULT_KEY "4916d208d40612daad6edbc7333c4c13"
                start = line.find('"') + 1
                end = line.find('"', start)
                if start > 0 and end > start:
                    return line[start:end]

    raise ValueError("No valid key found in AuthDefaultKey.h")


def main():
    """Main function to parse arguments and generate/extract AuthDefaultKey.h."""
    parser = argparse.ArgumentParser(
        description="Generate AuthDefaultKey.h with a random HMAC key"
    )
    parser.add_argument(
        "--output",
        type=str,
        default="FprimeZephyrReference/Components/Authenticate/AuthDefaultKey.h",
        help="Output path for AuthDefaultKey.h",
    )
    parser.add_argument(
        "--template",
        type=str,
        default="scripts/generate_auth_default_key.h",
        help="Path to template file",
    )
    parser.add_argument(
        "--key",
        type=str,
        default=None,
        help="Use this specific key instead of generating a random one",
    )
    parser.add_argument(
        "--print-key",
        action="store_true",
        help="Print the key to stdout (for use in Makefile or debugging)",
    )
    parser.add_argument(
        "--extract",
        action="store_true",
        help="Extract key from existing header file instead of generating",
    )

    args = parser.parse_args()

    if args.extract:
        # Extract from existing file
        try:
            key = extract_key_from_header(args.output)
            if args.print_key:
                print(key)
            else:
                print(f"Extracted key from {args.output}: {key}")
        except (FileNotFoundError, ValueError) as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        # Generate new file
        try:
            if args.key:
                key = args.key
                # Remove 0x prefix if present
                if key.startswith("0x") or key.startswith("0X"):
                    key = key[2:]
            else:
                key = generate_random_key()

            generate_auth_key_header(key, args.output, args.template)

            if args.print_key:
                print(key)
            else:
                print(f"Generated {args.output} with key: {key}")
        except (FileNotFoundError, ValueError) as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
