#!/bin/bash
for i in $(seq 1 10); do
  [ -b /dev/nvme0n1p1 ] && break
  sleep 1
done


if ! mountpoint -q /data/media/0/realdata; then
  mount /dev/nvme0n1p1 /data/media/0/realdata
fi

if mountpoint -q /data/media/0/realdata; then
  OWNER="$(stat -c '%U' /data/media/0/realdata)"
  GROUP="$(stat -c '%G' /data/media/0/realdata)"
  PERM="$(stat -c '%a' /data/media/0/realdata)"

  if [ "$OWNER" != "comma" ] || [ "$GROUP" != "comma" ]; then
    chown comma:comma /data/media/0/realdata
  fi

  if [ "$PERM" != "755" ]; then
    chmod 755 /data/media/0/realdata
  fi
fi
