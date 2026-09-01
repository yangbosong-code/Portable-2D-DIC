#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
  echo "Run this installer as root: sudo deploy/jetson/install.sh" >&2
  exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${P2DIC_BUILD_DIR:-${project_root}/out/build/jetson-release}"

for executable in dic_edge dic_ctl; do
  if [[ ! -x "${build_dir}/${executable}" ]]; then
    echo "Missing ${build_dir}/${executable}; run tools/build-jetson.sh first." >&2
    exit 3
  fi
done

if ! getent group p2dic >/dev/null; then
  groupadd --system p2dic
fi
if ! id p2dic >/dev/null 2>&1; then
  useradd --system --gid p2dic --home-dir /var/lib/p2dic \
    --shell /usr/sbin/nologin p2dic
fi
for device_group in video plugdev; do
  if getent group "${device_group}" >/dev/null; then
    usermod --append --groups "${device_group}" p2dic
  fi
done

install -d -o root -g root -m 0755 /opt/p2dic/bin /etc/p2dic
install -d -o p2dic -g p2dic -m 0750 /var/lib/p2dic /var/lib/p2dic/sessions
install -o root -g root -m 0755 "${build_dir}/dic_edge" /opt/p2dic/bin/dic_edge
install -o root -g root -m 0755 "${build_dir}/dic_ctl" /opt/p2dic/bin/dic_ctl
install -o root -g root -m 0755 "${script_dir}/p2dic-healthcheck" /opt/p2dic/bin/p2dic-healthcheck

missing_libraries="$(ldd /opt/p2dic/bin/dic_edge | awk '/not found/ { print }')"
if [[ -n "${missing_libraries}" ]]; then
  echo "DIC Edge has unresolved runtime libraries:" >&2
  echo "${missing_libraries}" >&2
  echo "Install/configure the matching CUDA and GalaxySDK ARM64 runtime first." >&2
  exit 4
fi

if [[ ! -e /etc/p2dic/dic-edge.conf ]]; then
  install -o root -g p2dic -m 0640 \
    "${project_root}/config/dic-edge.jetson.conf" /etc/p2dic/dic-edge.conf
else
  echo "Keeping existing /etc/p2dic/dic-edge.conf"
fi

for unit in p2dic-edge.service p2dic-healthcheck.service \
            p2dic-healthcheck.timer p2dic-recover.service; do
  install -o root -g root -m 0644 "${script_dir}/systemd/${unit}" \
    "/etc/systemd/system/${unit}"
done

systemctl daemon-reload
systemctl enable p2dic-edge.service p2dic-healthcheck.timer
systemctl restart p2dic-edge.service
systemctl restart p2dic-healthcheck.timer

echo "Portable 2D-DIC Edge installed."
echo "Status:  systemctl status p2dic-edge.service"
echo "Logs:    journalctl -u p2dic-edge.service -f"
