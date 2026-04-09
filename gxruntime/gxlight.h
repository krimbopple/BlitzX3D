#ifndef GXLIGHT_H
#define GXLIGHT_H

class gxScene;

#ifdef DX9
#include <d3d9.h>
#else
#include <d3d.h>
#endif

class gxLight {
public:
	gxLight(gxScene* scene, int type);
	~gxLight();

#ifdef DX9
	D3DLIGHT9 d3d_light;
#else
	D3DLIGHT7 d3d_light;
#endif

private:
	gxScene* scene;

	/***** GX INTERFACE *****/
public:
	enum {
		LIGHT_DISTANT = 1, LIGHT_POINT = 2, LIGHT_SPOT = 3
	};
	void setRange(float range);
	void setColor(const float rgb[3]);
	void setPosition(const float pos[3]);
	void setDirection(const float dir[3]);
	void setConeAngles(float inner, float outer);

	void getColor(float rgb[3]);
};

#endif