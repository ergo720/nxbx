#pragma once

#include <cstdint>

// Object graphics classes
#define NV01_CONTEXT_DMA_FROM_MEMORY                     0x00000002
#define NV01_CONTEXT_DMA_TO_MEMORY                       0x00000003
#define NV01_CONTEXT_DMA_IN_MEMORY                       0x0000003D
#define NV03_MEMORY_TO_MEMORY_FORMAT                     0x00000039
#define NV10_CONTEXT_SURFACES_2D                         0x00000062
#define NV20_KELVIN_PRIMITIVE                            0x00000097
#define NV15_IMAGE_BLIT                                  0x0000009F
#define HIGHEST_CLASS                                    NV15_IMAGE_BLIT

// Object methods
#define INC_MTHD(name, base, i) name ## i = ((base)+(i)*4)

enum class nv039 : uint32_t
{
	NV039_SET_OBJECT =                                   0x00000000,
	NV039_SET_CONTEXT_DMA_NOTIFIES =                     0x00000180,
};

enum class nv062 : uint32_t
{
	NV062_SET_OBJECT =                                   0x00000000,
};

enum class nv097 : uint32_t
{
	NV097_SET_OBJECT =                                   0x00000000,
};

enum class nv09f : uint32_t
{
	NV09F_SET_OBJECT =                                   0x00000000,
};

// Classes declarations
#define NV039_NOTIFIERS_NOTIFY                           0
#define NV039_NOTIFIERS_BUFFER_NOTIFY                    1
#define NV039_NOTIFICATION_STATUS_IN_PROGRESS            0x8000
#define NV039_NOTIFICATION_STATUS_ERROR_PROTECTION_FAULT 0x4000
#define NV039_NOTIFICATION_STATUS_ERROR_BAD_ARGUMENT     0x2000
#define NV039_NOTIFICATION_STATUS_ERROR_INVALID_STATE    0x1000
#define NV039_NOTIFICATION_STATUS_ERROR_STATE_IN_USE     0x0800
#define NV039_NOTIFICATION_STATUS_DONE_SUCCESS           0x0000
