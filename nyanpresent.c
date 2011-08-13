/*
 * Copyright (c) 2011  hunz
 *
 * based on Cairo SDL clock:
   * Made by Writser Cleveringa, based upon code from Eric Windisch.
   * Minor code clean up by Chris Nystrom (5/21/06) and converted to cairo-sdl
   * by Chris Wilson and converted to cairosdl by M Joonas Pihlaja.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <poppler.h>
#include "SDL_image.h"
#include "cairosdl.h"

static void
draw_page(SDL_Surface * dst, cairo_t * cr, PopplerDocument * document,
	  int page_num)
{
	cairo_status_t status;
	PopplerPage *page;

	/* Create a cairo drawing context, normalize it and draw a clock. */
	SDL_LockSurface(dst);
	{

		page = poppler_document_get_page(document, page_num - 1);
		if (page == NULL) {
			printf("poppler fail: page not found\n");
			return;
		}

		cairo_save(cr);
		poppler_page_render(page, cr);
		cairo_restore(cr);
		g_object_unref(page);

		status = cairo_status(cr);
	}
	SDL_UnlockSurface(dst);

	/* Nasty nasty error handling. */
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Unable to create or draw with a cairo context "
			"for the screen: %s\n", cairo_status_to_string(status));
		exit(1);
	}
}

static SDL_Surface *init_screen(int width, int height, int bpp)
{
	SDL_Surface *screen;

	/* Initialize SDL */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		fprintf(stderr, "Unable to initialize SDL: %s\n",
			SDL_GetError());
		exit(1);
	}

	/* Open a screen with the specified properties */
	screen = SDL_SetVideoMode(width, height, bpp, SDL_HWSURFACE |	//SDL_SRCALPHA |
				  SDL_FULLSCREEN);
	if (screen == NULL) {
		fprintf(stderr, "Unable to set %ix%i video: %s\n",
			width, height, SDL_GetError());
		exit(1);
	}

	return screen;
}

/* This function pushes a custom event onto the SDL event queue.
 * Whenever the main loop receives it, the window will be redrawn.
 * We can't redraw the window here, since this function could be called
 * from another thread.
 */
static Uint32 timer_cb(Uint32 interval, void *param)
{
	SDL_Event event;

	event.type = SDL_USEREVENT;
	SDL_PushEvent(&event);

	(void)param;
	(void)interval;
//      return interval;
	return 0;
}

#define NYAN_TOP

void make_nyans(SDL_Surface * d, SDL_Surface * s)
{
	SDL_Rect sr, dr;
	uint8_t *a = d->pixels;
	int i;

	dr.x = d->w - s->w;
	dr.y = 0;
	dr.w = s->w;
	dr.h = s->h;
	SDL_SetAlpha(s, 0, 0);
	SDL_BlitSurface(s, NULL, d, &dr);
	sr.x = 0;
	sr.y = 0;
	sr.w = 26;
	sr.h = s->h;
	for (dr.x = dr.x - 26; dr.x > 0; dr.x -= 26) {
		SDL_BlitSurface(s, &sr, d, &dr);
	}
	sr.x = -dr.x;
	sr.w = 26 + dr.x;
	dr.w = sr.w;
	dr.x = 0;
	SDL_BlitSurface(s, &sr, d, &dr);
	for (i = 0; i < d->w * d->h; i++, a += 4) {
		float val = *a;
		val *= 0.75;
		*a = val;
	}
}

int main(int argc, char **argv)
{
	SDL_Surface *screen;
	SDL_Event event;
	PopplerDocument *document;
	GError *error;
	const char *pdf_file;
	gchar *absolute, *uri;
	int page_num = 1, num_pages;
	cairo_t *cr;
	double width, height;
	float ofs;
	PopplerPage *page;
	SDL_TimerID t = 0;
	SDL_Surface *pg_sf;
	SDL_Surface *n1, *n2, *n1l, *n2l;
	SDL_Surface **preld_sf = NULL;
	SDL_Surface *src_sf = NULL;
	int prerender = 1;

	if (argc < 2) {
		printf("Usage: %s input_file.pdf (pagenum)\n",argv[0]);
		return 0;
	}

	pdf_file = argv[1];
	if (argc > 2)
		page_num = atoi(argv[2]);
	g_type_init();
	error = NULL;

	n1 = IMG_Load("1_32.png");
	n2 = IMG_Load("2_32.png");

	if (g_path_is_absolute(pdf_file)) {
		absolute = g_strdup(pdf_file);
	} else {
		gchar *dir = g_get_current_dir();
		absolute = g_build_filename(dir, pdf_file, (gchar *) 0);
		free(dir);
	}

	uri = g_filename_to_uri(absolute, NULL, &error);
	free(absolute);
	if (uri == NULL) {
		printf("%s\n", error->message);
		return 1;
	}

	document = poppler_document_new_from_file(uri, NULL, &error);
	if (document == NULL) {
		printf("%s\n", error->message);
		return 1;
	}

	num_pages = poppler_document_get_n_pages(document);
	if (page_num < 1 || page_num > num_pages) {
		printf("page must be between 1 and %d\n", num_pages);
		return 1;
	}

	page = poppler_document_get_page(document, 0);
	if (page == NULL) {
		printf("poppler fail: page not found\n");
		return 1;
	}

	/* Initialize SDL, open a screen */
	screen = init_screen(1024, 768, 32);

	n1l = SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCALPHA,
				   screen->w,
				   32,
				   32,
				   0xff000000,
				   0x00ff0000, 0x0000ff00, 0x000000ff);

	n2l = SDL_CreateRGBSurface(SDL_HWSURFACE | SDL_SRCALPHA,
				   screen->w,
				   32,
				   32,
				   0xff000000,
				   0x00ff0000, 0x0000ff00, 0x000000ff);

	make_nyans(n1l, n1);
	make_nyans(n2l, n2);

	pg_sf = SDL_CreateRGBSurface(SDL_HWSURFACE | 0,
				     screen->w,
				     screen->h,
				     32,
				     CAIROSDL_RMASK,
				     CAIROSDL_GMASK, CAIROSDL_BMASK, 0);

	cr = cairosdl_create(pg_sf);

	poppler_page_get_size(page, &width, &height);
	g_object_unref(page);

	cairo_scale(cr, screen->w / width, screen->h / height);
	draw_page(pg_sf, cr, document, page_num);
	SDL_BlitSurface(pg_sf, NULL, screen, NULL);
	SDL_Flip(screen);

	if (prerender) {
		int i;
		preld_sf = malloc(sizeof(SDL_Surface *) * num_pages);
		for (i = 0; i < num_pages; i++) {
			preld_sf[i] = SDL_CreateRGBSurface(SDL_HWSURFACE | 0,
							   screen->w,
							   screen->h,
							   32,
							   CAIROSDL_RMASK,
							   CAIROSDL_GMASK,
							   CAIROSDL_BMASK, 0);
			draw_page(pg_sf, cr, document, i + 1);
			SDL_BlitSurface(pg_sf, NULL, preld_sf[i], NULL);
		}
	}

	while (SDL_WaitEvent(&event)) {
		int new_page = 0;
		switch (event.type) {
		case SDL_KEYDOWN:
			if (event.key.keysym.sym == SDLK_ESCAPE) {
				goto done;
			} else if (event.key.keysym.sym == SDLK_SPACE) {
				new_page = 1;
				++page_num;
			} else if (event.key.keysym.sym == SDLK_RIGHT) {
				new_page = 1;
				++page_num;
			} else if (event.key.keysym.sym == SDLK_LEFT) {
				new_page = 1;
				--page_num;
			} else if (event.key.keysym.sym == SDLK_PAGEUP) {
				new_page = 1;
				--page_num;
			} else if (event.key.keysym.sym == SDLK_PAGEDOWN) {
				new_page = 1;
				++page_num;
			}

			if (new_page) {
				SDL_Rect sr, sd;
				float x;

				SDL_RemoveTimer(t);

				if (page_num > num_pages)
					page_num = num_pages;
				if (page_num < 1)
					page_num = 1;

				src_sf = pg_sf;
				if (!prerender)
					draw_page(pg_sf, cr, document,
						  page_num);
				else {
					src_sf = preld_sf[page_num - 1];
				}

				SDL_BlitSurface(src_sf, NULL, screen, NULL);
				
				ofs = num_pages - page_num;
				ofs /= num_pages;
				x = n1l->w;
				x *= ofs;
				sr.x = x;
				sr.w = n1l->w - x;
				sr.h = n1l->h;
				sr.y = 0;
#ifndef NYAN_TOP
				sd.y = screen->h - n1l->h;
#else
				sd.y = 0;
#endif
				sd.w = sr.w;
				sd.x = 0;
				sd.h = sr.h;
				SDL_BlitSurface(page_num & 1 ? n1l : n2l, &sr,
						screen, &sd);
				SDL_Flip(screen);
				t = SDL_AddTimer(1000, timer_cb, NULL);
			}
			break;

		case SDL_QUIT:
			goto done;

		case SDL_USEREVENT:
			SDL_BlitSurface(src_sf, NULL, screen, NULL);
			SDL_Flip(screen);
			break;

		default:
			break;
		}
	}

 done:
	SDL_FreeSurface(screen);
	SDL_Quit();
	return 0;
}
