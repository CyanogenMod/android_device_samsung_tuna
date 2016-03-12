#!/system/bin/sh

DEVICE="/dev/block/platform/omap/omap_hsmmc.0/by-name/dgs"

log_to_kernel() {
  echo "$*" > /dev/kmsg
}

create_tee_fs() {
    make_ext4fs -J -b 4096 ${DEVICE} || exit 1
    mount -t ext4 ${DEVICE} /tee || exit 1
    mkdir /tee/smc || exit 1
    chmod 0770 /tee/smc || exit 1
    chown drmrpc:drmrpc /tee/smc || exit 1
    restorecon -R /tee/smc || exit 1
}

if [ ! -e /tee/smc ]; then
  # sha1 hash of the empty 4MB partition.
  EXPECTED_HASH="2bccbd2f38f15c13eb7d5a89fd9d85f595e23bc3"
  ACTUAL_HASH="`/system/bin/sha1sum ${DEVICE}`"
  if [ "${ACTUAL_HASH}" == "${EXPECTED_HASH}  ${DEVICE}" ]; then
    if create_tee_fs > /dev/kmsg 2>&1; then
      log_to_kernel "tee-fs-setup: successfully initialized /tee for SMC, rebooting."
      # tf_daemon gets stuck when started after FS initialization,
      # but works fine after reboot.
      mount -t ext4 -o remount,ro /tee
      reboot
    else
      log_to_kernel "tee-fs-setup: initialization of /tee for SMC failed. SMC won't function!"
    fi
  else
    log_to_kernel "tee-fs-setup: unexpected hash '${ACTUAL_HASH}', skipping /tee filesystem creation. SMC won't function!"
  fi
else
  log_to_kernel "tee-fs-setup: /tee is already initialized for SMC, nothing to do."
  setprop init.tee_fs.ready true
fi

exit 0
