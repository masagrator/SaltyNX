/**
 * @file switch_min.h
 * @brief Central Switch header. Includes all others.
 * @copyright libnx Authors
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "switch_min/types.h"
#include "switch_min/result.h"

#include "switch_min/nro.h"
#include "switch_min/nacp.h"

#include "switch_min/arm/tls.h"
#include "switch_min/arm/cache.h"
#include "switch_min/arm/atomics.h"
#include "switch_min/arm/counter.h"

#include "switch_min/kernel/svc.h"
#include "switch_min/kernel/wait.h"
#include "switch_min/kernel/tmem.h"
#include "switch_min/kernel/shmem.h"
#include "switch_min/kernel/mutex.h"
#include "switch_min/kernel/event.h"
#include "switch_min/kernel/uevent.h"
#include "switch_min/kernel/utimer.h"
#include "switch_min/kernel/rwlock.h"
#include "switch_min/kernel/condvar.h"
#include "switch_min/kernel/thread.h"
#include "switch_min/kernel/semaphore.h"
#include "switch_min/kernel/virtmem.h"
#include "switch_min/kernel/detect.h"
#include "switch_min/kernel/random.h"
#include "switch_min/kernel/jit.h"
#include "switch_min/kernel/ipc.h"
#include "switch_min/kernel/barrier.h"

#include "switch_min/services/sm.h"
//#include "switch_min/services/smm.h"
#include "switch_min/services/fs.h"
//#include "switch_min/services/fspr.h"
#include "switch_min/services/apm.h"
#include "switch_min/services/applet.h"
#include "switch_min/services/lbl.h"
//#include "switch_min/services/psm.h"
#include "switch_min/services/spsm.h"
//#include "switch_min/services/bsd.h" Use switch_min/runtime/devices/socket.h instead
#include "switch_min/services/fatal.h"
#include "switch_min/services/time.h"
#include "switch_min/services/hid.h"
#include "switch_min/services/hiddbg.h"
#include "switch_min/services/hidsys.h"
#include "switch_min/services/irs.h"
#include "switch_min/services/pl.h"
#include "switch_min/services/nifm.h"
//#include "switch_min/services/ns.h"
//#include "switch_min/services/ldr.h"
//#include "switch_min/services/ro.h"
#include "switch_min/services/pm.h"
#include "switch_min/services/set.h"
//#include "switch_min/services/ncm.h"
#include "switch_min/services/psc.h"
#include "switch_min/services/nfc.h"
#include "switch_min/services/wlaninf.h"
#include "switch_min/services/pctl.h"

#include "switch_min/applets/libapplet.h"
#include "switch_min/applets/pctlauth.h"
#include "switch_min/applets/error.h"
#include "switch_min/applets/swkbd.h"

#include "switch_min/runtime/env.h"
#include "switch_min/runtime/hosversion.h"

#include "switch_min/runtime/util/utf.h"

#include "switch_min/runtime/devices/console.h"
#include "switch_min/runtime/devices/fs_dev.h"
//#include "switch_min/runtime/devices/romfs_dev.h"
#include "switch_min/runtime/devices/socket.h"

/*
#include "switch_min/crypto/aes.h"
#include "switch_min/crypto/aes_cbc.h"
#include "switch_min/crypto/aes_ctr.h"
#include "switch_min/crypto/aes_xts.h"
#include "switch_min/crypto/cmac.h"

#include "switch_min/crypto/sha256.h"
#include "switch_min/crypto/sha1.h"
#include "switch_min/crypto/hmac.h"

#include "switch_min/crypto/crc.h"
*/
#ifdef __cplusplus
}
#endif

