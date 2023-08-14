#pragma once

#include <cstdint>

// https://github.com/simple64/simple64/blob/1e4ab555054a659c6e6a91db16ce46714be7ac00/parallel-rdp-standalone/parallel_imp.cpp#L46
static const uint32_t vertex_spirv[] =
		{0x07230203,0x00010000,0x000d000a,0x00000034,
		 0x00000000,0x00020011,0x00000001,0x0006000b,
		 0x00000001,0x4c534c47,0x6474732e,0x3035342e,
		 0x00000000,0x0003000e,0x00000000,0x00000001,
		 0x0008000f,0x00000000,0x00000004,0x6e69616d,
		 0x00000000,0x00000008,0x00000016,0x0000002b,
		 0x00040047,0x00000008,0x0000000b,0x0000002a,
		 0x00050048,0x00000014,0x00000000,0x0000000b,
		 0x00000000,0x00050048,0x00000014,0x00000001,
		 0x0000000b,0x00000001,0x00050048,0x00000014,
		 0x00000002,0x0000000b,0x00000003,0x00050048,
		 0x00000014,0x00000003,0x0000000b,0x00000004,
		 0x00030047,0x00000014,0x00000002,0x00040047,
		 0x0000002b,0x0000001e,0x00000000,0x00020013,
		 0x00000002,0x00030021,0x00000003,0x00000002,
		 0x00040015,0x00000006,0x00000020,0x00000001,
		 0x00040020,0x00000007,0x00000001,0x00000006,
		 0x0004003b,0x00000007,0x00000008,0x00000001,
		 0x0004002b,0x00000006,0x0000000a,0x00000000,
		 0x00020014,0x0000000b,0x00030016,0x0000000f,
		 0x00000020,0x00040017,0x00000010,0x0000000f,
		 0x00000004,0x00040015,0x00000011,0x00000020,
		 0x00000000,0x0004002b,0x00000011,0x00000012,
		 0x00000001,0x0004001c,0x00000013,0x0000000f,
		 0x00000012,0x0006001e,0x00000014,0x00000010,
		 0x0000000f,0x00000013,0x00000013,0x00040020,
		 0x00000015,0x00000003,0x00000014,0x0004003b,
		 0x00000015,0x00000016,0x00000003,0x0004002b,
		 0x0000000f,0x00000017,0xbf800000,0x0004002b,
		 0x0000000f,0x00000018,0x00000000,0x0004002b,
		 0x0000000f,0x00000019,0x3f800000,0x0007002c,
		 0x00000010,0x0000001a,0x00000017,0x00000017,
		 0x00000018,0x00000019,0x00040020,0x0000001b,
		 0x00000003,0x00000010,0x0004002b,0x00000006,
		 0x0000001f,0x00000001,0x0004002b,0x0000000f,
		 0x00000023,0x40400000,0x0007002c,0x00000010,
		 0x00000024,0x00000017,0x00000023,0x00000018,
		 0x00000019,0x0007002c,0x00000010,0x00000027,
		 0x00000023,0x00000017,0x00000018,0x00000019,
		 0x00040017,0x00000029,0x0000000f,0x00000002,
		 0x00040020,0x0000002a,0x00000003,0x00000029,
		 0x0004003b,0x0000002a,0x0000002b,0x00000003,
		 0x0004002b,0x0000000f,0x0000002f,0x3f000000,
		 0x0005002c,0x00000029,0x00000033,0x0000002f,
		 0x0000002f,0x00050036,0x00000002,0x00000004,
		 0x00000000,0x00000003,0x000200f8,0x00000005,
		 0x0004003d,0x00000006,0x00000009,0x00000008,
		 0x000500aa,0x0000000b,0x0000000c,0x00000009,
		 0x0000000a,0x000300f7,0x0000000e,0x00000000,
		 0x000400fa,0x0000000c,0x0000000d,0x0000001d,
		 0x000200f8,0x0000000d,0x00050041,0x0000001b,
		 0x0000001c,0x00000016,0x0000000a,0x0003003e,
		 0x0000001c,0x0000001a,0x000200f9,0x0000000e,
		 0x000200f8,0x0000001d,0x000500aa,0x0000000b,
		 0x00000020,0x00000009,0x0000001f,0x000300f7,
		 0x00000022,0x00000000,0x000400fa,0x00000020,
		 0x00000021,0x00000026,0x000200f8,0x00000021,
		 0x00050041,0x0000001b,0x00000025,0x00000016,
		 0x0000000a,0x0003003e,0x00000025,0x00000024,
		 0x000200f9,0x00000022,0x000200f8,0x00000026,
		 0x00050041,0x0000001b,0x00000028,0x00000016,
		 0x0000000a,0x0003003e,0x00000028,0x00000027,
		 0x000200f9,0x00000022,0x000200f8,0x00000022,
		 0x000200f9,0x0000000e,0x000200f8,0x0000000e,
		 0x00050041,0x0000001b,0x0000002c,0x00000016,
		 0x0000000a,0x0004003d,0x00000010,0x0000002d,
		 0x0000002c,0x0007004f,0x00000029,0x0000002e,
		 0x0000002d,0x0000002d,0x00000000,0x00000001,
		 0x0005008e,0x00000029,0x00000030,0x0000002e,
		 0x0000002f,0x00050081,0x00000029,0x00000032,
		 0x00000030,0x00000033,0x0003003e,0x0000002b,
		 0x00000032,0x000100fd,0x00010038};

static const uint32_t fragment_spirv[] =
		{0x07230203,0x00010000,0x000d000a,0x00000015,
		 0x00000000,0x00020011,0x00000001,0x0006000b,
		 0x00000001,0x4c534c47,0x6474732e,0x3035342e,
		 0x00000000,0x0003000e,0x00000000,0x00000001,
		 0x0007000f,0x00000004,0x00000004,0x6e69616d,
		 0x00000000,0x00000009,0x00000011,0x00030010,
		 0x00000004,0x00000007,0x00040047,0x00000009,
		 0x0000001e,0x00000000,0x00040047,0x0000000d,
		 0x00000022,0x00000000,0x00040047,0x0000000d,
		 0x00000021,0x00000000,0x00040047,0x00000011,
		 0x0000001e,0x00000000,0x00020013,0x00000002,
		 0x00030021,0x00000003,0x00000002,0x00030016,
		 0x00000006,0x00000020,0x00040017,0x00000007,
		 0x00000006,0x00000004,0x00040020,0x00000008,
		 0x00000003,0x00000007,0x0004003b,0x00000008,
		 0x00000009,0x00000003,0x00090019,0x0000000a,
		 0x00000006,0x00000001,0x00000000,0x00000000,
		 0x00000000,0x00000001,0x00000000,0x0003001b,
		 0x0000000b,0x0000000a,0x00040020,0x0000000c,
		 0x00000000,0x0000000b,0x0004003b,0x0000000c,
		 0x0000000d,0x00000000,0x00040017,0x0000000f,
		 0x00000006,0x00000002,0x00040020,0x00000010,
		 0x00000001,0x0000000f,0x0004003b,0x00000010,
		 0x00000011,0x00000001,0x0004002b,0x00000006,
		 0x00000013,0x00000000,0x00050036,0x00000002,
		 0x00000004,0x00000000,0x00000003,0x000200f8,
		 0x00000005,0x0004003d,0x0000000b,0x0000000e,
		 0x0000000d,0x0004003d,0x0000000f,0x00000012,
		 0x00000011,0x00070058,0x00000007,0x00000014,
		 0x0000000e,0x00000012,0x00000002,0x00000013,
		 0x0003003e,0x00000009,0x00000014,0x000100fd,
		 0x00010038};