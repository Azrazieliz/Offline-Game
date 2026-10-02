#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UE_IMAGE="${UE_IMAGE:-ghcr.io/epicgames/unreal-engine:dev-slim-5.8.0}"
REPORT_DIR="${ROOT}/Saved/Automation/Reports"

python3 "${ROOT}/Scripts/verify_unreal_cpp.py"

if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: Docker is required." >&2
  exit 2
fi

if ! docker info >/dev/null 2>&1; then
  echo "ERROR: Docker daemon is not available." >&2
  exit 2
fi

echo "Host disk before Unreal image pull:"
df -h "${ROOT}" || true
docker system df || true

if ! docker image inspect "${UE_IMAGE}" >/dev/null 2>&1; then
  GH_USER="${GITHUB_USER:-}"
  GH_TOKEN="${CR_PAT:-${GHCR_TOKEN:-}}"

  if command -v gh >/dev/null 2>&1; then
    if [[ -z "${GH_USER}" ]]; then
      GH_USER="$(gh api user --jq .login 2>/dev/null || true)"
    fi

    if [[ -z "${GH_TOKEN}" ]]; then
      GH_TOKEN="$(gh auth token 2>/dev/null || true)"
    fi
  fi

  if [[ -z "${GH_USER}" || -z "${GH_TOKEN}" ]]; then
    cat >&2 <<'EOF'
ERROR: Unreal's private GHCR image requires GitHub authentication.
Set CR_PAT (or GHCR_TOKEN) to a GitHub personal access token with read:packages.
The GitHub account must have EpicGames/UnrealEngine access.
EOF
    exit 3
  fi

  echo "${GH_TOKEN}" | docker login ghcr.io -u "${GH_USER}" --password-stdin

  echo "Pulling ${UE_IMAGE} ..."
  docker pull "${UE_IMAGE}"
fi

mkdir -p "${REPORT_DIR}"
rm -rf "${REPORT_DIR:?}/"*

echo "Host disk after Unreal image pull:"
df -h "${ROOT}" || true
docker system df || true

docker run --rm --init --shm-size=2g   -v "${ROOT}:/project"   -w /project   "${UE_IMAGE}"   bash -lc '
    set -euo pipefail

    ENGINE_ROOT=/home/ue4/UnrealEngine
    PROJECT=/project/OfflineGame.uproject
    REPORT=/project/Saved/Automation/Reports

    echo "=== Unreal project compile ==="
    "${ENGINE_ROOT}/Engine/Build/BatchFiles/Linux/Build.sh"       OfflineGameEditor       Linux       Development       "${PROJECT}"       -WaitMutex       -NoHotReloadFromIDE

    echo "=== OfflineGame automation tests ==="
    mkdir -p "${REPORT}"

    "${ENGINE_ROOT}/Engine/Binaries/Linux/UnrealEditor-Cmd"       "${PROJECT}"       -unattended       -nop4       -nosplash       -NullRHI       -stdout       -FullStdOutLogOutput       -ExecCmds="Automation RunTest OfflineGame;Quit"       -TestExit="Automation Test Queue Empty"       -ReportExportPath="${REPORT}"
  '

echo "Unreal compile + OfflineGame automation tests completed."
echo "Reports: ${REPORT_DIR}"
