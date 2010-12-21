// $Id$

#include "GLTVScaler.hh"
#include "RenderSettings.hh"

using std::string;

namespace openmsx {

GLTVScaler::GLTVScaler(RenderSettings& renderSettings_)
	: renderSettings(renderSettings_)
{
	for (int i = 0; i < 2; ++i) {
		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  ("tv.vert");
		FragmentShader fragmentShader(header, "tv.frag");
		scalerProgram[i].reset(new ShaderProgram());
		scalerProgram[i]->attach(vertexShader);
		scalerProgram[i]->attach(fragmentShader);
		scalerProgram[i]->link();
#ifdef GL_VERSION_2_0
		if (GLEW_VERSION_2_0) {
			scalerProgram[i]->activate();
			glUniform1i(scalerProgram[i]->getUniformLocation("tex"), 0);
			if (i == 1) {
				glUniform1i(scalerProgram[i]->getUniformLocation("videoTex"), 1);
			}
			texSizeLoc[i] = scalerProgram[i]->getUniformLocation("texSize");
			minScanlineLoc[i] =
				scalerProgram[i]->getUniformLocation("minScanline");
			sizeVarianceLoc[i] =
				scalerProgram[i]->getUniformLocation("sizeVariance");
		}
#endif
	}
}

GLTVScaler::~GLTVScaler()
{
}

void GLTVScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned /*srcWidth*/,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	int i = superImpose ? 1 : 0;
	if (superImpose) {
		glActiveTexture(GL_TEXTURE1);
		superImpose->bind();
		glActiveTexture(GL_TEXTURE0);
	}
	scalerProgram[i]->activate();
	if (GLEW_VERSION_2_0) {
		glUniform3f(texSizeLoc[i], src.getWidth(), src.getHeight(), logSrcHeight);
		// These are experimentally established functions that look good.
		// By design, both are 0 for scanline 0.
		float gap = renderSettings.getScanlineGap();
		glUniform1f(minScanlineLoc[i], 0.1 * gap + 0.2 * gap * gap);
		glUniform1f(sizeVarianceLoc[i], 0.7 * gap - 0.3 * gap * gap);
	}
	drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
	             dstStartY, dstEndY, dstWidth, true);
}

} // namespace openmsx