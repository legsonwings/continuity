
#define ROOTSIG_BLEND "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED)"

[RootSignature(ROOTSIG_BLEND)]
[numthreads(8, 8, 1)]
void main(uint2 dtid : SV_DispatchThreadID)
{
	// todo : these slots are a bit weird, to fix later
	Texture2D<float4> src = ResourceDescriptorHeap[5];
	RWTexture2D<float4> dest = ResourceDescriptorHeap[1];

	float4 srccol = src[dtid];
	float4 dest_col = dest[dtid];

	// expect colour with premultiplied alpha
	dest_col.rgb = srccol.rgb + dest_col.rgb * (1 - srccol.a);
	dest_col.a = dest_col.a * srccol.a;

	dest[dtid] = dest_col;
}