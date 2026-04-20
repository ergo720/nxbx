#pragma once

#include <cstdint>

// Object graphics classes
#define NV01_CONTEXT_DMA_FROM_MEMORY                     0x00000002
#define NV01_CONTEXT_DMA_TO_MEMORY                       0x00000003
#define NV01_CONTEXT_BETA                                0x00000012
#define NV01_CONTEXT_CLIP_RECTANGLE                      0x00000019
#define NV01_NULL                                        0x00000030
#define NV01_CONTEXT_DMA_IN_MEMORY                       0x0000003D
#define NV03_MEMORY_TO_MEMORY_FORMAT                     0x00000039
#define NV03_CONTEXT_ROP                                 0x00000043
#define NV04_CONTEXT_PATTERN                             0x00000044
#define NV04_CONTEXT_COLOR_KEY                           0x00000057
#define NV10_CONTEXT_SURFACES_2D                         0x00000062
#define NV04_CONTEXT_BETA                                0x00000072
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
	NV062_SET_CONTEXT_DMA_IMAGE_SOURCE =                 0x00000184,
	NV062_SET_CONTEXT_DMA_IMAGE_DESTIN =                 0x00000188,
};

enum class nv097 : uint32_t
{
	NV097_SET_OBJECT =                                   0x00000000,
	NV097_SET_CONTEXT_DMA_NOTIFIES =                     0x00000180,
	NV097_SET_CONTEXT_DMA_A =                            0x00000184,
	NV097_SET_CONTEXT_DMA_B =                            0x00000188,
	NV097_SET_CONTEXT_DMA_STATE =                        0x00000190,
	NV097_SET_CONTEXT_DMA_COLOR =                        0x00000194,
	NV097_SET_CONTEXT_DMA_ZETA =                         0x00000198,
	NV097_SET_CONTEXT_DMA_VERTEX_A =                     0x0000019C,
	NV097_SET_CONTEXT_DMA_VERTEX_B =                     0x000001A0,
	NV097_SET_CONTEXT_DMA_SEMAPHORE =                    0x000001A4,
	NV097_SET_CONTEXT_DMA_REPORT =                       0x000001A8,
	NV097_SET_FLAT_SHADE_OP =                            0x000009FC,
	NV097_SET_SEMAPHORE_OFFSET =                         0x00001D6C,
	NV097_BACK_END_WRITE_SEMAPHORE_RELEASE =             0x00001D70,
};

enum class nv09f : uint32_t
{
	NV09F_SET_OBJECT =                                   0x00000000,
	NV09F_SET_CONTEXT_COLOR_KEY =                        0x00000184,
	NV09F_SET_CONTEXT_CLIP_RECTANGLE =                   0x00000188,
	NV09F_SET_CONTEXT_PATTERN =                          0x0000018C,
	NV09F_SET_CONTEXT_ROP =                              0x00000190,
	NV09F_SET_CONTEXT_BETA1 =                            0x00000194,
	NV09F_SET_CONTEXT_BETA4 =                            0x00000198,
	NV09F_SET_CONTEXT_SURFACES =                         0x0000019C,
	NV09F_SET_OPERATION =                                0x000002FC,
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

#define NV097_NOTIFICATION_STATUS_IN_PROGRESS            0x8000
#define NV097_NOTIFICATION_STATUS_ERROR_PROTECTION_FAULT 0x4000
#define NV097_NOTIFICATION_STATUS_ERROR_BAD_ARGUMENT     0x2000
#define NV097_NOTIFICATION_STATUS_ERROR_INVALID_STATE    0x1000
#define NV097_NOTIFICATION_STATUS_ERROR_STATE_IN_USE     0x0800
#define NV097_NOTIFICATION_STATUS_DONE_SUCCESS           0x0000

#define NV097_SET_FLAT_SHADE_OP_V_LAST_VTX               0x00000000
#define NV097_SET_FLAT_SHADE_OP_V_FIRST_VTX              0x00000001
