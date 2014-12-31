#ifndef MSXPAC_HH
#define MSXPAC_HH

#include "MSXDevice.hh"
#include "SRAM.hh"

namespace openmsx {

class MSXPac final : public MSXDevice
{
public:
	explicit MSXPac(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void checkSramEnable();

	SRAM sram;
	byte r1ffe, r1fff;
	bool sramEnabled;
};

} // namespace openmsx

#endif
