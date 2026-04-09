#include "std.h"
#include "gxlight.h"
#include "gxscene.h"
#include "gxgraphics.h"

#ifdef DX9

#include <float.h>

gxLight::gxLight(gxScene* s, int type) : scene(s) {
    memset(&d3d_light, 0, sizeof(d3d_light));

    switch (type) {
    case LIGHT_POINT:
        d3d_light.Type = D3DLIGHT_POINT;
        break;
    case LIGHT_SPOT:
        d3d_light.Type = D3DLIGHT_SPOT;
        break;
    default:
        d3d_light.Type = D3DLIGHT_DIRECTIONAL;
    }

    d3d_light.Diffuse.a = 1.0f;
    d3d_light.Diffuse.r = d3d_light.Diffuse.g = d3d_light.Diffuse.b = 1.0f;
    d3d_light.Specular.r = d3d_light.Specular.g = d3d_light.Specular.b = 1.0f;
    d3d_light.Range = FLT_MAX;
    d3d_light.Theta = 0;
    d3d_light.Phi = (float)(3.14159265358979323846 * 0.5);
    d3d_light.Falloff = 1.0f;
    d3d_light.Direction.z = 1.0f;
    setRange(1000);
}

gxLight::~gxLight() {
}

void gxLight::setRange(float r) {
    d3d_light.Attenuation1 = 1.0f / r;
}

void gxLight::setPosition(const float pos[3]) {
    d3d_light.Position.x = pos[0];
    d3d_light.Position.y = pos[1];
    d3d_light.Position.z = pos[2];
}

void gxLight::setDirection(const float dir[3]) {
    d3d_light.Direction.x = dir[0];
    d3d_light.Direction.y = dir[1];
    d3d_light.Direction.z = dir[2];
}

void gxLight::setConeAngles(float inner, float outer) {
    d3d_light.Theta = inner;
    d3d_light.Phi = outer;
}

void gxLight::setColor(const float rgb[3]) {
    d3d_light.Diffuse.r = rgb[0];
    d3d_light.Diffuse.g = rgb[1];
    d3d_light.Diffuse.b = rgb[2];
}

void gxLight::getColor(float rgb[3]) {
    rgb[0] = d3d_light.Diffuse.r;
    rgb[1] = d3d_light.Diffuse.g;
    rgb[2] = d3d_light.Diffuse.b;
}

#else

gxLight::gxLight(gxScene* s, int type) : scene(s) {
    memset(&d3d_light, 0, sizeof(d3d_light));

    switch (type) {
    case LIGHT_POINT:
        d3d_light.dltType = D3DLIGHT_POINT;
        break;
    case LIGHT_SPOT:
        d3d_light.dltType = D3DLIGHT_SPOT;
        break;
    default:
        d3d_light.dltType = D3DLIGHT_DIRECTIONAL;
    }

    d3d_light.dcvDiffuse.a = 1;
    d3d_light.dcvDiffuse.r = d3d_light.dcvDiffuse.g = d3d_light.dcvDiffuse.b = 1;
    d3d_light.dcvSpecular.r = d3d_light.dcvSpecular.g = d3d_light.dcvSpecular.b = 1;
    d3d_light.dvRange = D3DLIGHT_RANGE_MAX;
    d3d_light.dvTheta = 0;
    d3d_light.dvPhi = (float)(3.14159265358979323846 * 0.5);
    d3d_light.dvFalloff = 1;
    d3d_light.dvDirection.z = 1;
    setRange(1000);
}

gxLight::~gxLight() {
}

void gxLight::setRange(float r) {
	d3d_light.dvAttenuation1 = 1.0f / r;
}

void gxLight::setPosition(const float pos[3]) {
	d3d_light.dvPosition.x = pos[0];
	d3d_light.dvPosition.y = pos[1];
	d3d_light.dvPosition.z = pos[2];
}

void gxLight::setDirection(const float dir[3]) {
	d3d_light.dvDirection.x = dir[0];
	d3d_light.dvDirection.y = dir[1];
	d3d_light.dvDirection.z = dir[2];
}

void gxLight::setConeAngles(float inner, float outer) {
    d3d_light.dvTheta = inner;
    d3d_light.dvPhi = outer;
}

void gxLight::setColor(const float rgb[3]) {
    memcpy(&d3d_light.dcvDiffuse, rgb, 12);
}

void gxLight::getColor(float rgb[3]) {
    memcpy(rgb, &d3d_light.dcvDiffuse, 12);
}

#endif