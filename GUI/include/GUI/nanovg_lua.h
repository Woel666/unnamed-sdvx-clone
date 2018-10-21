#pragma once
#include "nanovg.h"
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "Graphics/Font.hpp"
#include "Graphics/RenderQueue.hpp"
#include "Shared/Transform.hpp"

struct GUIState
{
	NVGcontext* vg;
	RenderQueue* rq;
	Transform t;
	Map<lua_State*, Map<int, Text>> textCache;
	Map<lua_State*, int> nextTextId;
	Map<String, Font> fontCahce;
	Font* currentFont;
	Vector4 fillColor;
	int textAlign;
	int fontSize;
	Material* fontMaterial;
};


GUIState g_guiState;

static int LoadFont(const char* name, const char* filename)
{
	{
		Font* cached = g_guiState.fontCahce.Find(name);
		if (cached)
		{
			g_guiState.currentFont = cached;
		}
		else
		{
			String path = filename;
			Font newFont = FontRes::Create(g_gl, path);
			g_guiState.fontCahce.Add(name, newFont);
			g_guiState.currentFont = g_guiState.fontCahce.Find(name);
		}
	}

	{ //nanovg
		if (nvgFindFont(g_guiState.vg, name) != -1)
		{
			nvgFontFace(g_guiState.vg, name);
			return 0;
		}

		nvgFontFaceId(g_guiState.vg, nvgCreateFont(g_guiState.vg, name, filename));
		nvgAddFallbackFont(g_guiState.vg, name, "fallback");
	}
	return 0;
}

static int lBeginPath(lua_State* L)
{
	g_guiState.fillColor = Vector4(1.0);
	nvgBeginPath(g_guiState.vg);
	return 0;
}

static int lText(lua_State* L /*const char* s, float x, float y*/)
{
	const char* s;
	float x, y;
	s = luaL_checkstring(L, 1);
	x = luaL_checknumber(L, 2);
	y = luaL_checknumber(L, 3);
	nvgText(g_guiState.vg, x, y, s, NULL);

	//{ //Fast text
	//	WString text = Utility::ConvertToWString(s);
	//	Text te = (*g_guiState.currentFont)->CreateText(text, g_guiState.fontSize);
	//	Transform textTransform = g_guiState.t;
	//	textTransform *= Transform::Translation(Vector2(x, y));

	//	//vertical alignment
	//	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_BOTTOM) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(0, -te->size.y));
	//	}
	//	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_MIDDLE) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(0, -te->size.y / 2));
	//	}

	//	//horizontal alignment
	//	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_CENTER) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(-te->size.x / 2, 0));
	//	}
	//	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_RIGHT) != 0)
	//	{
	//		textTransform *= Transform::Translation(Vector2(-te->size.x, 0));
	//	}
	//	MaterialParameterSet params;
	//	params.SetParameter("color", g_guiState.fillColor);
	//	g_guiState.rq->Draw(textTransform, te, g_application->GetFontMaterial(), params);
	//}
	return 0;
}
static int guiText(const char* s, float x, float y)
{
	nvgText(g_guiState.vg, x, y, s, 0);
	return 0;
}

static int lFontFace(lua_State* L /*const char* s*/)
{
	const char* s;
	s = luaL_checkstring(L, 1);
	nvgFontFace(g_guiState.vg, s);
	return 0;
}
static int lFontSize(lua_State* L /*float size*/)
{
	float size = luaL_checknumber(L, 1);
	nvgFontSize(g_guiState.vg, size);
	g_guiState.fontSize = size;
	return 0;
}
static int lFillColor(lua_State* L /*int r, int g, int b*/)
{
	int r, g, b;
	r = luaL_checkinteger(L, 1);
	g = luaL_checkinteger(L, 2);
	b = luaL_checkinteger(L, 3);
	nvgFillColor(g_guiState.vg, nvgRGB(r, g, b));
	g_guiState.fillColor = Vector4(r / 255.0, g / 255.0, b / 255.0, 1.0);
	return 0;
}
static int lRect(lua_State* L /*float x, float y, float w, float h*/)
{
	float x, y, w, h;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	w = luaL_checknumber(L, 3);
	h = luaL_checknumber(L, 4);
	nvgRect(g_guiState.vg, x, y, w, h);
	return 0;
}
static int lFill(lua_State* L)
{
	nvgFill(g_guiState.vg);
	return 0;
}
static int lTextAlign(lua_State* L /*int align*/)
{
	nvgTextAlign(g_guiState.vg, luaL_checkinteger(L, 1));
	g_guiState.textAlign = luaL_checkinteger(L, 1);
	return 0;
}
static int lCreateImage(lua_State* L /*const char* filename, int imageflags */)
{
	const char* filename = luaL_checkstring(L, 1);
	int imageflags = luaL_checkinteger(L, 2);
	int handle = nvgCreateImage(g_guiState.vg, filename, imageflags);
	if (handle != 0)
	{
		lua_pushnumber(L, handle);
		return 1;
	}
	return 0;
}
static int lImagePatternFill(lua_State* L /*int image, float alpha*/)
{
	int image = luaL_checkinteger(L, 1);
	float alpha = luaL_checknumber(L, 2);
	int w, h;
	nvgImageSize(g_guiState.vg, image, &w, &h);
	nvgFillPaint(g_guiState.vg, nvgImagePattern(g_guiState.vg, 0, 0, w, h, 0, image, alpha));
	return 0;
}
static int lImageRect(lua_State* L /*float x, float y, float w, float h, int image, float alpha, float angle*/)
{
	float x, y, w, h, alpha, angle;
	int image;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	w = luaL_checknumber(L, 3);
	h = luaL_checknumber(L, 4);
	image = luaL_checkinteger(L, 5);
	alpha = luaL_checknumber(L, 6);
	angle = luaL_checknumber(L, 7);

	int imgH, imgW;
	nvgImageSize(g_guiState.vg, image, &imgW, &imgH);
	float scaleX, scaleY;
	scaleX = w / imgW;
	scaleY = h / imgH;
	nvgTranslate(g_guiState.vg, x, y);
	nvgRotate(g_guiState.vg, angle);
	nvgScale(g_guiState.vg, scaleX, scaleY);
	nvgFillPaint(g_guiState.vg, nvgImagePattern(g_guiState.vg, 0, 0, imgW, imgH, 0, image, alpha));
	nvgRect(g_guiState.vg, 0, 0, imgW, imgH);
	nvgFill(g_guiState.vg);
	nvgScale(g_guiState.vg, 1.0 / scaleX, 1.0 / scaleY);
	nvgRotate(g_guiState.vg, -angle);
	nvgTranslate(g_guiState.vg, -x, -y);
	return 0;
}
static int lScale(lua_State* L /*float x, float y*/)
{
	float x, y;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	nvgScale(g_guiState.vg, x, y);
	g_guiState.t *= Transform::Scale({ x, y, 0 });
	return 0;
}
static int lTranslate(lua_State* L /*float x, float y*/)
{
	float x, y;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	g_guiState.t *= Transform::Translation({ x, y, 0 });
	nvgTranslate(g_guiState.vg, x, y);
	return 0;
}
static int lRotate(lua_State* L /*float angle*/)
{
	float angle = luaL_checknumber(L, 1);
	nvgRotate(g_guiState.vg, angle);
	g_guiState.t *= Transform::Rotation({ 0, 0, angle });
	return 0;
}
static int lResetTransform(lua_State* L)
{
	nvgResetTransform(g_guiState.vg);
	g_guiState.t = Transform();
	return 0;
}
static int lLoadFont(lua_State* L /*const char* name, const char* filename*/)
{
	const char* name = luaL_checkstring(L, 1);
	const char* filename = luaL_checkstring(L, 2);
	LoadFont(name, filename);
	return 0;
}
static int lCreateLabel(lua_State* L /*const char* text, int size, bool monospace*/)
{
	const char* text = luaL_checkstring(L, 1);
	int size = luaL_checkinteger(L, 2);
	int monospace = luaL_checkinteger(L, 3);

	g_guiState.textCache.FindOrAdd(L).Add(g_guiState.nextTextId[L], (*g_guiState.currentFont)->CreateText(Utility::ConvertToWString(text), size, (FontRes::TextOptions)monospace));
	lua_pushnumber(L, g_guiState.nextTextId[L]);
	g_guiState.nextTextId[L]++;
	return 1;
}

static int lUpdateLabel(lua_State* L /*int labelId, const char* text, int size*/)
{
	int labelId = luaL_checkinteger(L, 1);
	const char* text = luaL_checkstring(L, 2);
	int size = luaL_checkinteger(L, 3);
	g_guiState.textCache[L][labelId] = (*g_guiState.currentFont)->CreateText(Utility::ConvertToWString(text), size);
	return 0;
}

static int lDrawLabel(lua_State* L /*int labelId, float x, float y*/)
{
	int labelId = luaL_checkinteger(L, 1);
	float x = luaL_checknumber(L, 2);
	float y = luaL_checknumber(L, 3);
	Transform textTransform = g_guiState.t;
	textTransform *= Transform::Translation(Vector2(x, y));
	Text te = g_guiState.textCache[L][labelId];
	//vertical alignment
	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_BOTTOM) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te->size.y));
	}
	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_MIDDLE) != 0)
	{
		textTransform *= Transform::Translation(Vector2(0, -te->size.y / 2));
	}

	//horizontal alignment
	if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_CENTER) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te->size.x / 2, 0));
	}
	else if ((g_guiState.textAlign & (int)NVGalign::NVG_ALIGN_RIGHT) != 0)
	{
		textTransform *= Transform::Translation(Vector2(-te->size.x, 0));
	}

	MaterialParameterSet params;
	params.SetParameter("color", g_guiState.fillColor);
	g_guiState.rq->Draw(textTransform, te, *g_guiState.fontMaterial, params);
	return 0;
}
