module;

#include "shared/raytracecommon.h"

export module graphics:pathtrace;

import vec;
import :resourcetypes;

export namespace gfx
{

struct ptbuffers
{
	uint32 pos;
	uint32 texcoord;
	uint32 tbn;
	uint32 idx;
	uint32 materials;
	uint32 tlas;
};

struct ptresources
{
	gputexandidx diffusecolortex;
	gputexandidx specbrdftex;
	gputexandidx hdrrendertarget;
	std::array<gputexandidx, 2> hitposition;
	std::array<gputexandidx, 2> normaldepthtex;
	std::array<gputexandidx, 2> diffuseradiancetex;
	std::array<gputexandidx, 2> specularradiancetex;
	std::array<gputexandidx, 2> momentstex;
	std::array<gputexandidx, 2> historylentex; // todo : encode normal into two floats and use remaining channel for depth instead of having two channel historylen
	stdx::ext<structuredbuffer<rt::ptsettings, accesstype::both>, uint32> ptsettingsbuffer;

	uint32 finalcolour;
	uint32 viewglobals;
	uint32 sceneglobals;

	void create(rt::ptsettings const& ptsettings, stdx::vecui2 dims, uint32 colour, uint32 viewglobalsidx, uint32 sceneglobalsidx);
};

struct pathtracepass
{
	pathtracepass(ptbuffers const& ptbuff, ptresources& ptres, uint32 ptspp, resource const& raygenshadertable);
	void operator()(deviceresources& devres);

	uint32 spp;
	ptresources& res;
	rt::rootdescs rootdescs;
	resource const& raygenst;
	structuredbuffer<rt::dispatchparams, accesstype::both> dispatchparams;
};

struct temporalaccumpass
{
	temporalaccumpass(ptresources& ptres);
	void operator()(deviceresources& devres);

	stdx::vecui2 dispatchxy;
	uint32 accumcount;
	ptresources& res;
	stdx::ext<structuredbuffer<rt::accumparams, accesstype::both>, uint32> accumparamsbuffer;
};

struct atrousdenoisepass
{
	atrousdenoisepass(ptresources& ptres);
	void operator()(deviceresources& devres);

	stdx::vecui2 dispatchxy;
	ptresources& res;
	stdx::ext<structuredbuffer<rt::denoiserparams, accesstype::both>, uint32> denoiseparamsbuffer;
	stdx::ext<structuredbuffer<rt::postdenoiseparams, accesstype::both>, uint32> postdenoiseparamsbuffer;
};

}
