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
	push {r1}
	svc 0x6
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcExitProcess
	svc 0x7
	bx lr
SVC_END

SVC_BEGIN svcCreateThread
	push {r0,r4}
	ldr r0, [sp, #8]
	ldr r4, [sp, #0xc]
	svc 0x8
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	pop {r4}
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
	push {r0}
	svc 0xC
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
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
	push {r0}
	svc 0x15
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
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
	push {r0}
	ldr r0, [sp, #4]
	ldr r3, [sp, #8]
	svc 0x18
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
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
	push {r4}
	ldr r3, [sp, #4]
	ldr r4, [sp, #8]
	svc 0x1C
	pop {r4}
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
	push {r0}
	svc 0x23
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcGetProcessId
	push {r0}
	svc 0x24
	ldr r3, [sp]
	str r1, [r3]
	str r2, [r3, #4]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcGetThreadId
	push {r0}
	svc 0x25
	ldr r3, [sp]
	str r1, [r3]
	str r2, [r3, #4]
	add sp, sp, #4
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
	push {r0}
	ldr r0, [sp, #4]
	ldr r3, [sp, #8]
	svc 0x29
	ldr r3, [sp]
	str r1, [r3]
	str r2, [r3, #4]
	add sp, sp, #4
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
	push {r0}
	svc 0x30
	ldr r3, [sp]
	str r1, [r3]
	str r2, [r3, #4]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcGetResourceLimitCurrentValue
	push {r0}
	svc 0x31
	ldr r3, [sp]
	str r1, [r3]
	str r2, [r3, #4]
	add sp, sp, #4
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
	push {r0}
	svc 0x41
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcReplyAndReceive
	push {r0, r4}
	ldr r1, [sp, #4]
	ldr r4, [sp, #8]
	svc 0x43
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	pop {r4}
	bx lr
SVC_END

SVC_BEGIN svcReplyAndReceiveWithUserBuffer
	push {r0, r4-r6}
	ldr r0, [sp, #0x10]
	ldr r4, [sp, #0x14]
	ldr r5, [sp, #0x18]
	ldr r6, [sp, #0x1c]
	svc 0x44
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	pop {r4-r6}
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
	push {r0}
	svc 0x4B
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcControlCodeMemory
	svc 0x4C
	bx lr
SVC_END

SVC_BEGIN svcReadWriteRegister
	push {r0}
	ldr r0, [sp, #4]
	ldr r1, [sp, #8]
	svc 0x4E
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
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
	push {r0}
	svc 0x53
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcLegacyQueryIoMapping
	push {r0}
	ldr r0, [sp, #4]
	svc 0x55
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcCreateDeviceAddressSpace
	push {r0}
	ldr r0, [sp, #4]
	ldr r1, [sp, #8]
	svc 0x56
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
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
	push {r4-r7}
	ldr r4, [sp, #0x10]
	ldr r5, [sp, #0x18]
	ldr r6, [sp, #0x1c]
	ldr r7, [sp, #0x20]
	svc 0x59
	pop {r4-r7}
	bx lr
SVC_END

SVC_BEGIN svcMapDeviceAddressSpaceAligned
	push {r4-r7}
	ldr r4, [sp, #0x10]
	ldr r5, [sp, #0x18]
	ldr r6, [sp, #0x1c]
	ldr r7, [sp, #0x20]
	svc 0x5A
	pop {r4-r7}
	bx lr
SVC_END

SVC_BEGIN svcUnmapDeviceAddressSpace
	push {r4-r6}
	ldr r4, [sp, #0xc]
	ldr r5, [sp, #0x14]
	ldr r6, [sp, #0x18]
	svc 0x5C
	pop {r4-r6}
	bx lr
SVC_END

SVC_BEGIN svcDebugActiveProcess
	push {r0}
	svc 0x60
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
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
	push {r0}
	svc 0x65
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcGetThreadList
	push {r0}
	svc 0x66
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcGetDebugThreadContext
	push {r4}
	ldr r4, [sp, #4]
	svc 0x67
	pop {r4}
	bx lr
SVC_END

SVC_BEGIN svcSetDebugThreadContext
	PUSH {r4}
	ldr r1, [sp, #4]
	ldr r4, [sp, #8]
	svc 0x68
	pop {r4}
	bx lr
SVC_END

SVC_BEGIN svcQueryDebugProcessMemory
	push {r1}
	svc 0x69
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
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
	push {r0}
	svc 0x6F
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcManageNamedPort
	push {r0}
	svc 0x71
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcConnectToPort
	push {r0}
	svc 0x72
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcSetProcessMemoryPermission
	push {r4, r5}
	ldr r1, [sp, #8]
	ldr r4, [sp, #0xc]
	ldr r5, [sp, #0x10]
	svc 0x73
	pop {r4, r5}
	bx lr
SVC_END

SVC_BEGIN svcMapProcessMemory
	push {r4}
	ldr r4, [sp, #4]
	svc 0x74
	pop {r4}
	bx lr
SVC_END

SVC_BEGIN svcUnmapProcessMemory
	push {r4}
	ldr r4, [sp, #4]
	svc 0x75
	pop {r4}
	bx lr
SVC_END

SVC_BEGIN svcQueryProcessMemory
	push {r1}
	ldr r1, [sp, #4]
	ldr r3, [sp, #8]
	svc 0x76
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcMapProcessCodeMemory
	push {r4-r6}
	ldr r1, [sp, #0xc]
	ldr r4, [sp, #0x10]
	ldr r5, [sp, #0x14]
	ldr r6, [sp, #0x18]
	svc 0x77
	pop {r4-r6}
	bx lr
SVC_END

SVC_BEGIN svcUnmapProcessCodeMemory
	push {r4-r6}
	ldr r1, [sp, #0xc]
	ldr r4, [sp, #0x10]
	ldr r5, [sp, #0x14]
	ldr r6, [sp, #0x18]
	svc 0x78
	pop {r4-r6}
	bx lr
SVC_END

SVC_BEGIN svcCreateProcess
	push {r0}
	svc 0x79
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcStartProcess
	push {r4}
	ldr r3, [sp, #4]
	ldr r4, [sp, #8]
	svc 0x7A
	pop {r4}
	bx lr
SVC_END

SVC_BEGIN svcTerminateProcess
	svc 0x7B
	bx lr
SVC_END

SVC_BEGIN svcGetProcessInfo
	push {r0}
	svc 0x7C
	ldr r3, [sp]
	str r1, [r3]
	str r2, [r3, #4]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcCreateResourceLimit
	push {r0}
	svc 0x7D
	ldr r2, [sp]
	str r1, [r2]
	add sp, sp, #4
	bx lr
SVC_END

SVC_BEGIN svcSetResourceLimitLimitValue
	svc 0x7E
	bx lr
SVC_END