// $Id$

#include "ResampleLQ.hh"
#include "ResampledSoundDevice.hh"
#include "aligned.hh"
#include <cassert>
#include <cstring>

namespace openmsx {

static const unsigned BUFSIZE = 16384;
ALIGNED(static int bufferInt[BUFSIZE + 4], 16);

////

template<unsigned CHANNELS>
std::auto_ptr<ResampleLQ<CHANNELS> > ResampleLQ<CHANNELS>::create(
		ResampledSoundDevice& input,
		const DynamicClock& hostClock, unsigned emuSampleRate)
{
	std::auto_ptr<ResampleLQ<CHANNELS> > result;
	unsigned hostSampleRate = hostClock.getFreq();
	if (emuSampleRate < hostSampleRate) {
		result.reset(new ResampleLQUp  <CHANNELS>(input, hostClock, emuSampleRate));
	} else {
		result.reset(new ResampleLQDown<CHANNELS>(input, hostClock, emuSampleRate));
	}
	return result;
}

template <unsigned CHANNELS>
ResampleLQ<CHANNELS>::ResampleLQ(
		ResampledSoundDevice& input_,
		const DynamicClock& hostClock_, unsigned emuSampleRate)
	: input(input_)
	, hostClock(hostClock_)
	, emuClock(hostClock.getTime(), emuSampleRate)
	, step(FP::roundRatioDown(emuSampleRate, hostClock.getFreq()))
{
	for (unsigned j = 0; j < 2 * CHANNELS; ++j) {
		lastInput[j] = 0;
	}
}

template <unsigned CHANNELS>
bool ResampleLQ<CHANNELS>::fetchData(EmuTime::param time, unsigned& valid)
{
	unsigned emuNum = emuClock.getTicksTill(time);
	valid = 2 + emuNum;

	assert(emuNum < BUFSIZE);
	emuClock += emuNum;
	assert(emuClock.getTime() <= time);
	assert(emuClock.getFastAdd(1) > time);

	int* buffer = &bufferInt[4 - 2 * CHANNELS];
	assert((long(&buffer[2 * CHANNELS]) & 15) == 0);

	if (!input.generateInput(&buffer[2 * CHANNELS], emuNum)) {
		// New input is all zero
		int last = 0;
		for (unsigned j = 0; j < 2 * CHANNELS; ++j) {
			last |= lastInput[j];
		}
		if (last == 0) {
			// Old input was also all zero, then the resampled
			// output will be all zero as well.
			return false;
		}
		memset(&buffer[CHANNELS], 0, emuNum * CHANNELS * sizeof(int));
	}
	for (unsigned j = 0; j < 2 * CHANNELS; ++j) {
		buffer[j] = lastInput[j];
		lastInput[j] = buffer[emuNum * CHANNELS + j];
	}
	return true;
}

////

template <unsigned CHANNELS>
ResampleLQUp<CHANNELS>::ResampleLQUp(
		ResampledSoundDevice& input,
		const DynamicClock& hostClock, unsigned emuSampleRate)
	: ResampleLQ<CHANNELS>(input, hostClock, emuSampleRate)
{
	assert(emuSampleRate < hostClock.getFreq()); // only upsampling
}

template <unsigned CHANNELS>
bool ResampleLQUp<CHANNELS>::generateOutput(
	int* __restrict dataOut, unsigned hostNum, EmuTime::param time) __restrict
{
	EmuTime host1 = this->hostClock.getFastAdd(1);
	assert(host1 > this->emuClock.getTime());
	FP pos;
	this->emuClock.getTicksTill(host1, pos);
	assert(pos.toInt() < 2);

	unsigned valid; // only indices smaller than this number are valid
	if (!this->fetchData(time, valid)) return false;

	// this is currently only used to upsample cassette player sound,
	// sound quality is not so important here, so use 0-th order
	// interpolation (instead of 1st-order).
	int* buffer = &bufferInt[4 - 2 * CHANNELS];
	for (unsigned i = 0; i < hostNum; ++i) {
		unsigned p = pos.toInt();
		assert(p < valid);
		for (unsigned j = 0; j < CHANNELS; ++j) {
			dataOut[i * CHANNELS + j] = buffer[p * CHANNELS + j];
		}
		pos += this->step;
	}

	return true;
}

////

template <unsigned CHANNELS>
ResampleLQDown<CHANNELS>::ResampleLQDown(
		ResampledSoundDevice& input,
		const DynamicClock& hostClock, unsigned emuSampleRate)
	: ResampleLQ<CHANNELS>(input, hostClock, emuSampleRate)
{
	assert(emuSampleRate > hostClock.getFreq()); // can only do downsampling
}

template <unsigned CHANNELS>
bool ResampleLQDown<CHANNELS>::generateOutput(
	int* __restrict dataOut, unsigned hostNum, EmuTime::param time) __restrict
{
	EmuTime host1 = this->hostClock.getFastAdd(1);
	assert(host1 > this->emuClock.getTime());
	FP pos;
	this->emuClock.getTicksTill(host1, pos);

	unsigned valid;
	if (!this->fetchData(time, valid)) return false;

	int* buffer = &bufferInt[4 - 2 * CHANNELS];
#ifdef __arm__
	if (CHANNELS == 1) {
		unsigned dummy;
		// This asm code is equivalent to the c++ code below (does
		// 1st order interpolation). It's still a bit slow, so we
		// use 0th order interpolation. Sound quality is still good
		// especially on portable devices with only medium quality
		// speakers.
		/*asm volatile (
		"0:\n\t"
			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"mov	r7,%[p],LSR #16\n\t"
			"add	r7,%[buf],r7,LSL #2\n\t"
			"ldmia	r7,{r7,r8}\n\t"
			"sub	r8,r8,r7\n\t"
			"and	%[t],%[p],%[m]\n\t"
			"mul	%[t],r8,%[t]\n\t"
			"add	%[t],r7,%[t],ASR #16\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"subs	%[n],%[n],#4\n\t"
			"bgt	0b\n\t"
			: [p]   "=r"  (pos)
			, [t]   "=&r" (dummy)
			:       "[p]" (pos)
			, [buf] "r"   (buffer)
			, [out] "r"   (dataOut)
			, [s]   "r"   (step)
			, [n]   "r"   (hostNum)
			, [m]   "r"   (0xFFFF)
			: "r7","r8"
		);*/

		// 0th order interpolation
		asm volatile (
		"0:\n\t"
			"lsrs	%[t],%[p],#16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"lsrs	%[t],%[p],#16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"lsrs	%[t],%[p],#16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"lsrs	%[t],%[p],#16\n\t"
			"ldr	%[t],[%[buf],%[t],LSL #2]\n\t"
			"str	%[t],[%[out]],#4\n\t"
			"add	%[p],%[p],%[s]\n\t"

			"subs	%[n],%[n],#4\n\t"
			"bgt	0b\n\t"
			: [p]   "=r"    (pos)
			, [out] "=r"    (dataOut)
			, [n]   "=r"    (hostNum)
			, [t]   "=&r"   (dummy)
			:       "[p]"   (pos)
			,       "[out]" (dataOut)
			,       "[n]"   (hostNum)
			, [buf] "r"     (buffer)
			, [s]   "r"     (this->step)
			: "memory"
		);
	} else {
#endif
		for (unsigned i = 0; i < hostNum; ++i) {
			unsigned p = pos.toInt();
			assert((p + 1) < valid);
			FP fract = pos.fract();
			for (unsigned j = 0; j < CHANNELS; ++j) {
				int s0 = buffer[(p + 0) * CHANNELS + j];
				int s1 = buffer[(p + 1) * CHANNELS + j];
				int out = s0 + (fract * (s1 - s0)).toInt();
				dataOut[i * CHANNELS + j] = out;
			}
			pos += this->step;
		}
#ifdef __arm__
	}
#endif
	return true;
}


// Force template instantiation.
template class ResampleLQ<1>;
template class ResampleLQ<2>;

} // namespace openmsx
