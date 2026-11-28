/*
 * FreeType where the translated code cannot have it: the program is
 * compiled against FreeType's headers in Plan 9's data model
 * (../../phase1/ft9.sh: long is 32 bits), which only 9front's own
 * libfreetype.a6 matches.  goken, the Linux fast loop and
 * build-p9's native link get this instead: FreeType does not start,
 * and OpenTTD keeps its sprite fonts ("Unable to initialize
 * FreeType").  The rest is never called; it is here for the link.
 * Pointers and ints only: no FreeType header.
 */

enum
{
	Err	= 1,	/* FT_Err_Cannot_Open_Resource: any error will do */
};

int
FT_Init_FreeType(void **lib)
{
	*lib = 0;
	return Err;
}

int
FT_Done_FreeType(void *lib)
{
	(void)lib;
	return 0;
}

void
FT_Library_Version(void *lib, int *major, int *minor, int *patch)
{
	(void)lib;
	*major = 0;
	*minor = 0;
	*patch = 0;
}

int
FT_New_Face(void *lib, char *path, int index, void **face)
{
	(void)lib;
	(void)path;
	(void)index;
	*face = 0;
	return Err;
}

int
FT_Done_Face(void *face)
{
	(void)face;
	return 0;
}

int
FT_Select_Charmap(void *face, int enc)
{
	(void)face;
	(void)enc;
	return Err;
}

int
FT_Set_Charmap(void *face, void *map)
{
	(void)face;
	(void)map;
	return Err;
}

int
FT_Set_Pixel_Sizes(void *face, unsigned w, unsigned h)
{
	(void)face;
	(void)w;
	(void)h;
	return Err;
}

int
FT_Select_Size(void *face, int i)
{
	(void)face;
	(void)i;
	return Err;
}

unsigned
FT_Get_Char_Index(void *face, unsigned c)
{
	(void)face;
	(void)c;
	return 0;
}

int
FT_Load_Glyph(void *face, unsigned g, int flags)
{
	(void)face;
	(void)g;
	(void)flags;
	return Err;
}

int
FT_Render_Glyph(void *slot, int mode)
{
	(void)slot;
	(void)mode;
	return Err;
}

void*
FT_Get_Sfnt_Table(void *face, int tag)
{
	(void)face;
	(void)tag;
	return 0;
}
