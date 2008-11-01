// $Id: $

#ifndef GLRGBSCALER_HH
#define GLRGBSCALER_HH

#include "GLScaler.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class RenderSettings;
class ShaderProgram;

class GLRGBScaler : public GLScaler, private noncopyable
{
public:
	explicit GLRGBScaler(RenderSettings& renderSettings);

	virtual void scaleImage(
		ColorTexture& src,
		unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
		unsigned dstStartY, unsigned dstEndY, unsigned dstWidth);

private:
	RenderSettings& renderSettings;
	std::auto_ptr<ShaderProgram> scalerProgram;
	int texSizeLoc;
	int cnstsLoc;
};

} // namespace openmsx

#endif // GLSIMPLESCALER_HH
