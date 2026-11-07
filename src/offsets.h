#pragma once

#include <stdint.h>


struct Addresses
{
	uint64_t CalculateCameraAngle_CallSite;
	uint64_t UpdateCamera_CallSite;
	uint64_t SaveToInputConfigFile_CallSite;
	uint64_t roll_movss;
	uint64_t zoom_movss;
};
struct Addresses* GetAddresses(void);

struct Sigs
{
	const char* CalculateCameraAngle_Callsite;
	const char* UpdateCamera_CallSite;
	const char* SaveToInputConfigFile_CallSite;
	const char* roll_movss;
	const char* zoom_movss;
};
struct Sigs* GetSigs(void);
