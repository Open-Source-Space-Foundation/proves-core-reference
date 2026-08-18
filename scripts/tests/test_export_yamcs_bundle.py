"""Unit tests for the standalone Yamcs bundle exporter."""

import contextlib
import io
import json
import stat
import tempfile
import unittest
from pathlib import Path

from scripts.export_yamcs_bundle import export_bundle, extract_auth_key, find_dictionary


class ExportYamcsBundleTest(unittest.TestCase):
    """Exercise validation, normalization, and secret-safe output."""

    def setUp(self):
        """Create a valid isolated dictionary and authentication header."""
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.dictionary_root = self.root / "build-artifacts"
        self.dictionary_root.mkdir()
        self.dictionary = self.dictionary_root / "ReferenceTopologyDictionary.json"
        self.dictionary.write_text(json.dumps({"constants": []}), encoding="utf-8")
        self.header = self.root / "AuthDefaultKey.h"
        self.header.write_text(
            '#define AUTH_DEFAULT_KEY "00112233445566778899AABBCCDDEEFF"\n',
            encoding="utf-8",
        )

    def tearDown(self):
        """Remove the isolated export tree."""
        self.temporary.cleanup()

    def test_exports_normalized_private_key_without_logging_it(self):
        """The exported key is normalized, private, and absent from logs."""
        output = self.root / "inputs/proves"
        captured = io.StringIO()
        with contextlib.redirect_stdout(captured):
            export_bundle(self.dictionary_root, self.header, output)

        self.assertEqual(
            (output / "auth-key.hex").read_text(encoding="ascii"),
            "00112233445566778899aabbccddeeff\n",
        )
        self.assertEqual(stat.S_IMODE((output / "auth-key.hex").stat().st_mode), 0o600)
        self.assertNotIn("00112233445566778899aabbccddeeff", captured.getvalue())
        self.assertEqual(
            json.loads((output / "fprime-dictionary.json").read_text()),
            {"constants": []},
        )

    def test_dictionary_count_must_be_exactly_one(self):
        """Ambiguous build-artifact trees are rejected."""
        second = self.dictionary_root / "SecondTopologyDictionary.json"
        second.write_text(json.dumps({"constants": []}), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "found 2"):
            find_dictionary(self.dictionary_root)

    def test_dictionary_must_exist(self):
        """A build without a topology dictionary is rejected."""
        self.dictionary.unlink()
        with self.assertRaisesRegex(ValueError, "found 0"):
            find_dictionary(self.dictionary_root)

    def test_dictionary_must_be_valid_json_with_constants(self):
        """Malformed or structurally invalid dictionaries are rejected."""
        self.dictionary.write_text("not json", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "invalid topology dictionary"):
            find_dictionary(self.dictionary_root)

        self.dictionary.write_text(json.dumps({"metadata": {}}), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "no constants list"):
            find_dictionary(self.dictionary_root)

    def test_rejects_malformed_key(self):
        """Malformed C header key definitions are rejected."""
        self.header.write_text('#define AUTH_DEFAULT_KEY "not-a-key"\n')
        with self.assertRaisesRegex(ValueError, "exactly one"):
            extract_auth_key(self.header)


if __name__ == "__main__":
    unittest.main()
