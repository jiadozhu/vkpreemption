/*
 ******************************************************************************
 *
 *  Trade secret of Advanced Micro Devices, Inc.
 *  Copyright (c) 2014-2019, Advanced Micro Devices, Inc., (unpublished)
 *
 *  All rights reserved. This notice is intended as a precaution against
 *  inadvertent publication and does not imply
 *  publication or any waiver of confidentiality. The year included in the
 *  foregoing notice is the year of creation of the work.
 *
 *****************************************************************************/
#ifndef VK_AMD_DISPATCH_TUNNEL_H_
#define VK_AMD_DISPATCH_TUNNEL_H_

#include "vk_internal_ext_helper.h"

#define VK_AMD_DISPATCH_TUNNEL                             1
#define VK_AMD_DISPATCH_TUNNEL_SPEC_VERSION                1
#define VK_AMD_DISPATCH_TUNNEL_EXTENSION_NAME              "VK_AMD_dispatch_tunnel"
#define VK_AMD_DISPATCH_TUNNEL_EXTENSION_NUMBER            318

#define VK_AMD_DISPATCH_TUNNEL_ENUM(type, offset) \
	VK_EXTENSION_ENUM(VK_AMD_DISPATCH_TUNNEL_EXTENSION_NUMBER, type, offset)

typedef struct VkDispatchTunnelInfoEXT {
	VkStructureType	sType;
	const void	*pNext;
	bool		dispatchTunneling;
} VkDispatchTunnelInfoEXT;

#define VK_STRUCTURE_TYPE_DISPATCH_TUNNEL_INFO_AMD \
	VK_AMD_DISPATCH_TUNNEL_ENUM(VkStructureType, 0)
#endif /* VK_AMD_DISPATCH_TUNNEL_H_ */
