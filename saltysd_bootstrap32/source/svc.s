.macro SVC_BEGIN name
	.section .text.\name, "ax", %progbits
	.global \name
	.type \name, %function
	.align 2
	.cfi_startproc
\name:
.endm

.macro SVC_END
	.cfi_endproc
.endm

SVC_BEGIN svcSetHeapSize
	push {r0}
	svc 0x1
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcSetMemoryPermission
	svc 0x2
	bx lr
SVC_END

SVC_BEGIN svcSetMemoryAttribute
	svc 0x3
	bx lr
SVC_END

SVC_BEGIN svcMapMemory
	svc 0x4
	bx lr
SVC_END

SVC_BEGIN svcUnmapMemory
	svc 0x5
	bx lr
SVC_END

SVC_BEGIN svcQueryMemory
	str r1, [sp, #-16]!
	svc 0x6
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcExitProcess
	svc 0x7
	bx lr
SVC_END

SVC_BEGIN svcCreateThread
	str r0, [sp, #-16]!
	svc 0x8
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcStartThread
	svc 0x9
	bx lr
SVC_END

SVC_BEGIN svcExitThread
	svc 0xA
	bx lr
SVC_END

SVC_BEGIN svcSleepThread
	svc 0xB
	bx lr
SVC_END

SVC_BEGIN svcGetThreadPriority
	str r0, [sp, #-16]!
	svc 0xC
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcSetThreadPriority
	svc 0xD
	bx lr
SVC_END

SVC_BEGIN svcSetThreadCoreMask
	svc 0xF
	bx lr
SVC_END

SVC_BEGIN svcGetCurrentProcessorNumber
	svc 0x10
	bx lr
SVC_END

SVC_BEGIN svcSignalEvent
	svc 0x11
	bx lr
SVC_END

SVC_BEGIN svcClearEvent
	svc 0x12
	bx lr
SVC_END

SVC_BEGIN svcMapSharedMemory
	svc 0x13
	bx lr
SVC_END

SVC_BEGIN svcUnmapSharedMemory
	svc 0x14
	bx lr
SVC_END

SVC_BEGIN svcCreateTransferMemory
	str r0, [sp, #-16]!
	svc 0x15
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcCloseHandle
	svc 0x16
	bx lr
SVC_END

SVC_BEGIN svcResetSignal
	svc 0x17
	bx lr
SVC_END

SVC_BEGIN svcWaitSynchronization
	str r0, [sp, #-16]!
	svc 0x18
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcCancelSynchronization
	svc 0x19
	bx lr
SVC_END

SVC_BEGIN svcArbitrateLock
	svc 0x1A
	bx lr
SVC_END

SVC_BEGIN svcArbitrateUnlock
	svc 0x1B
	bx lr
SVC_END

SVC_BEGIN svcWaitProcessWideKeyAtomic
	svc 0x1C
	bx lr
SVC_END

SVC_BEGIN svcSignalProcessWideKey
	svc 0x1D
	bx lr
SVC_END

SVC_BEGIN svcGetSystemTick
	svc 0x1E
	bx lr
SVC_END

SVC_BEGIN svcConnectToNamedPort
	push {r0}
	svc 0x1F
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcSendSyncRequest
	svc 0x21
	bx lr
SVC_END

SVC_BEGIN svcSendSyncRequestWithUserBuffer
	svc 0x22
	bx lr
SVC_END

SVC_BEGIN svcSendAsyncRequestWithUserBuffer
	str r0, [sp, #-16]!
	svc 0x23
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcGetProcessId
	str r0, [sp, #-16]!
	svc 0x24
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcGetThreadId
	str r0, [sp, #-16]!
	svc 0x25
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcBreak
	svc 0x26
	bx lr
SVC_END

SVC_BEGIN svcOutputDebugString
	svc 0x27
	bx lr
SVC_END

SVC_BEGIN svcReturnFromException
	svc 0x28
	bx lr
SVC_END

SVC_BEGIN svcGetInfo
	str r0, [sp, #-16]!
	svc 0x29
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcMapPhysicalMemory
	svc 0x2C
	bx lr
SVC_END

SVC_BEGIN svcUnmapPhysicalMemory
	svc 0x2D
	bx lr
SVC_END

SVC_BEGIN svcGetResourceLimitLimitValue
	str r0, [sp, #-16]!
	svc 0x30
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcGetResourceLimitCurrentValue
	str r0, [sp, #-16]!
	svc 0x31
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcSetThreadActivity
	svc 0x32
	bx lr
SVC_END

SVC_BEGIN svcGetThreadContext3
	svc 0x33
	bx lr
SVC_END

SVC_BEGIN svcAcceptSession
	str r0, [sp, #-16]!
	svc 0x41
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcReplyAndReceive
	str r0, [sp, #-16]!
	svc 0x43
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcReplyAndReceiveWithUserBuffer
	str r0, [sp, #-16]!
	svc 0x44
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcMapPhysicalMemoryUnsafe
	svc 0x48
	bx lr
SVC_END

SVC_BEGIN svcUnmapPhysicalMemoryUnsafe
	svc 0x49
	bx lr
SVC_END

SVC_BEGIN svcSetUnsafeLimit
	svc 0x4A
	bx lr
SVC_END

SVC_BEGIN svcCreateCodeMemory
	str r0, [sp, #-16]!
	svc 0x4B
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcControlCodeMemory
	svc 0x4C
	bx lr
SVC_END

SVC_BEGIN svcReadWriteRegister
	str r0, [sp, #-16]!
	svc 0x4E
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcCreateSharedMemory
	str r0, [sp, #-16]!
	svc 0x50
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcMapTransferMemory
	svc 0x51
	bx lr
SVC_END

SVC_BEGIN svcUnmapTransferMemory
	svc 0x52
	bx lr
SVC_END

SVC_BEGIN svcCreateInterruptEvent
	str r0, [sp, #-16]!
	svc 0x53
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcLegacyQueryIoMapping
	str r0, [sp, #-16]!
	svc 0x55
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcCreateDeviceAddressSpace
	str r0, [sp, #-16]!
	svc 0x56
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcAttachDeviceAddressSpace
	svc 0x57
	bx lr
SVC_END

SVC_BEGIN svcDetachDeviceAddressSpace
	svc 0x58
	bx lr
SVC_END

SVC_BEGIN svcMapDeviceAddressSpaceByForce
	svc 0x59
	bx lr
SVC_END

SVC_BEGIN svcMapDeviceAddressSpaceAligned
	svc 0x5A
	bx lr
SVC_END

SVC_BEGIN svcUnmapDeviceAddressSpace
	svc 0x5C
	bx lr
SVC_END

SVC_BEGIN svcDebugActiveProcess
	str r0, [sp, #-16]!
	svc 0x60
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcBreakDebugProcess
	svc 0x61
	bx lr
SVC_END

SVC_BEGIN svcTerminateDebugProcess
	svc 0x62
	bx lr
SVC_END

SVC_BEGIN svcGetDebugEvent
	svc 0x63
	bx lr
SVC_END

SVC_BEGIN svcLegacyContinueDebugEvent
	svc 0x64
	bx lr
SVC_END

SVC_BEGIN svcContinueDebugEvent
	svc 0x64
	bx lr
SVC_END

SVC_BEGIN svcGetProcessList
	str r0, [sp, #-16]!
	svc 0x65
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcGetThreadList
	str r0, [sp, #-16]!
	svc 0x66
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcGetDebugThreadContext
	svc 0x67
	bx lr
SVC_END

SVC_BEGIN svcSetDebugThreadContext
	svc 0x68
	bx lr
SVC_END

SVC_BEGIN svcQueryDebugProcessMemory
	str r1, [sp, #-16]!
	svc 0x69
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcReadDebugProcessMemory
	svc 0x6A
	bx lr
SVC_END

SVC_BEGIN svcWriteDebugProcessMemory
	svc 0x6B
	bx lr
SVC_END

SVC_BEGIN svcGetSystemInfo
	str r0, [sp, #-16]!
	svc 0x6F
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcManageNamedPort
	str r0, [sp, #-16]!
	svc 0x71
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcConnectToPort
	str r0, [sp, #-16]!
	svc 0x72
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcSetProcessMemoryPermission
	svc 0x73
	bx lr
SVC_END

SVC_BEGIN svcMapProcessMemory
	svc 0x74
	bx lr
SVC_END

SVC_BEGIN svcUnmapProcessMemory
	svc 0x75
	bx lr
SVC_END

SVC_BEGIN svcQueryProcessMemory
	str r1, [sp, #-16]!
	svc 0x76
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcMapProcessCodeMemory
	svc 0x77
	bx lr
SVC_END

SVC_BEGIN svcUnmapProcessCodeMemory
	svc 0x78
	bx lr
SVC_END

SVC_BEGIN svcCreateProcess
	str r0, [sp, #-16]!
	svc 0x79
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcStartProcess
	svc 0x7A
	bx lr
SVC_END

SVC_BEGIN svcTerminateProcess
	svc 0x7B
	bx lr
SVC_END

SVC_BEGIN svcGetProcessInfo
	str r0, [sp, #-16]!
	svc 0x7C
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcCreateResourceLimit
	str r0, [sp, #-16]!
	svc 0x7D
	ldr r2, [sp], #16
	str r1, [r2]
	bx lr
SVC_END

SVC_BEGIN svcSetResourceLimitLimitValue
	svc 0x7E
	bx lr
SVC_END