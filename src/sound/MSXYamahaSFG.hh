// $Id$

#ifndef MSXYAMAHASFG_HH
#define MSXYAMAHASFG_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;
class YM2151;
class YM2148;

class MSXYamahaSFG : public MSXDevice
{
public:
	MSXYamahaSFG(MSXMotherBoard& motherBoard, const XMLElement& config,
	             const EmuTime& time);
	virtual ~MSXYamahaSFG();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte readIRQVector();

private:
	void writeRegisterPort(byte value, const EmuTime& time);
	void writeDataPort(byte value, const EmuTime& time);

	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<YM2151> ym2151;
	const std::auto_ptr<YM2148> ym2148;
	int registerLatch;
	byte irqVector;
};

} // namespace openmsx

#endif
