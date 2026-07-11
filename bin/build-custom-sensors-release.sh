#!/usr/bin/env bash
# Build custom-sensors release matrix: Heltec V4 + Seeed Solar Node (no GPS),
# UART controlled only, for 2.7.15 / 2.7.26 bases.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/release/custom-sensors"
PATH=~/.platformio/penv/bin:$PATH
TAG_2715=v2.7.15.405b2f5b9
TAG_2726=v2.7.26.54e0d8d
WORKTREE_2715="$ROOT/.worktree-2715-release"
WORKTREE_2726="$ROOT/.worktree-2726-release"

PATCH_FILES=(
  src/modules/Telemetry/Sensor/DYPA01Sensor.cpp
  src/modules/Telemetry/Sensor/DYPA01Sensor.h
  variants/nrf52840/seeed_solar_node/variant.h
  variants/nrf52840/seeed_solar_node/variant.cpp
  variants/nrf52840/seeed_solar_node/platformio.ini
  variants/esp32s3/heltec_v4/platformio.ini
  variants/esp32s3/heltec_v4/variant.h
)

copy_artifacts() {
  local build_root="$1"
  local env="$2"
  local dest="$3"
  local arch="$4"
  local ver
  ver="$(cd "$build_root" && bin/buildinfo.py long)"

  mkdir -p "$dest"
  local build_dir="$build_root/.pio/build/${env}"
  local base="firmware-${env}-${ver}"

  if [[ "$arch" == "nrf52" ]]; then
    if [[ -f "$build_dir/${base}.uf2" ]]; then
      cp "$build_dir/${base}.uf2" "$dest/"
      cp "$build_dir/${base}.zip" "$dest/${base}-ota.zip"
      cp "$build_dir/${base}.mt.json" "$dest/" 2>/dev/null || true
    else
      cp "$build_dir/firmware.uf2" "$dest/firmware-${env}-${ver}.uf2"
      cp "$build_dir/firmware.zip" "$dest/firmware-${env}-${ver}-ota.zip"
    fi
  else
    if [[ -f "$build_dir/${base}.factory.bin" ]]; then
      cp "$build_dir/${base}.factory.bin" "$dest/"
      cp "$build_dir/${base}.bin" "$dest/"
      cp "$build_dir/littlefs-${env}-${ver}.bin" "$dest/" 2>/dev/null || true
      cp "$build_dir/${base}.mt.json" "$dest/" 2>/dev/null || true
    else
      cp "$build_dir/firmware.factory.bin" "$dest/firmware-${env}-${ver}.factory.bin"
      cp "$build_dir/firmware.bin" "$dest/firmware-${env}-${ver}.bin"
      cp "$build_dir/littlefs.bin" "$dest/littlefs-${env}-${ver}.bin" 2>/dev/null || true
    fi
  fi
}

build_env() {
  local build_root="$1"
  local env="$2"
  local dest="$3"
  local arch="$4"
  local use_mtjson="${5:-true}"
  echo "========== Building $env @ $(cd "$build_root" && bin/buildinfo.py long) -> $dest =========="
  (
    cd "$build_root"
    platformio pkg install -e "$env" >/dev/null
    export APP_VERSION="$(bin/buildinfo.py long)"
    rm -f ".pio/build/${env}/firmware-${env}-"*
    if [[ "$use_mtjson" == "true" ]]; then
      pio run --environment "$env" -t mtjson
    else
      pio run --environment "$env"
    fi
  )
  copy_artifacts "$build_root" "$env" "$dest" "$arch"
}

patch_worktree() {
  local worktree="$1"
  local backport_proto="${2:-false}"
  local backport_env_telemetry="${3:-false}"

  for f in "${PATCH_FILES[@]}"; do
    cp "$ROOT/$f" "$worktree/$f"
  done

  if [[ "$backport_proto" == "true" ]]; then
    git -C "$worktree" submodule update --init protobufs
    cp "$ROOT/protobufs/meshtastic/telemetry.proto" "$worktree/protobufs/meshtastic/telemetry.proto"
    cp "$ROOT/src/mesh/generated/meshtastic/telemetry.pb.h" "$worktree/src/mesh/generated/meshtastic/telemetry.pb.h"
  fi

  if [[ "$backport_env_telemetry" == "true" ]]; then
    cp "$ROOT/src/modules/Telemetry/EnvironmentTelemetry.cpp" "$worktree/src/modules/Telemetry/EnvironmentTelemetry.cpp"
  fi
}

setup_worktree() {
  local worktree="$1"
  local tag="$2"
  local backport_proto="${3:-false}"
  local backport_env_telemetry="${4:-false}"

  if [[ ! -e "$worktree/.git" ]]; then
    git -C "$ROOT" worktree add "$worktree" "$tag"
  fi
  patch_worktree "$worktree" "$backport_proto" "$backport_env_telemetry"
}

build_matrix() {
  local build_root="$1"
  local out_ver="$2"
  local use_mtjson="${3:-true}"
  local out_base="$OUT/$out_ver"
  build_env "$build_root" heltec-v4 "$out_base/heltec-v4-dyp-controlled" esp32 "$use_mtjson"
  build_env "$build_root" seeed_solar_node_dyp_nogps "$out_base/seeed-solar-node-dyp-nogps-controlled" nrf52 "$use_mtjson"
}

mkdir -p "$OUT"

echo "========== 2.7.15 (worktree) =========="
setup_worktree "$WORKTREE_2715" "$TAG_2715" false false
build_matrix "$WORKTREE_2715" "2.7.15" false

echo "========== 2.7.26 (worktree) =========="
setup_worktree "$WORKTREE_2726" "$TAG_2726" true true
build_matrix "$WORKTREE_2726" "2.7.26" true

cat >"$OUT/README.md" <<'EOF'
# Custom sensors release (DYP-A01)

## Targets

| Folder | Board | GPS | DYP UART mode |
|--------|-------|-----|---------------|
| `heltec-v4-dyp-controlled` | Heltec V4 | Yes (Serial1) | Controlled on-demand (ANYTB SKU) |
| `seeed-solar-node-dyp-nogps-controlled` | Seeed Solar Node | Disabled | Controlled on-demand on GNSS UART (D6/D7) |

## Driver behavior (controlled)

- Measures only when environment telemetry is sent (mesh interval or phone sync)
- Each send: burst of 10 samples, averaged distance published
- Seeed nogps: DYP VCC on switched GNSS 3V3 (D18); rail off between bursts
- Heltec V4: DYP on Serial2 (GPIO5/6); GPS keeps Serial1

## Flash

- **Heltec V4:** `firmware-heltec-v4-*.factory.bin` via web flasher or esptool
- **Seeed Solar Node:** `firmware-seeed_solar_node_dyp_nogps-*.uf2` (double-tap USB) or `*-ota.zip` (nRF Connect)

## Versions

- `2.7.26/` — base `v2.7.26.54e0d8d`, protobuf `DYP_A01 = 54`
- `2.7.15/` — base `v2.7.15.405b2f5b9`, protobuf `DYP_A01 = 46`
EOF

echo "Release artifacts in $OUT"
find "$OUT" -type f \( -name '*.uf2' -o -name '*.bin' -o -name '*.zip' \) | sort
