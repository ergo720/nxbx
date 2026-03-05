// SPDX-License-Identifier: GPL-3.0-only

// SPDX-FileCopyrightText: 2025 ergo720

#pragma once

#define REVISION (USB0_BASE + 0x00)

#define CTRL (USB0_BASE + 0x04)
#define CTRL_HCFS (3 << 6) // HostControllerFunctionalState

#define CMD_ST (USB0_BASE + 0x08)
#define CMD_ST_HCR (1 << 0) // SchedulingOverrunCount
#define CMD_ST_SOC (3 << 16) // SchedulingOverrunCount
#define CMD_ST_RO_MASK CMD_ST_SOC

#define INT_ST (USB0_BASE + 0x0C)
#define INT_SO (1 << 0) // SchedulingOverrun
#define INT_WD (1 << 1) // WritebackDoneHead
#define INT_SF (1 << 2) // StartofFrame
#define INT_RD (1 << 3) // ResumeDetected
#define INT_UE (1 << 4) // UnrecoverableError
#define INT_FNO (1 << 5) // FrameNumberOverflow
#define INT_RHSC (1 << 6) // RootHubStatusChange
#define INT_OC (1 << 30) // OwnershipChange
#define INT_ALL (INT_SO | INT_WD | INT_SF | INT_RD | INT_UE | INT_FNO | INT_RHSC | INT_OC)

#define INT_EN (USB0_BASE + 0x10)
#define INT_MIE (1 << 31) // MasterInterruptEnable

#define INT_DIS (USB0_BASE + 0x14)

#define HCCA (USB0_BASE + 0x18)
#define HCCA_RO_MASK 0xFF

#define PERIOD_CURR_ED (USB0_BASE + 0x1C)
#define ED_RO_MASK 0xF // valid for all "ED" registers

#define CTRL_HEAD_ED (USB0_BASE + 0x20)

#define CTRL_CURR_ED (USB0_BASE + 0x24)

#define BULK_HEAD_ED (USB0_BASE + 0x28)

#define BULK_CURR_ED (USB0_BASE + 0x2C)

#define DONE_HEAD (USB0_BASE + 0x30)

#define FM_INTERVAL (USB0_BASE + 0x34)
#define FM_INTERVAL_FI 0x3FFF // FrameInterval

#define FM_REMAINING (USB0_BASE + 0x38)
#define FM_REMAINING_FRT (1 << 31) // FrameRemainingToggle

#define FM_NUM (USB0_BASE + 0x3C)

#define PERIOD_START (USB0_BASE + 0x40)

#define LS_THRESHOLD (USB0_BASE + 0x44)

#define RH_DESCRIPTOR_A (USB0_BASE + 0x48)
#define RHDA_NDP 0xFF // NumberDownstreamPorts
#define RHDA_PSM (1 << 8) // PowerSwitchingMode
#define RHDA_NPS (1 << 9) // NoPowerSwitching
#define RHDA_DT (1 << 10) // DeviceType
#define RHDA_NOCP (1 << 12) // NoOverCurrentProtection
#define RHDA_RO_MASK (RHDA_DT | RHDA_NDP)

#define RH_DESCRIPTOR_B (USB0_BASE + 0x4C)
#define RHDB_PPCM(i) (1 << (17 + i)) // PortPowerControlMask

#define RH_ST (USB0_BASE + 0x50)
#define RH_ST_LPS (1 << 0) // LocalPowerStatus, ClearGlobalPower
#define RH_ST_OCI (1 << 1) // OverCurrentIndicator
#define RH_ST_DRWE (1 << 15) // DeviceRemoteWakeupEnable
#define RH_ST_LPSC (1 << 16) // LocalPowerStatusChange, SetGlobalPower
#define RH_ST_CRWE (1 << 31) // ClearRemoteWakeupEnable

#define RH_PORT_ST(i) (USB0_BASE + 0x54 + i * 4)
#define RH_PORT_ST_PPS (1 << 8) // PortPowerStatus
