#!/usr/bin/env python3
"""Validate and scaffold the branch-local camera driver artifact catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path, PurePosixPath
from typing import Any


INDEX_PATH = Path("ci/camera-driver-artifacts.json")
FRAGMENT_DIR = Path("ci/camera-driver-artifacts")
SUPPORTED_SCHEMA_VERSION = 1
ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9.-]*$")
OUTPUT_PATTERN = re.compile(r"^[A-Za-z0-9_.+-]+\.ko$")


class CatalogError(ValueError):
    """Raised when source and catalog do not describe the same inventory."""


def _read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise CatalogError(f"branch-local catalog file is missing: {path}") from exc
    except json.JSONDecodeError as exc:
        raise CatalogError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise CatalogError(f"catalog document must be an object: {path}")
    return value


def _makefile_assignments(path: Path, variable_pattern: str) -> set[str]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except FileNotFoundError as exc:
        raise CatalogError(f"Makefile is missing: {path}") from exc
    text = re.sub(r"\\\r?\n", " ", text)
    values: set[str] = set()
    pattern = re.compile(
        rf"^\s*{variable_pattern}\s*\+=\s*(.*?)\s*(?:#.*)?$", re.MULTILINE
    )
    for match in pattern.finditer(text):
        values.update(re.findall(r"([A-Za-z0-9_.+-]+)", match.group(1)))
    return values


def discover_makefile_modules(root: Path) -> dict[tuple[str, str], str]:
    directories = {
        value
        for value in _makefile_assignments(
            root / "Makefile", r"obj-(?:m|\$\([^)]+\))"
        )
        if (root / value).is_dir()
    }
    modules: dict[tuple[str, str], str] = {}
    for directory in sorted(directories):
        makefile = root / directory / "Makefile"
        targets = _makefile_assignments(makefile, r"obj-(?:m|\$\([^)]+\))")
        for target in sorted(targets):
            if target.endswith(".o"):
                target = target[:-2]
            if target and not (root / directory / target).is_dir():
                modules[(directory, target)] = f"{target}.ko"
    return modules


def load_catalog(root: Path) -> dict[str, Any]:
    index_path = root / INDEX_PATH
    index = _read_json(index_path)
    if index.get("schemaVersion") != SUPPORTED_SCHEMA_VERSION:
        raise CatalogError(
            f"unknown driver catalog schemaVersion in {index_path}: "
            f"{index.get('schemaVersion')!r}; validator supports 1"
        )
    if index.get("kind") != "camera-driver-artifact-index":
        raise CatalogError(f"invalid catalog index kind in {index_path}")
    fragments = index.get("fragments")
    if not isinstance(fragments, list) or not fragments:
        raise CatalogError(f"catalog index fragments must be a non-empty list: {index_path}")
    if len(fragments) != len(set(fragments)):
        raise CatalogError(f"catalog index contains duplicate fragments: {index_path}")

    expected_fragment_paths: set[Path] = set()
    artifacts: list[dict[str, Any]] = []
    for relative in fragments:
        if not isinstance(relative, str):
            raise CatalogError(f"catalog fragment path must be a string: {relative!r}")
        posix = PurePosixPath(relative)
        if (
            posix.is_absolute()
            or ".." in posix.parts
            or tuple(posix.parts[:2]) != ("ci", "camera-driver-artifacts")
        ):
            raise CatalogError(f"unsafe catalog fragment path: {relative}")
        fragment_path = root.joinpath(*posix.parts)
        expected_fragment_paths.add(fragment_path.resolve())
        fragment = _read_json(fragment_path)
        if fragment.get("schemaVersion") != SUPPORTED_SCHEMA_VERSION:
            raise CatalogError(
                f"unknown driver fragment schemaVersion in {fragment_path}: "
                f"{fragment.get('schemaVersion')!r}"
            )
        if fragment.get("kind") != "camera-driver-artifact-fragment":
            raise CatalogError(f"invalid catalog fragment kind in {fragment_path}")
        if not isinstance(fragment.get("family"), str) or not fragment["family"]:
            raise CatalogError(f"catalog fragment family is missing: {fragment_path}")
        fragment_artifacts = fragment.get("artifacts")
        if not isinstance(fragment_artifacts, list):
            raise CatalogError(f"catalog fragment artifacts must be a list: {fragment_path}")
        for artifact in fragment_artifacts:
            if not isinstance(artifact, dict):
                raise CatalogError(f"catalog artifact must be an object: {fragment_path}")
            materialized = dict(artifact)
            materialized["catalogFragment"] = relative
            artifacts.append(materialized)

    actual_fragment_paths = {
        path.resolve() for path in (root / FRAGMENT_DIR).glob("*.json")
    }
    missing = expected_fragment_paths - actual_fragment_paths
    orphaned = actual_fragment_paths - expected_fragment_paths
    if missing:
        raise CatalogError(
            "catalog index references missing fragments: "
            + ", ".join(sorted(str(path) for path in missing))
        )
    if orphaned:
        raise CatalogError(
            "orphan driver catalog fragments are not listed by the index: "
            + ", ".join(sorted(str(path) for path in orphaned))
        )
    return {"index": index, "artifacts": artifacts}


def _validate_artifact_shape(artifact: dict[str, Any]) -> None:
    artifact_id = artifact.get("id")
    if not isinstance(artifact_id, str) or not ID_PATTERN.fullmatch(artifact_id):
        raise CatalogError(f"invalid driver logical ID: {artifact_id!r}")
    for key in ("makeDirectory", "makeTarget", "output", "installGroup"):
        if not isinstance(artifact.get(key), str) or not artifact[key]:
            raise CatalogError(f"driver {artifact_id} is missing {key}")
    if not OUTPUT_PATTERN.fullmatch(artifact["output"]):
        raise CatalogError(f"driver {artifact_id} has invalid output {artifact['output']!r}")
    for key in ("dependsOn", "legacyOutputs"):
        if not isinstance(artifact.get(key), list):
            raise CatalogError(f"driver {artifact_id} {key} must be a list")
        if len(artifact[key]) != len(set(artifact[key])):
            raise CatalogError(f"driver {artifact_id} has duplicate {key}")
    if artifact["output"] in artifact["legacyOutputs"]:
        raise CatalogError(f"driver {artifact_id} lists its current output as a legacy alias")
    for legacy in artifact["legacyOutputs"]:
        if not isinstance(legacy, str) or not OUTPUT_PATTERN.fullmatch(legacy):
            raise CatalogError(f"driver {artifact_id} has invalid legacy output {legacy!r}")


def dependency_closure(
    artifacts_by_id: dict[str, dict[str, Any]], selected_ids: list[str]
) -> list[str]:
    result: list[str] = []
    visited: set[str] = set()
    visiting: set[str] = set()

    def visit(artifact_id: str) -> None:
        if artifact_id in visited:
            return
        if artifact_id in visiting:
            raise CatalogError(f"driver dependency cycle includes {artifact_id}")
        artifact = artifacts_by_id.get(artifact_id)
        if artifact is None:
            raise CatalogError(f"driver dependency references unknown logical ID {artifact_id}")
        visiting.add(artifact_id)
        for dependency in artifact["dependsOn"]:
            visit(dependency)
        visiting.remove(artifact_id)
        visited.add(artifact_id)
        result.append(artifact_id)

    for selected_id in selected_ids:
        visit(selected_id)
    return result


def validate_previous_outputs(
    current_by_id: dict[str, dict[str, Any]], previous_by_id: dict[str, dict[str, Any]]
) -> None:
    for artifact_id, previous in previous_by_id.items():
        current = current_by_id.get(artifact_id)
        if current is None or current["output"] == previous["output"]:
            continue
        if previous["output"] not in current["legacyOutputs"]:
            raise CatalogError(
                f"driver {artifact_id} output changed from {previous['output']} to "
                f"{current['output']} without legacyOutputs alias {previous['output']}"
            )


def validate_repository(root: Path, previous_root: Path | None = None) -> str:
    catalog = load_catalog(root)
    artifacts = catalog["artifacts"]
    for artifact in artifacts:
        _validate_artifact_shape(artifact)

    ids = [artifact["id"] for artifact in artifacts]
    outputs = [artifact["output"] for artifact in artifacts]
    source_keys = [
        (artifact["makeDirectory"], artifact["makeTarget"])
        for artifact in artifacts
    ]
    if len(ids) != len(set(ids)):
        raise CatalogError("driver catalog contains duplicate logical IDs")
    if len(outputs) != len(set(outputs)):
        raise CatalogError("driver catalog contains duplicate current outputs")
    if len(source_keys) != len(set(source_keys)):
        raise CatalogError("driver catalog contains duplicate Makefile targets")

    by_id = {artifact["id"]: artifact for artifact in artifacts}
    dependency_closure(by_id, sorted(by_id))

    discovered = discover_makefile_modules(root)
    catalog_modules = {
        (artifact["makeDirectory"], artifact["makeTarget"]): artifact["output"]
        for artifact in artifacts
    }
    missing_catalog = sorted(set(discovered) - set(catalog_modules))
    stale_catalog = sorted(set(catalog_modules) - set(discovered))
    mismatched_outputs = sorted(
        key
        for key in set(discovered) & set(catalog_modules)
        if discovered[key] != catalog_modules[key]
    )
    issues: list[str] = []
    issues.extend(
        f"uncatalogued Makefile module {directory}/{target}.ko; run --scaffold"
        for directory, target in missing_catalog
    )
    issues.extend(
        f"stale catalog target {directory}/{target}.ko is not declared by enabled Makefiles"
        for directory, target in stale_catalog
    )
    issues.extend(
        f"catalog output mismatch for {directory}/{target}: "
        f"Makefile produces {discovered[(directory, target)]}, "
        f"catalog declares {catalog_modules[(directory, target)]}"
        for directory, target in mismatched_outputs
    )
    for artifact in artifacts:
        expected = f"{artifact['makeTarget']}.ko"
        if artifact["output"] != expected:
            issues.append(
                f"driver {artifact['id']} output {artifact['output']} does not match "
                f"Makefile target {expected}"
            )
    if issues:
        raise CatalogError("\n".join(issues))

    if previous_root is not None:
        previous = load_catalog(previous_root)["artifacts"]
        validate_previous_outputs(by_id, {item["id"]: item for item in previous})

    normalized = [
        {key: value for key, value in artifact.items() if key != "catalogFragment"}
        for artifact in sorted(artifacts, key=lambda item: item["id"])
    ]
    return hashlib.sha256(
        json.dumps(normalized, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def scaffold(root: Path) -> dict[str, Any]:
    discovered = discover_makefile_modules(root)
    try:
        catalog = load_catalog(root)
        known = {
            (item["makeDirectory"], item["makeTarget"])
            for item in catalog["artifacts"]
        }
    except CatalogError:
        known = set()
    pending = []
    for (directory, target), output in sorted(discovered.items()):
        if (directory, target) in known:
            continue
        pending.append(
            {
                "id": target.removesuffix("_tn").replace("_", "-"),
                "makeDirectory": directory,
                "makeTarget": target,
                "output": output,
                "installGroup": directory,
                "dependsOn": [],
                "legacyOutputs": [],
                "reviewRequired": ["id", "dependsOn", "installGroup", "legacyOutputs"],
            }
        )
    return {"kind": "camera-driver-artifact-scaffold", "artifacts": pending}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--previous-root", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--scaffold", action="store_true")
    args = parser.parse_args()
    try:
        if args.scaffold:
            print(json.dumps(scaffold(args.root.resolve()), indent=2))
        else:
            digest = validate_repository(
                args.root.resolve(),
                args.previous_root.resolve() if args.previous_root else None,
            )
            print(f"Camera driver artifact catalog OK: sha256:{digest}")
    except CatalogError as exc:
        print(f"Camera driver artifact catalog validation failed:\n{exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
