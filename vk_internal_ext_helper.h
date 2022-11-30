/*
 ******************************************************************************
 *
 *  Trade secret of Advanced Micro Devices, Inc.
 *  Copyright (c) 2014-2015, Advanced Micro Devices, Inc., (unpublished)
 *
 *  All rights reserved. This notice is intended as a precaution against
 *  inadvertent publication and does not imply publication or any waiver
 *  of confidentiality. The year included in the foregoing notice is the
 *  year of creation of the work.
 *
 ******************************************************************************
/**
 ******************************************************************************
 * @file  vk_internal_ext_helper.h
 * @brief Helper header for unpublished extensions.
 ******************************************************************************
 */

#ifndef VK_INTERNAL_EXT_HELPER_H_
#define VK_INTERNAL_EXT_HELPER_H_

#define VK_EXTENSION_ENUM_BASE_VALUE        1000000000ull
#define VK_EXTENSION_ENUM_RANGE_SIZE        1000ull

#define VK_EXTENSION_ENUM(extnr, type, offset) \
	((type)(VK_EXTENSION_ENUM_BASE_VALUE + (((extnr)-1) * \
	VK_EXTENSION_ENUM_RANGE_SIZE) + (offset)))

#define VK_EXTENSION_BIT(type, bit) \
	((type)(1 << (bit)))

#endif /* VK_INTERNAL_EXT_HELPER_H_ */
