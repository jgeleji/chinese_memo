//
// input_sdl2.cpp - SDL2 backend.
// Captures std::cout (including the ANSI color constants from utils.cpp) into
// a virtual text screen, renders it fullscreen with a real CJK font
// (stb_truetype + NotoSansSC.ttf), and shows the numbered character choices
// as large tappable buttons. A button tap injects the "<digits><enter>"
// keystrokes the core logic already understands, so the game logic is
// completely untouched.
//

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <SDL.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "input.h"
#include "utils.h"

// ---------------------------------------------------------------------------
// colors
// ---------------------------------------------------------------------------

static const Uint32 DEF_FG = 0xFFE1E1DCu; // default foreground (light grey)
static const Uint32 DEF_BG = 0xFF0C0C10u; // default background (near black)

static Uint32 rgb(int r, int g, int b)
{
	return 0xFF000000u | (((Uint32)r) << 16) | (((Uint32)g) << 8) | ((Uint32)b);
}

static Uint32 palette_color(int idx)
{
	static const Uint32 base16[16] = {
		0xFF000000u,  rgb(170, 0, 0),   rgb(0, 170, 0),   rgb(170, 85, 0),
		rgb(0, 0, 170), rgb(170, 0, 170), rgb(0, 170, 170), rgb(170, 170, 170),
		rgb(85, 85, 85), rgb(255, 85, 85), rgb(85, 255, 85), rgb(255, 255, 85),
		rgb(85, 85, 255), rgb(255, 85, 255), rgb(85, 255, 255), 0xFFFFFFFFu
	};
	if(idx >= 0 && idx < 16) return base16[idx];
	if(idx >= 16 && idx < 232)
	{
		int v = idx - 16;
		int cv[3];
		cv[0] = v / 36;
		cv[1] = (v % 36) / 6;
		cv[2] = v % 6;
		int out[3];
		for(int i = 0; i < 3; ++i) out[i] = cv[i] ? 55 + cv[i] * 40 : 0;
		return rgb(out[0], out[1], out[2]);
	}
	if(idx >= 232 && idx < 256)
	{
		int v = 8 + (idx - 232) * 10;
		return rgb(v, v, v);
	}
	return DEF_FG;
}

// ---------------------------------------------------------------------------
// virtual screen: cout is captured into styled runs, one vector per line
// ---------------------------------------------------------------------------

struct Run
{
	std::string text;
	Uint32 fg;
	Uint32 bg;
};

struct SLine
{
	std::vector<Run> runs;
};

static std::vector<SLine> g_lines;     // committed lines of the current screen
static std::string g_partial;          // text since the last '\n'
static Uint32 g_cur_fg = DEF_FG;       // ANSI parser state, persists per line
static Uint32 g_cur_bg = DEF_BG;

// parse one captured line into runs, applying ANSI SGR sequences
static void commit_line(const std::string& raw)
{
	SLine line;
	std::string cur;
	Uint32 fg = g_cur_fg, bg = g_cur_bg;
	size_t i = 0;
	while(i < raw.size())
	{
		if(raw[i] == 0x1B && i + 1 < raw.size() && raw[i+1] == '[')
		{
			size_t j = i + 2;
			while(j < raw.size() && raw[j] != 'm') ++j;
			std::string params = raw.substr(i + 2, j - (i + 2));
			std::vector<int> p;
			size_t pos = 0;
			while(pos < params.size())
			{
				size_t semi = params.find(';', pos);
				if(semi == std::string::npos) semi = params.size();
				p.push_back(atoi(params.substr(pos, semi - pos).c_str()));
				pos = semi + 1;
			}
			for(size_t k = 0; k < p.size(); ++k)
			{
				if(p[k] == 0) { fg = DEF_FG; bg = DEF_BG; }
				else if(p[k] == 39) fg = DEF_FG;
				else if(p[k] == 49) bg = DEF_BG;
				else if(p[k] == 38 && k + 2 < p.size() && p[k+1] == 5) { fg = palette_color(p[k+2]); k += 2; }
				else if(p[k] == 48 && k + 2 < p.size() && p[k+1] == 5) { bg = palette_color(p[k+2]); k += 2; }
				else if(p[k] >= 30 && p[k] <= 37) fg = palette_color(p[k] - 30);
				else if(p[k] >= 40 && p[k] <= 47) bg = palette_color(p[k] - 40);
			}
			i = j + 1;
			continue;
		}
		cur.push_back(raw[i]);
		++i;
	}
	if(!cur.empty()) line.runs.push_back(Run{cur, fg, bg});
	g_cur_fg = fg;
	g_cur_bg = bg;
	g_lines.push_back(line);
}

// ---------------------------------------------------------------------------
// cout capture streambuf
// ---------------------------------------------------------------------------

class ScreenBuf : public std::streambuf
{
	public:
		virtual int overflow(int ch) override
		{
			if(ch == EOF) return 0;
			char c = (char)ch;
			if(c == '\r') return ch;
			if(c == '\n')
			{
				commit_line(g_partial);
				g_partial.clear();
			}
			else
			{
				g_partial.push_back(c);
			}
			return ch;
		}
		virtual int sync() override
		{
			if(!g_partial.empty())
			{
				commit_line(g_partial);
				g_partial.clear();
			}
			return 0;
		}
};

static ScreenBuf g_screenbuf;
static std::streambuf* g_cout_backup = nullptr;

// ---------------------------------------------------------------------------
// backend state
// ---------------------------------------------------------------------------

static bool g_inited = false;
static SDL_Window* g_window = nullptr;
static SDL_Renderer* g_renderer = nullptr;
static SDL_Texture* g_texture = nullptr;
static Uint32* g_pixels = nullptr;
static int g_w = 0, g_h = 0;         // texture pixel size == screen pixels
static int g_dump_counter = 0;

static std::deque<unsigned char> g_keys;             // synthesized keystrokes
static std::vector<SDL_Rect> g_button_rects;         // hit boxes, per option
static int g_active_state = 0;                       // INPUT_STATE mirror
static bool g_ime_on = false;
static bool g_scripted = false;                      // CM_AUTOKEYS headless run
static const char* g_keysfile = nullptr;             // CM_AUTOKEYS_FILE: keystrokes appended by a driver script
static long g_keysfile_pos = 0;
static const char* g_statefile = nullptr;            // CM_STATE_FILE: "waiting" marker for the driver script
static long g_wait_seq = 0;

static stbtt_fontinfo g_font;
static std::vector<unsigned char> g_fontdata;
static float g_font_scale_cache_size = 0;
static bool g_font_ok = false;

// ---------------------------------------------------------------------------
// dirty region
//
// Filling the whole 1280x2467 framebuffer, uploading it and blitting it costs
// ~38 MB of memory traffic per frame, which on the phone is far too slow: taps
// queue up behind the drawing. Everything written to the framebuffer goes
// through fill_rect()/blit_runs(), and both tell us the box they touched, so a
// frame only has to repaint (the box that changed last time) + (the boxes the
// new frame draws).
// ---------------------------------------------------------------------------

static SDL_Rect g_damage;          // what the frame being built touches
static SDL_Rect g_prev_damage;     // what is currently visible on screen
static bool g_full_repaint = true; // first frame, or after the surface was recreated

static void mark_dirty(int x0, int y0, int x1, int y1)
{
	if(x0 < 0) x0 = 0;
	if(y0 < 0) y0 = 0;
	if(x1 > g_w) x1 = g_w;
	if(y1 > g_h) y1 = g_h;
	if(x0 >= x1 || y0 >= y1) return;
	if(g_damage.w <= 0)
	{
		g_damage.x = x0; g_damage.y = y0; g_damage.w = x1 - x0; g_damage.h = y1 - y0;
		return;
	}
	int rx1 = g_damage.x + g_damage.w, ry1 = g_damage.y + g_damage.h;
	if(x0 < g_damage.x) g_damage.x = x0;
	if(y0 < g_damage.y) g_damage.y = y0;
	if(x1 > rx1) rx1 = x1;
	if(y1 > ry1) ry1 = y1;
	g_damage.w = rx1 - g_damage.x;
	g_damage.h = ry1 - g_damage.y;
}

// INPUT_STATE is a private enum of Input (TYPE_PINYIN=0, CHOOSE_NUMBER=1, EXIT=2);
// member functions of Input set g_active_state, free functions use these constants.
static const int STATE_TYPE_PINYIN = 0;
static const int STATE_CHOOSE_NUMBER = 1;
static const int STATE_EXIT = 2;

static bool buttons_active()
{
	return g_active_state == STATE_CHOOSE_NUMBER
		&& !chinese::current_options().empty();
}

static void push_key_string(const std::string& s)
{
	for(size_t i = 0; i < s.size(); ++i) g_keys.push_back((unsigned char)s[i]);
}

// headless driver support: CM_AUTOKEYS_FILE is a file a driver script appends
// raw keystrokes to; CM_STATE_FILE tells that script when the program is
// sitting in a wait for a key (and in which INPUT_STATE), so the driver can
// read the last dumped frame, work out the correct answer and feed it in.
static void poll_keys_file(void)
{
	if(!g_keysfile) return;
	FILE* f = fopen(g_keysfile, "rb");
	if(!f) return;
	fseek(f, g_keysfile_pos, SEEK_SET);
	std::string got;
	int c;
	while((c = fgetc(f)) != EOF) got.push_back((char)c);
	fclose(f);
	g_keysfile_pos += (long)got.size();
	push_key_string(got);
}

static void write_state(const char* what)
{
	if(!g_statefile) return;
	FILE* f = fopen(g_statefile, "w");
	if(!f) return;
	fprintf(f, "%s seq=%ld state=%d keys=%d frame=%d\n",
		what, g_wait_seq, g_active_state, (int)g_keys.size(), g_dump_counter);
	fclose(f);
	++g_wait_seq;
}

// ---------------------------------------------------------------------------
// font + glyph atlas
// ---------------------------------------------------------------------------

// one rasterized glyph, stored in this cache's alpha atlas
struct Glyph
{
	int x0, y0;      // position of the bitmap inside the atlas
	int w, h;        // bitmap size (may be 0 for whitespace)
	int xoff, yoff;  // offset of the bitmap from the pen position
	int advance;     // pen advance in pixels
};

// Simple row-based atlas packer on top of stbtt_GetCodepointBitmapSubpixel.
// Atlases grow (x2) on demand and are re-baked, so any set of codepoints fits.
struct GlyphCache
{
	int px;                                        // pixel height of this atlas
	int w, h;                                      // atlas dimensions
	std::vector<unsigned char> bmp;                // atlas alpha bytes
	int cursor_x, cursor_y, row_h;
	float scale;
	stbtt_fontinfo* font;
	std::unordered_map<unsigned int, Glyph> glyphs;
	std::unordered_set<unsigned int> missing;      // codepoints the font does not have
	std::vector<unsigned int> order;               // codepoints baked, for re-bake on grow

	GlyphCache() : px(0), w(0), h(0), cursor_x(0), cursor_y(0), row_h(0), scale(0.0f), font(NULL) {}

	bool init(int pixels, int aw, int ah)
	{
		px = pixels;
		w = aw; h = ah;
		cursor_x = 0; cursor_y = 0; row_h = 0;
		scale = stbtt_ScaleForPixelHeight(font, (float)px);
		bmp.assign((size_t)w * h, 0);
		return true;
	}

	bool alloc_slot(int gw, int gh, int& x, int& y)
	{
		if(gw > w || gh > h) return false;             // needs a bigger atlas
		if(cursor_x + gw > w) { cursor_x = 0; cursor_y += row_h; row_h = 0; }
		if(cursor_y + gh > h) return false;            // needs a bigger atlas
		x = cursor_x; y = cursor_y;
		cursor_x += gw + 1;
		if(gh + 1 > row_h) row_h = gh + 1;
		return true;
	}

	// returns true if the glyph is baked (or was already), false if the font
	// lacks it (recorded in missing) or the atlas is full (caller grows)
	bool bake(unsigned int cp)
	{
		if(stbtt_FindGlyphIndex(font, (int)cp) == 0 && cp != ' ')
		{
			missing.insert(cp);
			return false;
		}
		int adv = 0, lsb = 0;
		stbtt_GetCodepointHMetrics(font, (int)cp, &adv, &lsb);

		int bw = 0, bh = 0, bxoff = 0, byoff = 0;
		unsigned char* bits = stbtt_GetCodepointBitmapSubpixel(
			font, scale, scale, 0.0f, 0.0f, (int)cp, &bw, &bh, &bxoff, &byoff);

		Glyph g;
		g.xoff = bxoff; g.yoff = byoff;
		g.advance = (int)((float)adv * scale + 0.5f);
		g.w = bw; g.h = bh;
		g.x0 = 0; g.y0 = 0;
		bool ok = true;
		if(bw > 0 && bh > 0)
		{
			if(!alloc_slot(bw, bh, g.x0, g.y0))
			{
				if(bits) STBTT_free(bits, NULL);
				return false;
			}
			for(int y = 0; y < bh; ++y)
				memcpy(&bmp[(size_t)(g.y0 + y) * w + g.x0], bits + (size_t)y * bw, bw);
		}
		if(bits) STBTT_free(bits, NULL);
		glyphs[cp] = g;
		order.push_back(cp);
		return ok;
	}

	bool grow()
	{
		int nw = w * 2, nh = h * 2;
		if(nw > 4096 || nh > 4096) return false;
		std::vector<unsigned int> old = order;
		glyphs.clear();
		order.clear();
		w = nw; h = nh;
		cursor_x = 0; cursor_y = 0; row_h = 0;
		bmp.assign((size_t)w * h, 0);
		for(size_t i = 0; i < old.size(); ++i) bake(old[i]);
		return true;
	}

	const Glyph* get(unsigned int cp)
	{
		std::unordered_map<unsigned int, Glyph>::iterator it = glyphs.find(cp);
		if(it != glyphs.end()) return &it->second;
		if(missing.find(cp) != missing.end()) return NULL;
		for(int attempt = 0; attempt < 4; ++attempt)
		{
			if(bake(cp)) return &glyphs[cp];
			if(missing.find(cp) != missing.end()) return NULL;
			if(!grow())
			{
				missing.insert(cp);
				return NULL;
			}
		}
		missing.insert(cp);
		return NULL;
	}
};

static std::unordered_map<int, GlyphCache> g_caches;   // keyed by pixel size (quantized)

static GlyphCache* cache_for(int px)
{
	if(px < 10) px = 10;
	px = (px / 4) * 4;                                  // quantize to limit atlas count
	std::unordered_map<int, GlyphCache>::iterator it = g_caches.find(px);
	if(it != g_caches.end()) return &it->second;
	GlyphCache c;
	c.font = &g_font;
	c.init(px, 512, 512);
	g_caches[px] = c;
	return &g_caches[px];
}

static float font_ascent(int px)
{
	int a, d, lg;
	stbtt_GetFontVMetrics(&g_font, &a, &d, &lg);
	return (float)a * stbtt_ScaleForPixelHeight(&g_font, (float)px);
}

static float font_descent(int px)
{
	int a, d, lg;
	stbtt_GetFontVMetrics(&g_font, &a, &d, &lg);
	return (float)d * stbtt_ScaleForPixelHeight(&g_font, (float)px);
}

static void font_line_height(int px, float& height)
{
	height = (font_ascent(px) - font_descent(px)) * 1.35f;
}

// ---------------------------------------------------------------------------
// UTF-8 helpers
// ---------------------------------------------------------------------------

static const unsigned char* utf8_next(const unsigned char* p, const unsigned char* end, unsigned int& cp)
{
	if(p >= end) { cp = 0; return end; }
	unsigned char c = *p;
	if(c < 0x80) { cp = c; return p + 1; }
	if((c & 0xE0) == 0xC0 && p + 1 < end) { cp = ((unsigned)(c & 0x1F) << 6) | (p[1] & 0x3F); return p + 2; }
	if((c & 0xF0) == 0xE0 && p + 2 < end) { cp = ((unsigned)(c & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); return p + 3; }
	if((c & 0xF8) == 0xF0 && p + 3 < end) { cp = ((unsigned)(c & 0x07) << 18) | ((unsigned)(p[1] & 0x3F) << 12) | ((unsigned)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); return p + 4; }
	cp = 0xFFFD;
	return p + 1;
}

static size_t utf8_len(unsigned char lead)
{
	if(lead < 0x80) return 1;
	if((lead & 0xE0) == 0xC0) return 2;
	if((lead & 0xF0) == 0xE0) return 3;
	if((lead & 0xF8) == 0xF0) return 4;
	return 1;
}

// ---------------------------------------------------------------------------
// text measurement
// ---------------------------------------------------------------------------

// width of a substring (in bytes from run.text, limited) at a pixel size
static double measure_range(GlyphCache* gc, const std::string& text, size_t begin, size_t end, int px)
{
	double total = 0.0;
	const unsigned char* p = (const unsigned char*)text.data() + begin;
	const unsigned char* e = (const unsigned char*)text.data() + end;
	while(p < e)
	{
		unsigned int cp;
		const unsigned char* q = utf8_next(p, e, cp);
		if(cp == 0) break;
		const Glyph* g = gc->get(cp);
		if(g) total += (double)g->advance;
		else total += (double)px * 0.5;
		p = q;
	}
	return total;
}

static double measure_run(GlyphCache* gc, const Run& r, int px)
{
	return measure_range(gc, r.text, 0, r.text.size(), px);
}

static double measure_line_runs(GlyphCache* gc, const std::vector<Run>& runs, int px)
{
	double total = 0.0;
	for(size_t i = 0; i < runs.size(); ++i) total += measure_run(gc, runs[i], px);
	return total;
}

// wrap a logical line into sublines that fit avail_w; split at spaces where
// possible, mid-character for overlong runs. styles are preserved.
static std::vector<SLine> wrap_line(GlyphCache* gc, const SLine& line, int px, double avail_w)
{
	std::vector<SLine> out;
	SLine cur;
	double cur_w = 0.0;

	std::vector<Run> runs = line.runs;
	for(size_t ri = 0; ri < runs.size(); ++ri)
	{
		Run r = runs[ri];
		size_t pos = 0;
		while(pos < r.text.size())
		{
			// take the next word (up to and including trailing spaces)
			size_t word_start = pos;
			bool started_with_space = (r.text[pos] == ' ');
			while(pos < r.text.size() && r.text[pos] == ' ') ++pos;
			while(pos < r.text.size() && r.text[pos] != ' ') ++pos;
			size_t word_end = pos;
			double w = measure_range(gc, r.text, word_start, word_end, px);
			if(cur_w > 0.0 && cur_w + w > avail_w)
			{
				if(!cur.runs.empty())
				{
					// trim trailing spaces on the closing line
					while(!cur.runs.empty())
					{
						Run& last = cur.runs.back();
						size_t t = last.text.find_last_not_of(' ');
						if(t == std::string::npos) { cur.runs.pop_back(); continue; }
						last.text.resize(t + 1);
						break;
					}
				}
				out.push_back(cur);
				cur.runs.clear();
				cur_w = 0.0;
				if(started_with_space) continue;   // drop the space that broke the line
			}
			if(word_end > word_start)
				cur.runs.push_back(Run{r.text.substr(word_start, word_end - word_start), r.fg, r.bg});
			cur_w += w;
		}
	}
	if(!cur.runs.empty()) out.push_back(cur);
	if(out.empty()) out.push_back(SLine());
	return out;
}

// Does this line look like one of the auto-generated numbered option rows the
// core logic prints ("  1 中   2 忠 ...")? Those become tappable buttons
// instead of text. They are the only lines printed in yellow foreground whose
// text starts (after pad_to_three's padding) with a digit; the label lines
// ("0 top_row", "1 raw", "2 pinyin", "4 echo") always start at column 0.
static bool is_option_line(const SLine& line)
{
	if(line.runs.empty()) return false;
	if(line.runs.front().fg != palette_color(3)) return false;   // yellow_foreground
	std::string joined;
	for(size_t i = 0; i < line.runs.size(); ++i) joined += line.runs[i].text;
	size_t i = 0;
	while(i < joined.size() && joined[i] == ' ') ++i;
	if(i >= joined.size()) return false;
	return isdigit((unsigned char)joined[i]) != 0;
}

// ---------------------------------------------------------------------------
// pixel blitting
// ---------------------------------------------------------------------------

static void blit_runs(const std::vector<Run>& runs, int x, int y_top, float baseline, int px)
{
	GlyphCache* gc = cache_for(px);
	if(!gc) return;
	float asc = font_ascent(px);
	float dsc = font_descent(px);
	int band_h = (int)(asc - dsc) + 2;

	int x_cur = x;
	for(size_t i = 0; i < runs.size(); ++i)
	{
		const Run& r = runs[i];
		double w = measure_run(gc, r, px);
		int x0 = x_cur, x1 = (int)(x_cur + w);
		if(r.bg != DEF_BG)
		{
			for(int yy = (int)y_top; yy < (int)y_top + band_h; ++yy)
				for(int xx = x0; xx < x1; ++xx)
					if(xx >= 0 && xx < g_w && yy >= 0 && yy < g_h)
						g_pixels[(size_t)yy * g_w + xx] = r.bg;
			mark_dirty(x0, (int)y_top, x1, (int)y_top + band_h);
		}
		const unsigned char* p = (const unsigned char*)r.text.data();
		const unsigned char* e = (const unsigned char*)r.text.data() + r.text.size();
		double fx = (double)x_cur;
		while(p < e)
		{
			unsigned int cp;
			const unsigned char* q = utf8_next(p, e, cp);
			if(cp == 0) break;
			const Glyph* gb = gc->get(cp);
			if(gb)
			{
				int gx0 = (int)floor(fx + (double)gb->xoff);
				int gy0 = (int)floor(baseline + (double)gb->yoff);
				int gw = gb->w;
				int gh = gb->h;
				for(int row = 0; row < gh; ++row)
				{
					int dy = gy0 + row;
					if(dy < 0 || dy >= g_h) continue;
					const unsigned char* src = &gc->bmp[(size_t)(gb->y0 + row) * gc->w + gb->x0];
					for(int col = 0; col < gw; ++col)
					{
						int dx = gx0 + col;
						if(dx < 0 || dx >= g_w) continue;
						unsigned int cov = src[col];
						if(!cov) continue;
						Uint32* dst = &g_pixels[(size_t)dy * g_w + dx];
						unsigned int a = cov;
						Uint32 f = r.fg;
						Uint32 dr = (*dst) >> 16 & 0xFF, dg = (*dst) >> 8 & 0xFF, db = (*dst) & 0xFF;
						Uint32 fr = f >> 16 & 0xFF, fg2 = f >> 8 & 0xFF, fb = f & 0xFF;
						Uint32 nr = (fr * a + dr * (255 - a)) / 255;
						Uint32 ng = (fg2 * a + dg * (255 - a)) / 255;
						Uint32 nb = (fb * a + db * (255 - a)) / 255;
						*dst = 0xFF000000u | (nr << 16) | (ng << 8) | nb;
					}
				}
				mark_dirty(gx0, gy0, gx0 + gw, gy0 + gh);
				fx += (double)gb->advance;
			}
			else
			{
				fx += (double)px * 0.5;
			}
			p = q;
		}
		x_cur = (int)floor(fx);
	}
}

static void blit_text_centered(const std::string& text, Uint32 fg, int cx, int cy, int px)
{
	GlyphCache* gc = cache_for(px);
	if(!gc) return;
	Run r; r.text = text; r.fg = fg; r.bg = DEF_BG;
	double w = measure_run(gc, r, px);
	float h;
	font_line_height(px, h);
	std::vector<Run> runs;
	runs.push_back(r);
	blit_runs(runs, (int)(cx - w / 2.0), (int)(cy - h / 2.0), (cy - h / 2.0) + (double)font_ascent(px), px);
}

static void fill_rect(SDL_Rect rc, Uint32 color)
{
	for(int yy = rc.y; yy < rc.y + rc.h; ++yy)
		for(int xx = rc.x; xx < rc.x + rc.w; ++xx)
			if(xx >= 0 && xx < g_w && yy >= 0 && yy < g_h)
				g_pixels[(size_t)yy * g_w + xx] = color;
	mark_dirty(rc.x, rc.y, rc.x + rc.w, rc.y + rc.h);
}

// ---------------------------------------------------------------------------
// button grid
// ---------------------------------------------------------------------------

struct ButtonLayout
{
	int cols, rows, cell_w, cell_h, gap, x0, y0;
};

static size_t button_cell_count()
{
	// one extra cell beyond the options: the empty-Enter key, which is how the
	// keyboard version closed an answer ("press Enter with nothing typed")
	return chinese::current_options().size() + 1;
}

static ButtonLayout compute_button_layout()
{
	size_t n = button_cell_count();
	ButtonLayout L;
	L.gap = std::max(8, g_w / 50);
	L.cols = n <= 2 ? 1 : (n <= 8 ? 2 : (n <= 16 ? 3 : 4));
	L.rows = (int)((n + L.cols - 1) / L.cols);
	L.cell_w = (g_w - (L.cols + 1) * L.gap) / L.cols;
	int max_btn_area = (int)(g_h * 0.78);
	int max_cell_h = std::max(60, g_h / 7);        // big targets, but leave room for the question
	L.cell_h = std::min(max_cell_h, std::max(42, (max_btn_area - (L.rows + 1) * L.gap) / L.rows));
	L.x0 = L.gap;
	L.y0 = g_h - (L.gap + L.rows * (L.cell_h + L.gap));
	return L;
}

static void layout_button_rects()
{
	g_button_rects.clear();
	if(!buttons_active()) return;
	size_t n = button_cell_count();
	ButtonLayout L = compute_button_layout();
	for(size_t i = 0; i < n; ++i)
	{
		int r = (int)(i / L.cols), c = (int)(i % L.cols);
		SDL_Rect rc;
		rc.x = L.x0 + c * (L.cell_w + L.gap);
		rc.y = L.y0 + r * (L.cell_h + L.gap);
		rc.w = L.cell_w;
		rc.h = L.cell_h;
		g_button_rects.push_back(rc);
	}
}

// ---------------------------------------------------------------------------
// on-screen key bar (only while a free-typed answer is expected):
// DEL + the four tone keys + OK. Taps push the same keystrokes the phone
// keyboard would produce, so the input logic is unchanged.
// ---------------------------------------------------------------------------

static std::vector<SDL_Rect> g_bar_rects;
static std::vector<std::string> g_bar_labels;
static std::vector<std::string> g_bar_keys;

struct BarLayout
{
	int x0, y0, cell_w, cell_h, gap;
};

// The bar must survive the whole typed answer, including the "one Enter was
// pressed, now finish or continue" state (state 1 in the free-typed flows).
// It is hidden only while the option buttons take that place on screen.
static bool inputbar_active()
{
	return g_inited && !buttons_active();
}

static void init_bar_keys()
{
	if(!g_bar_keys.empty()) return;
	g_bar_labels.push_back("DEL");
	g_bar_keys.push_back(std::string(1, (char)127));   // the model's backspace key
	// the tone keys are drawn as tone marks, but they push the digit the input
	// logic expects ("zhong" + tap of the 3rd key == typing zhong3)
	g_bar_labels.push_back("ˉ");  g_bar_keys.push_back("1");
	g_bar_labels.push_back("ˊ");  g_bar_keys.push_back("2");
	g_bar_labels.push_back("ˇ");  g_bar_keys.push_back("3");
	g_bar_labels.push_back("ˋ");  g_bar_keys.push_back("4");
	g_bar_labels.push_back("OK"); g_bar_keys.push_back("\n");
}

// The soft keyboard is a separate window: SDL's surface is not resized when it
// appears (the activity window uses softInputMode=pan), so anything drawn in the
// bottom ~40% of the screen ends up underneath the keyboard. Measure where the
// keys actually are on the target phone (iFlytek/MIUI, 1280x2620 surface, keys
// start at surface y=1561 => 40.4%) and keep the key bar above that band.
static int ime_band(void)
{
#ifdef __ANDROID__
	if(!inputbar_active()) return 0;      // option buttons replace the bar, no keyboard
	return (int)(g_h * 0.41);
#else
	return 0;
#endif
}

static BarLayout compute_bar_layout()
{
	init_bar_keys();
	BarLayout B;
	B.gap = std::max(8, g_w / 50);
	int n = (int)g_bar_keys.size();
	B.cell_w = (g_w - (n + 1) * B.gap) / n;
	B.cell_h = std::max(72, g_h / 9);
	B.x0 = B.gap;
	int band = ime_band();
	B.y0 = g_h - band - B.gap - B.cell_h;
	if(B.y0 < g_h / 2) B.y0 = g_h / 2;    // never push the bar into the text area
	return B;
}

static void layout_bar_rects()
{
	g_bar_rects.clear();
	if(!inputbar_active()) return;
	BarLayout B = compute_bar_layout();
	for(size_t i = 0; i < g_bar_keys.size(); ++i)
	{
		SDL_Rect rc;
		rc.x = B.x0 + (int)i * (B.cell_w + B.gap);
		rc.y = B.y0;
		rc.w = B.cell_w;
		rc.h = B.cell_h;
		g_bar_rects.push_back(rc);
	}
}

static std::string bar_label(size_t i)
{
	// state 1 of a free-typed answer means "Enter already pressed: add another
	// part or finish", so say what the gold key does there.
	if(g_bar_keys[i] == "\n")
		return g_active_state == STATE_TYPE_PINYIN ? "OK" : "END";
	return g_bar_labels[i];
}

static void draw_bar()
{
	if(!inputbar_active()) return;
	for(size_t i = 0; i < g_bar_rects.size(); ++i)
	{
		const SDL_Rect& rc = g_bar_rects[i];
		Uint32 border = 0xFF2E3440u, face = 0xFF3B4250u, fg = 0xFFECEAF0u;
		if(g_bar_keys[i] == "\n") { border = 0xFF7A6A18u; face = 0xFF9A8A2Au; fg = rgb(255, 230, 120); }
		else if(i >= 1 && i <= 4) { face = 0xFF343B48u; fg = rgb(200, 215, 235); }
		fill_rect(rc, border);
		SDL_Rect inner = {rc.x + 3, rc.y + 3, rc.w - 6, rc.h - 6};
		fill_rect(inner, face);
		std::string label = bar_label(i);
		int px = std::min(96, std::max(20, rc.h / 3));
		GlyphCache* gc = cache_for(px);
		if(gc)
		{
			Run r; r.text = label; r.fg = fg; r.bg = face;
			while(px > 20 && measure_run(gc, r, px) > rc.w - 16) px = (int)(px * 0.85);
		}
		blit_text_centered(label, fg, rc.x + rc.w / 2, rc.y + rc.h / 2, px);
	}
}

static void draw_buttons()
{
	if(!buttons_active()) return;
	const std::vector<std::string>& opts = chinese::current_options();
	for(size_t i = 0; i < g_button_rects.size(); ++i)
	{
		const SDL_Rect& rc = g_button_rects[i];
		bool endcell = (i >= opts.size());
		Uint32 border = 0xFF2E3440u, face = 0xFF3B4250u;
		if(endcell) { border = 0xFF7A6A18u; face = 0xFF9A8A2Au; }
		fill_rect(rc, border);
		SDL_Rect inner = {rc.x + 3, rc.y + 3, rc.w - 6, rc.h - 6};
		fill_rect(inner, face);
		// number badge (the "2" in "tap 2") in gold, top-left
		Run badge;
		badge.text = endcell ? std::string("") : std::to_string(i + 1) + ".";
		badge.fg = rgb(255, 215, 0);
		badge.bg = face;
		std::vector<Run> br;
		if(!endcell) br.push_back(badge);
		int badge_px = std::min(64, std::max(16, rc.h / 5));
		float bl = (float)rc.y + 6 + font_ascent(badge_px);
		if(!br.empty()) blit_runs(br, rc.x + 10, rc.y + 6, bl, badge_px);
		// option text, centered, shrunk to fit
		std::string label = endcell ? std::string("END") : opts[i];
		int px = std::min(150, std::max(20, (int)(rc.h * 0.66)));
		GlyphCache* gc = cache_for(px);
		if(gc)
		{
			Run r; r.text = label; r.fg = endcell ? rgb(255, 230, 120) : DEF_FG; r.bg = face;
			while(px > 20 && measure_run(gc, r, px) > rc.w - 24) px = (int)(px * 0.85);
		}
		blit_text_centered(label, endcell ? rgb(255, 230, 120) : DEF_FG, rc.x + rc.w / 2, rc.y + rc.h / 2 + rc.h / 12, px);
	}
}

// ---------------------------------------------------------------------------
// text region + present
// ---------------------------------------------------------------------------

struct Subline
{
	std::vector<Run> runs;
	int px;
};

// Dump destination + frame cap. On the PC these come from the environment; an
// Android app has no way to set environment variables, so the same hooks are
// configured by files in the app's own data directory, which is reachable over
// adb because the build is debuggable:
//   adb shell run-as com.geleji.chinesememo mkdir -p files/dump
//   adb shell run-as com.geleji.chinesememo sh -c 'echo /data/data/com.geleji.chinesememo/files/dump > files/cm_dump_dir'
//   pull with: adb exec-out "run-as com.geleji.chinesememo cat files/dump/frame000.png" > shot.png
static const char* dump_dir(void)
{
	static std::string cached;
	static bool checked = false;
	const char* env = getenv("CM_DUMP_DIR");
	if(env && env[0]) return env;
	if(checked) return cached.empty() ? NULL : cached.c_str();
	checked = true;
	FILE* f = fopen("cm_dump_dir", "rb");
	if(f)
	{
		char buf[400];
		size_t n = fread(buf, 1, sizeof(buf) - 1, f);
		fclose(f);
		while(n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r' || buf[n-1] == ' ' || buf[n-1] == '\t')) --n;
		buf[n] = 0;
		cached = buf;
	}
	return cached.empty() ? NULL : cached.c_str();
}

static int frame_cap(void)
{
	const char* env = getenv("CM_MAXFRAMES");
	if(env && env[0]) return atoi(env);
	FILE* f = fopen("cm_maxframes", "rb");
	if(!f) return 0;
	char buf[32] = {0};
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	(void)n;
	return atoi(buf);
}

static void dump_frame(void)
{
	const char* dir = dump_dir();
	if(!dir || !dir[0]) return;
	int cap = frame_cap();
	if(cap > 0 && g_dump_counter >= cap) exit(0);
	char name[512];
	snprintf(name, sizeof(name), "%s/frame%03d.png", dir, g_dump_counter);
	// the framebuffer is ARGB8888 (memory order B,G,R,A) while PNG wants RGBA,
	// so convert before writing or the dump comes out with red and blue swapped
	static std::vector<unsigned char> rgba;
	rgba.resize((size_t)g_w * g_h * 4);
	for(size_t i = 0; i < (size_t)g_w * g_h; ++i)
	{
		Uint32 c = g_pixels[i];
		rgba[i * 4 + 0] = (unsigned char)((c >> 16) & 0xFF);
		rgba[i * 4 + 1] = (unsigned char)((c >> 8) & 0xFF);
		rgba[i * 4 + 2] = (unsigned char)(c & 0xFF);
		rgba[i * 4 + 3] = 0xFF;
	}
	stbi_write_png(name, g_w, g_h, 4, &rgba[0], g_w * 4);
	snprintf(name, sizeof(name), "%s/frame%03d.txt", dir, g_dump_counter);
	FILE* f = fopen(name, "w");
	if(f)
	{
		for(size_t i = 0; i < g_lines.size(); ++i)
		{
			for(size_t j = 0; j < g_lines[i].runs.size(); ++j)
				fprintf(f, "%s", g_lines[i].runs[j].text.c_str());
			fprintf(f, "\n");
		}
		fclose(f);
	}
	++g_dump_counter;
}

static void present(void)
{
	if(!g_inited || !g_texture) return;

	Uint32 t_start = SDL_GetTicks();

	std::cout.flush();   // push any partial line into the screen model

	// the framebuffer still holds exactly what the screen shows, so only the
	// region that was drawn last frame needs to be erased before redrawing
	g_damage = g_full_repaint ? SDL_Rect{0, 0, g_w, g_h} : g_prev_damage;
	g_full_repaint = false;
	fill_rect(g_damage, DEF_BG);

	layout_button_rects();
	layout_bar_rects();

	int margin = std::max(10, g_w / 40);
	int text_top = margin;
	int text_bottom = g_h - margin;
	if(buttons_active())
	{
		ButtonLayout L = compute_button_layout();
		text_bottom = L.y0 - L.gap - 2 * margin;
	}
	else if(inputbar_active())
	{
		BarLayout B = compute_bar_layout();
		text_bottom = B.y0 - B.gap - 2 * margin;
	}

	// build sublines (skip the numbered option lines; those become buttons)
	std::vector<Subline> subs;
	{
		GlyphCache* gc = cache_for(44);
		int base_px = 44;
		for(size_t i = 0; i < g_lines.size(); ++i)
		{
			if(buttons_active() && is_option_line(g_lines[i])) continue;
			if(g_lines[i].runs.empty())
			{
				Subline s;
				s.px = base_px;
				subs.push_back(s);
				continue;
			}
			std::vector<SLine> parts = wrap_line(gc, g_lines[i], base_px, g_w - 2 * margin);
			for(size_t k = 0; k < parts.size(); ++k)
			{
				Subline s;
				s.runs = parts[k].runs;
				s.px = base_px;
				subs.push_back(s);
			}
		}
	}

	// shrink everything until it fits the text region
	int avail_h = text_bottom - text_top;
	for(;;)
	{
		double total = 0.0;
		for(size_t i = 0; i < subs.size(); ++i)
		{
			float lh;
			font_line_height(subs[i].px, lh);
			total += lh;
		}
		if(total <= avail_h || subs.empty()) break;
		bool shrunk = false;
		for(size_t i = 0; i < subs.size(); ++i)
		{
			if(subs[i].px > 20)
			{
				subs[i].px = std::max(20, (int)(subs[i].px * 0.9));
				shrunk = true;
			}
		}
		if(!shrunk) break;
	}

	// keep the last lines that fit (statistics screens can be long)
	size_t first = 0;
	if(!subs.empty())
	{
		double h_from = 0.0;
		first = subs.size();
		for(size_t i = subs.size(); i-- > 0; )
		{
			float lh;
			font_line_height(subs[i].px, lh);
			if(h_from + lh > avail_h && first != subs.size()) break;
			h_from += lh;
			first = i;
		}
	}

	double y = (double)text_top;
	for(size_t i = first; i < subs.size(); ++i)
	{
		float lh;
		font_line_height(subs[i].px, lh);
		blit_runs(subs[i].runs, margin, (int)y, (float)y + font_ascent(subs[i].px), subs[i].px);
		y += lh;
	}

	draw_buttons();
	draw_bar();

	if(g_damage.w > 0 && g_damage.h > 0)
	{
		bool ok = (SDL_UpdateTexture(g_texture, &g_damage, g_pixels + (size_t)g_damage.y * g_w, g_w * 4) == 0);
		if(ok) ok = (SDL_RenderCopy(g_renderer, g_texture, &g_damage, &g_damage) == 0);
		if(!ok) g_full_repaint = true;   // fall back to a full repaint next frame
		SDL_RenderPresent(g_renderer);
	}
	g_prev_damage = g_damage;

	dump_frame();

	// frame cost, for logcat on the phone
	{
		static Uint32 acc = 0, worst = 0, last_log = 0;
		static int cnt = 0;
		Uint32 dt = SDL_GetTicks() - t_start;
		acc += dt; ++cnt;
		if(dt > worst) worst = dt;
		Uint32 now = SDL_GetTicks();
		if(cnt >= 30 || (now - last_log) > 3000)
		{
			SDL_Log("chinese_memo: %d frames, avg %ums, worst %ums, last damage %dx%d",
				cnt, cnt ? (unsigned)(acc / cnt) : 0u, worst, g_damage.w, g_damage.h);
			acc = 0; worst = 0; cnt = 0; last_log = now;
		}
	}
}

static void toggle_ime(bool on)
{
	if(on)
	{
		// Android hides the soft keyboard on its own after a touch outside the
		// input area, so re-assert it every time a typed answer is expected
		// instead of trusting the cached state.
		g_ime_on = true;
		SDL_StartTextInput();
	}
	else
	{
		if(!g_ime_on) return;
		g_ime_on = false;
		SDL_StopTextInput();
	}
}

// ---------------------------------------------------------------------------
// event loop -> synthesized keystrokes
//
// ncurses semantics reproduced: each call consumes exactly one "key":
//   7 / 127      -> backspace
//   10 (\n)      -> advance INPUT_STATE (i.e. Enter)
//   anything else-> append to raw_input
// A button tap pushes the decimal digits of the option number + Enter, so
// "tap button 7" is literally the keystrokes "7" then Enter.
// ---------------------------------------------------------------------------

static bool wait_key_queued(void)
{
	for(;;)
	{
		if(g_keysfile) poll_keys_file();
		if(!g_keys.empty()) return true;
		if(g_keysfile)
		{
			// driver-driven run: never block in SDL_WaitEvent, no events will come
			SDL_Delay(20);
			continue;
		}
		SDL_Event ev;
		if(!SDL_WaitEvent(&ev)) continue;
		switch(ev.type)
		{
			case SDL_QUIT:
				exit(0);
				break;
			case SDL_FINGERDOWN:
			{
				float fx = ev.tfinger.x * (float)g_w;
				float fy = ev.tfinger.y * (float)g_h;
				bool hit = false;
				for(size_t i = 0; i < g_button_rects.size() && !hit; ++i)
				{
					const SDL_Rect& rc = g_button_rects[i];
					if(fx >= rc.x && fx < rc.x + rc.w && fy >= rc.y && fy < rc.y + rc.h)
					{
						if(i < chinese::current_options().size())
						{
							push_key_string(std::to_string(i + 1));
							push_key_string("\n");
						}
						else
						{
							// the gold cell after the last option = Enter with nothing
							// typed, which closes the answer exactly like the console
							push_key_string("\n");
						}
						hit = true;
					}
				}
				for(size_t i = 0; i < g_bar_rects.size() && !hit; ++i)
				{
					const SDL_Rect& rc = g_bar_rects[i];
					if(fx >= rc.x && fx < rc.x + rc.w && fy >= rc.y && fy < rc.y + rc.h)
					{
						push_key_string(g_bar_keys[i]);
						hit = true;
					}
				}
				if(!hit && inputbar_active())
				{
					// taps anywhere else re-summon the soft keyboard (Android hides it
					// on a tap outside the input area, and SDL has no visible caret)
					SDL_StartTextInput();
					g_ime_on = true;
				}
				break;
			}
			case SDL_KEYDOWN:
				switch(ev.key.keysym.sym)
				{
					case SDLK_BACKSPACE:
					case SDLK_AC_BACK:
						g_keys.push_back(127);
						break;
					case SDLK_RETURN:
					case SDLK_KP_ENTER:
						g_keys.push_back('\n');
						break;
					case SDLK_ESCAPE:
						push_key_string("exit\n");
						break;
					default:
						break;
				}
				break;
			case SDL_TEXTINPUT:
			{
				std::string s = ev.text.text;
				if(s == "exit") { push_key_string("exit\n"); break; }
				for(size_t i = 0; i < s.size(); ++i)
					if(s[i] >= 32 && s[i] < 127) g_keys.push_back((unsigned char)s[i]);
				break;
			}
			case SDL_WINDOWEVENT:
				if(ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
				   ev.window.event == SDL_WINDOWEVENT_RESIZED)
				{
					int nw = 0, nh = 0;
					SDL_GL_GetDrawableSize(g_window, &nw, &nh);
					if(nw == 0 || nh == 0) SDL_GetWindowSize(g_window, &nw, &nh);
					if(nw > 0 && nh > 0 && (nw != g_w || nh != g_h))
					{
						if(g_texture) SDL_DestroyTexture(g_texture);
						delete[] g_pixels;
						g_texture = NULL;
						g_pixels = NULL;
						g_w = nw; g_h = nh;
						g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, g_w, g_h);
						g_pixels = new Uint32[(size_t)g_w * g_h];
					}
					g_damage.x = g_damage.y = g_damage.w = g_damage.h = 0;
					g_prev_damage.x = g_prev_damage.y = g_prev_damage.w = g_prev_damage.h = 0;
					g_full_repaint = true;
				}
				else if(ev.window.event == SDL_WINDOWEVENT_EXPOSED ||
				        ev.window.event == SDL_WINDOWEVENT_SHOWN ||
				        ev.window.event == SDL_WINDOWEVENT_RESTORED)
				{
					// the screen contents are unknown again (Android also throws the
					// surface away when the app goes to the background)
					g_full_repaint = true;
				}
				break;
			default:
				break;
		}
	}
}

// ---------------------------------------------------------------------------
// Input member implementations
// ---------------------------------------------------------------------------

void chinese::Input::move_clear_refresh() const
{
	if(!g_partial.empty())
	{
		commit_line(g_partial);
		g_partial.clear();
	}
	g_lines.clear();
	g_cur_fg = DEF_FG;
	g_cur_bg = DEF_BG;
}

std::string chinese::Input::do_input_inner_raw(
	std::string& debug_string,
	std::string& raw_input,
	INPUT_STATE& state_number
) const
{
	g_active_state = (int)state_number;
	toggle_ime(state_number != INPUT_STATE_CHOOSE_NUMBER);
	present();

	if(g_keysfile) poll_keys_file();
	write_state("WAIT");
	if(g_keys.empty()) wait_key_queued();
	unsigned char ch = g_keys.empty() ? 0 : g_keys.front();
	if(!g_keys.empty()) g_keys.pop_front();

	if(ch == 'a' || ch == 'z' || ch == 'A' || ch == 'Z' || ch == '0' || ch == '9')
	{
		debug_string += std::string("char(") + (char)ch + ")";
	}
	else
	{
		debug_string += std::string("code(") + std::to_string((int)ch) + ")";
	}

	if(((int)ch) == 7 || ((int)ch) == 127)
	{
		raw_input = raw_input.substr(0, std::max(0, ((int)raw_input.size()) - 1));
	}
	else if(((int)ch) == 10)
	{
		state_number = (INPUT_STATE)(((int)state_number) + 1);
	}
	else
	{
		raw_input += (char)ch;
	}
	if(raw_input == "exit")
	{
		state_number = INPUT_STATE_EXIT;
		raw_input = "";
	}

	g_active_state = (int)state_number;
	return raw_input;
}

std::string chinese::Input::do_input_inner(
	std::string& debug_string,
	std::string& raw_input,
	INPUT_STATE& state_number
) const
{
	do_input_inner_raw(debug_string, raw_input, state_number);
	std::string pinyin = pinyin_convert_1syll(raw_input);
	return pinyin;
}

void chinese::Input::system_pause() const
{
	present();
	if(g_scripted) return;   // headless test run: no user is watching
	while(!g_keys.empty()) g_keys.pop_front();
	present();
	for(;;)
	{
		SDL_Event ev;
		if(!SDL_WaitEvent(&ev)) continue;
		if(ev.type == SDL_QUIT) exit(0);
		if(ev.type == SDL_FINGERDOWN) return;
		if(ev.type == SDL_KEYDOWN)
		{
			SDL_Keycode k = ev.key.keysym.sym;
			if(k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE ||
			   k == SDLK_AC_BACK || k == SDLK_BACKSPACE || k == SDLK_DOWN || k == SDLK_RIGHT)
				return;
			continue;
		}
		if(ev.type == SDL_TEXTINPUT) return;
	}
}

void chinese::Input::init() const
{
	if(g_inited) return;

	if(SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return;
	}

	int w = 0, h = 0;
#ifndef __ANDROID__
	if(getenv("CM_WINDOW")) sscanf(getenv("CM_WINDOW"), "%dx%d", &w, &h);
#endif
	if(w <= 0 || h <= 0)
	{
		SDL_DisplayMode dm;
		if(SDL_GetDesktopDisplayMode(0, &dm) == 0) { w = dm.w; h = dm.h; }
		else { w = 1080; h = 1920; }
	}

	Uint32 flags = 0;
#ifdef __ANDROID__
	flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
	if(getenv("SDL_VIDEODRIVER") && strcmp(getenv("SDL_VIDEODRIVER"), "dummy") == 0)
		flags |= SDL_WINDOW_RESIZABLE;
#endif

	g_window = SDL_CreateWindow("chinese_memo", 0, 0, w, h, flags);
	if(!g_window)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return;
	}
	g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
	if(!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, 0);
	if(!g_renderer)
	{
		fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		return;
	}

	SDL_GL_GetDrawableSize(g_window, &w, &h);
	int dw = w, dh = h;
	if(w <= 0 || h <= 0) SDL_GetWindowSize(g_window, &w, &h);
	if(w > 0 && h > 0)
	{
		g_w = w; g_h = h;
		g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, g_w, g_h);
		if(g_texture) g_pixels = new Uint32[(size_t)g_w * g_h];
		else g_texture = NULL;
	}
	{
		int ww = 0, wh = 0;
		SDL_GetWindowSize(g_window, &ww, &wh);
		// logcat tells us the real surface size on the phone: the soft keyboard
		// and the status bar both change what "the screen" means
		SDL_Log("chinese_memo: drawable=%dx%d window=%dx%d texture=%dx%d", dw, dh, ww, wh, g_w, g_h);
	}

	FILE* f = fopen("NotoSansSC.ttf", "rb");
	if(!f) f = fopen("assets/NotoSansSC.ttf", "rb");
	if(f)
	{
		fseek(f, 0, SEEK_END);
		long sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		g_fontdata.resize((size_t)sz);
		size_t got = fread(&g_fontdata[0], 1, (size_t)sz, f);
		fclose(f);
		if((long)got == sz && stbtt_InitFont(&g_font, &g_fontdata[0], stbtt_GetFontOffsetForIndex(&g_fontdata[0], 0)))
			g_font_ok = true;
	}
	if(!g_font_ok)
	{
		fprintf(stderr, "chinese_memo: FATAL: failed to load NotoSansSC.ttf from the working directory\n");
		exit(1);
	}

	g_cout_backup = std::cout.rdbuf();
	std::cout.rdbuf(&g_screenbuf);

	// headless test hook: CM_AUTOKEYS injects a scripted session
	// ("zhong\n1\nmo\n2\n"), so rendering can be checked without a screen.
	const char* autokeys = getenv("CM_AUTOKEYS");
	if(autokeys)
	{
		g_scripted = true;
		std::string s = autokeys;
		std::string decoded;
		for(size_t i = 0; i < s.size(); ++i)
		{
			if(s[i] == '\\' && i + 1 < s.size())
			{
				++i;
				if(s[i] == 'n') decoded.push_back('\n');
				else if(s[i] == 'r') decoded.push_back('\r');
				else if(s[i] == 't') decoded.push_back('\t');
				else decoded.push_back(s[i]);
			}
			else decoded.push_back(s[i]);
		}
		push_key_string(decoded);
	}

	// headless test hook: a driver script appends keystrokes to this file and
	// reads the "waiting" marker written by write_state(). On the phone there
	// are no environment variables, so "cm_autokeys" in the working directory
	// (the app data directory) does the same thing.
	const char* keysfile = getenv("CM_AUTOKEYS_FILE");
	if(keysfile && keysfile[0])
	{
		g_keysfile = keysfile;
		g_scripted = true;
	}
	else
	{
		FILE* probe = fopen("cm_autokeys", "rb");
		if(probe)
		{
			fclose(probe);
			g_keysfile = "cm_autokeys";
			g_scripted = true;
		}
	}
	const char* statefile = getenv("CM_STATE_FILE");
	if(statefile && statefile[0]) g_statefile = statefile;
#ifdef __ANDROID__
	else g_statefile = "cm_state";
#endif

	g_inited = true;
	g_active_state = (int)INPUT_STATE_TYPE_PINYIN;
	present();
}

void chinese::Input::close() const
{
	if(!g_inited) return;
	std::cout.flush();
	if(g_cout_backup) std::cout.rdbuf(g_cout_backup);
	g_cout_backup = nullptr;
	present();
	toggle_ime(false);
	SDL_Quit();
	g_inited = false;
}

int chinese::Input::cols() const
{
	return 42;
}
