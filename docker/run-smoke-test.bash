#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)
COMPOSE_FILE="${SCRIPT_DIR}/docker-compose.yml"

cd "${REPO_ROOT}"

docker compose -f "${COMPOSE_FILE}" build
docker compose -f "${COMPOSE_FILE}" run --rm --no-deps hakoniwa-pdu-foxglove \
  ctest --test-dir build --output-on-failure

echo "Starting CDR publisher on ws://localhost:8765"
echo "Open Foxglove on the host and connect to ws://localhost:8765"
docker compose -f "${COMPOSE_FILE}" up --force-recreate hakoniwa-pdu-foxglove

