#!/usr/bin/env bash
set -euo pipefail

# 0. Check & Install Host Dependencies (Debian/Ubuntu)
for cmd in qemu-system-x86_64 qemu-img expect xz curl ssh-keygen; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "[*] Installing required host packages (qemu, expect, xz, curl)..."
        sudo apt-get update && sudo apt-get install -y \
            qemu-system-x86 qemu-utils expect xz-utils curl openssh-client
        break
    fi
done

TAR_FILE="${1:?Usage: $0 <submission.tar>}"
CACHE="${CACHE_DIR:-$HOME/.freebsd-cache}"
PORT="${SSH_PORT:-2222}"
REL="14.5-RELEASE"

mkdir -p "$CACHE"
BASE_IMG="$CACHE/freebsd-$REL.qcow2"
OVERLAY="$CACHE/overlay-$$.qcow2"
KEY="$CACHE/id_ed25519"
HOST_IP="10.0.2.2"

ACCEL=(-cpu max)
[[ -w /dev/kvm ]] && ACCEL=(-enable-kvm -cpu host)

cleanup() {
    if [[ -n "${QEMU_PID:-}" ]]; then
        kill "$QEMU_PID" 2>/dev/null || true
        wait "$QEMU_PID" 2>/dev/null || true
        QEMU_PID=""
    fi
    rm -f "$OVERLAY"
}
trap cleanup EXIT

# 1. Download & Provision Cached Base Image (Runs Once)
if [[ ! -f "$BASE_IMG" ]]; then
    echo "[*] Downloading FreeBSD $REL image and kernel headers (one-time)..."
    [[ -f "$CACHE/img.xz" ]]  || curl -fL "https://download.freebsd.org/releases/VM-IMAGES/$REL/amd64/Latest/FreeBSD-$REL-amd64-ufs.qcow2.xz" -o "$CACHE/img.xz"
    [[ -f "$CACHE/src.txz" ]] || curl -fL "https://download.freebsd.org/releases/amd64/$REL/src.txz" -o "$CACHE/src.txz"

    echo "[*] Extracting base image..."
    xz -dkc "$CACHE/img.xz" > "$BASE_IMG.tmp"
    qemu-img resize "$BASE_IMG.tmp" 10G
    rm -f "$KEY" "$KEY.pub" && ssh-keygen -t ed25519 -N "" -f "$KEY" -q
    PUBKEY=$(cat "$KEY.pub")

    echo "[*] Provisioning base VM over serial console..."
    expect <<EOF
    set timeout 300
    spawn qemu-system-x86_64 ${ACCEL[*]} -m 2048M -smp 2 -nographic \
        -drive file=$BASE_IMG.tmp,format=qcow2,if=virtio \
        -drive file=$CACHE/src.txz,format=raw,if=virtio,readonly=on

    expect {
        -re "Autoboot in|Welcome to FreeBSD" {
            send "5"
            sleep 1
            send "\r"
        }
        "Cons: Video" {
            send "5\r"
        }
        timeout {
            send "\r"
        }
    }

    expect "login:"
    send "root\r"

    expect "#"
    send "service growfs onestart\r"

    expect "#"
    send "tar -xJf /dev/vtbd1 -C / usr/src/sys usr/src/share/mk\r"

    expect "#"
    send "echo 'freebsd' | pw usermod root -h 0\r"

    expect "#"
    send "pw useradd student -m -G wheel -s /bin/sh && echo 'freebsd' | pw usermod student -h 0\r"

    expect "#"
    send "sysrc sshd_enable=YES\r"

    expect "#"
    send "printf 'PermitRootLogin yes\\\nPasswordAuthentication yes\\\n' >> /etc/ssh/sshd_config\r"

    # Install SSH keys for root with strict 700/600 permissions
    expect "#"
    send "mkdir -p /root/.ssh && chmod 700 /root/.ssh && echo '$PUBKEY' > /root/.ssh/authorized_keys && chmod 600 /root/.ssh/authorized_keys\r"

    # Install SSH keys for student with strict 700/600 permissions and enforce strict home ownership
    expect "#"
    send "mkdir -p /home/student/.ssh && echo '$PUBKEY' > /home/student/.ssh/authorized_keys\r"
    expect "#"
    send "chown -R student:student /home/student\r"
    expect "#"
    send "chmod 750 /home/student && chmod 700 /home/student/.ssh && chmod 600 /home/student/.ssh/authorized_keys\r"

    expect "#"
    send "printf 'autoboot_delay=\"1\"\\\nconsole=\"comconsole,vidconsole\"\\\n' >> /boot/loader.conf\r"

    expect "#"
    send "poweroff\r"

    expect eof
EOF
    mv "$BASE_IMG.tmp" "$BASE_IMG"
    echo "[+] Base image provisioning complete!"
fi

# 2. Boot Background QEMU VM with Disposable Overlay
qemu-img create -f qcow2 -b "$BASE_IMG" -F qcow2 "$OVERLAY" >/dev/null
qemu-system-x86_64 "${ACCEL[@]}" -m 2048M -smp 2 \
    -drive "file=$OVERLAY,format=qcow2,if=virtio" \
    -netdev "user,id=n0,hostfwd=tcp:127.0.0.1:$PORT-:22" -device virtio-net-pci,netdev=n0 \
    -display none -serial stdio -monitor none </dev/null &
QEMU_PID=$!

SSH_OPTS=(-i "$KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR)
until ssh "${SSH_OPTS[@]}" -p "$PORT" root@127.0.0.1 true 2>/dev/null; do sleep 1; done

# 3. Copy Tarball to both Root and Student Home Directories
scp "${SSH_OPTS[@]}" -P "$PORT" "$TAR_FILE" root@127.0.0.1:/root/submission.tar
scp "${SSH_OPTS[@]}" -P "$PORT" "$TAR_FILE" root@127.0.0.1:/home/student/submission.tar

# 4. Execute ./install strictly from the root of /root/work
echo "[*] Extracting and running ./install as root..."
ssh "${SSH_OPTS[@]}" -p "$PORT" root@127.0.0.1 "
    rm -rf /root/work && mkdir -p /root/work && tar -xf /root/submission.tar -C /root/work
    rm -rf /home/student/work && mkdir -p /home/student/work && tar -xf /home/student/submission.tar -C /home/student/work
    chown -R student:student /home/student
    
    cd /root/work
    chmod +x ./install
    ./install \"$HOST_IP\"
"

# 5. Print Instructions Banner and Drop into Interactive Shell
echo ""
echo "========================================================================"
echo "💀 ROOTKIT LAB ENVIRONMENT READY"
echo "========================================================================"
echo "  [✓] 1. Rootkit compiled and installed as root"
echo "  [✓] 2. Source tar copied to /home/student/work and /root/work"
echo "  [→] 3. Starting interactive session as 'student'..."
echo ""
echo "  📋 Credentials & Environment Details:"
echo "      • Current User   : student (password: freebsd)"
echo "      • Root Account   : root    (password: freebsd, or run 'su -')"
echo "      • Host IP (LAN)  : $HOST_IP (use for reverse shells/listeners)"
echo "      • Working Dir    : /home/student/work"
echo ""
echo "  ⚡ To end your test session and destroy this VM, type: exit"
echo "========================================================================"
echo ""

# Log in interactively as the unprivileged student user inside /home/student/work
ssh -t "${SSH_OPTS[@]}" -o IdentitiesOnly=yes -o PubkeyAuthentication=yes -p "$PORT" student@127.0.0.1 "cd /home/student/work; exec /bin/sh" || true

# 6. Teardown
echo ""
echo "[*] Shutting down VM and removing temporary overlay..."
ssh "${SSH_OPTS[@]}" -p "$PORT" root@127.0.0.1 "poweroff" 2>/dev/null || true
for attempt in {1..15}; do
    kill -0 "$QEMU_PID" 2>/dev/null || break
    sleep 1
done
if kill -0 "$QEMU_PID" 2>/dev/null; then
    echo "[*] Guest did not stop in time; terminating QEMU..."
    kill "$QEMU_PID" 2>/dev/null || true
fi
wait "$QEMU_PID" 2>/dev/null || true
QEMU_PID=""
rm -f "$OVERLAY"
