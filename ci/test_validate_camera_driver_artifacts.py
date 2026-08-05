import copy
import json
import tempfile
import unittest
from pathlib import Path

from ci import validate_camera_driver_artifacts as validator


REPO_ROOT = Path(__file__).resolve().parents[1]


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def create_fixture(root: Path) -> None:
    (root / "tevs").mkdir(parents=True)
    (root / "maxim-gmsl").mkdir(parents=True)
    (root / "Makefile").write_text(
        "obj-m += tevs/\nobj-m += maxim-gmsl/\n", encoding="utf-8"
    )
    (root / "tevs" / "Makefile").write_text("obj-m += tevs.o\n", encoding="utf-8")
    (root / "maxim-gmsl" / "Makefile").write_text(
        "obj-m += max_serdes_all_tn.o\nobj-m += max96724_tn.o\n",
        encoding="utf-8",
    )
    write_json(
        root / validator.INDEX_PATH,
        {
            "schemaVersion": 1,
            "kind": "camera-driver-artifact-index",
            "fragments": [
                "ci/camera-driver-artifacts/tevs.json",
                "ci/camera-driver-artifacts/maxim-gmsl.json",
            ],
        },
    )
    write_json(
        root / "ci/camera-driver-artifacts/tevs.json",
        {
            "schemaVersion": 1,
            "kind": "camera-driver-artifact-fragment",
            "family": "tevs",
            "artifacts": [
                {
                    "id": "tevs",
                    "makeDirectory": "tevs",
                    "makeTarget": "tevs",
                    "output": "tevs.ko",
                    "installGroup": "tevs",
                    "dependsOn": [],
                    "legacyOutputs": [],
                }
            ],
        },
    )
    write_json(
        root / "ci/camera-driver-artifacts/maxim-gmsl.json",
        {
            "schemaVersion": 1,
            "kind": "camera-driver-artifact-fragment",
            "family": "maxim-gmsl",
            "artifacts": [
                {
                    "id": "max-serdes-common",
                    "makeDirectory": "maxim-gmsl",
                    "makeTarget": "max_serdes_all_tn",
                    "output": "max_serdes_all_tn.ko",
                    "installGroup": "maxim-gmsl",
                    "dependsOn": [],
                    "legacyOutputs": [],
                },
                {
                    "id": "max96724",
                    "makeDirectory": "maxim-gmsl",
                    "makeTarget": "max96724_tn",
                    "output": "max96724_tn.ko",
                    "installGroup": "maxim-gmsl",
                    "dependsOn": ["max-serdes-common"],
                    "legacyOutputs": ["max96724.ko"],
                },
            ],
        },
    )


class DriverCatalogTests(unittest.TestCase):
    def test_current_branch_makefiles_and_catalog_are_bidirectional(self) -> None:
        digest = validator.validate_repository(REPO_ROOT)

        self.assertRegex(digest, r"^[0-9a-f]{64}$")

    def test_dependency_closure_orders_common_module_before_driver(self) -> None:
        artifacts = validator.load_catalog(REPO_ROOT)["artifacts"]
        by_id = {artifact["id"]: artifact for artifact in artifacts}

        self.assertEqual(
            validator.dependency_closure(by_id, ["max96724"]),
            ["max-serdes-common", "max96724"],
        )

    def test_new_makefile_module_requires_catalog_entry(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            create_fixture(root)
            with (root / "tevs" / "Makefile").open("a", encoding="utf-8") as handle:
                handle.write("obj-m += new_sensor.o\n")

            with self.assertRaisesRegex(validator.CatalogError, "uncatalogued.*new_sensor"):
                validator.validate_repository(root)

            scaffold = validator.scaffold(root)
            self.assertEqual(scaffold["artifacts"][0]["makeTarget"], "new_sensor")

    def test_stale_catalog_entry_requires_makefile_target(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            create_fixture(root)
            (root / "tevs" / "Makefile").write_text("", encoding="utf-8")

            with self.assertRaisesRegex(validator.CatalogError, "stale catalog target"):
                validator.validate_repository(root)

    def test_orphan_fragment_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            create_fixture(root)
            write_json(root / "ci/camera-driver-artifacts/orphan.json", {})

            with self.assertRaisesRegex(validator.CatalogError, "orphan driver catalog"):
                validator.validate_repository(root)

    def test_output_rename_requires_catalog_and_legacy_alias(self) -> None:
        with tempfile.TemporaryDirectory() as previous_dir, tempfile.TemporaryDirectory() as current_dir:
            previous_root = Path(previous_dir)
            current_root = Path(current_dir)
            create_fixture(previous_root)
            create_fixture(current_root)
            (current_root / "maxim-gmsl" / "Makefile").write_text(
                "obj-m += max_serdes_all_tn.o\nobj-m += max96724_v2_tn.o\n",
                encoding="utf-8",
            )

            with self.assertRaisesRegex(validator.CatalogError, "uncatalogued.*max96724_v2_tn"):
                validator.validate_repository(current_root)

            fragment_path = current_root / "ci/camera-driver-artifacts/maxim-gmsl.json"
            fragment = json.loads(fragment_path.read_text(encoding="utf-8"))
            renamed = next(item for item in fragment["artifacts"] if item["id"] == "max96724")
            renamed["makeTarget"] = "max96724_v2_tn"
            renamed["output"] = "max96724_v2_tn.ko"
            renamed["legacyOutputs"] = []
            write_json(fragment_path, fragment)

            with self.assertRaisesRegex(validator.CatalogError, "without legacyOutputs alias"):
                validator.validate_repository(current_root, previous_root)

    def test_unknown_schema_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            create_fixture(root)
            index_path = root / validator.INDEX_PATH
            index = json.loads(index_path.read_text(encoding="utf-8"))
            index["schemaVersion"] = 2
            write_json(index_path, index)

            with self.assertRaisesRegex(validator.CatalogError, "unknown.*schemaVersion"):
                validator.validate_repository(root)


if __name__ == "__main__":
    unittest.main()
