// Copyright (C) 2017-2024 Intel Corporation
// SPDX-License-Identifier: MIT
//
// This file originally generated by etw_list
//     version:    public 1b19f39ddb669f7a700a5d0c16cf079943e996d5
//     parameters: --no_event_structs --event=ProcessStart::Start --event=ProcessStop::Stop --provider=Microsoft-Windows-Kernel-Process
#pragma once

namespace Microsoft_Windows_Kernel_Process {

struct __declspec(uuid("{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}")) GUID_STRUCT;
static const auto GUID = __uuidof(GUID_STRUCT);

enum class Keyword : uint64_t {
    WINEVENT_KEYWORD_PROCESS                          = 0x10,
    WINEVENT_KEYWORD_THREAD                           = 0x20,
    WINEVENT_KEYWORD_IMAGE                            = 0x40,
    WINEVENT_KEYWORD_CPU_PRIORITY                     = 0x80,
    WINEVENT_KEYWORD_OTHER_PRIORITY                   = 0x100,
    WINEVENT_KEYWORD_PROCESS_FREEZE                   = 0x200,
    WINEVENT_KEYWORD_JOB                              = 0x400,
    WINEVENT_KEYWORD_ENABLE_PROCESS_TRACING_CALLBACKS = 0x800,
    WINEVENT_KEYWORD_JOB_IO                           = 0x1000,
    WINEVENT_KEYWORD_WORK_ON_BEHALF                   = 0x2000,
    WINEVENT_KEYWORD_JOB_SILO                         = 0x4000,
    Microsoft_Windows_Kernel_Process_Analytic         = 0x8000000000000000,
};

enum class Level : uint8_t {
    win_Informational = 0x4,
};

enum class Channel : uint8_t {
    Microsoft_Windows_Kernel_Process_Analytic = 0x10,
};

// Event descriptors:
#define EVENT_DESCRIPTOR_DECL(name_, id_, version_, channel_, level_, opcode_, task_, keyword_) struct name_ { \
    static uint16_t const Id      = id_; \
    static uint8_t  const Version = version_; \
    static uint8_t  const Channel = channel_; \
    static uint8_t  const Level   = level_; \
    static uint8_t  const Opcode  = opcode_; \
    static uint16_t const Task    = task_; \
    static Keyword  const Keyword = (Keyword) keyword_; \
};

EVENT_DESCRIPTOR_DECL(ProcessStart_Start, 0x0001, 0x03, 0x10, 0x04, 0x01, 0x0001, 0x8000000000000010)
EVENT_DESCRIPTOR_DECL(ProcessStop_Stop  , 0x0002, 0x02, 0x10, 0x04, 0x02, 0x0002, 0x8000000000000010)

#undef EVENT_DESCRIPTOR_DECL

enum class ProcessFlags : uint32_t {
    PackageId = 1,
};

}
