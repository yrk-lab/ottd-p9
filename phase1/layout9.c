/*
 * layout9: the FreeType structs OpenTTD reads (fontcache/
 * freetypefontcache.cpp), their sizes and their fields' offsets, one
 * per line.  Compiled twice, the output must be the same: by clang
 * against ft9.sh's headers (what the translated code was compiled
 * against) and by 6c with FreeType's own Plan 9 flags (the library it
 * links): build.rc compares them.  Both sides print with printf: on
 * Plan 9 p9lib.h brings in its stdio.
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#define SIZE(t)		printf("sizeof %s %d\n", #t, (int)sizeof(t))
#define OFF(t, f)	printf("%s.%s %d\n", #t, #f, (int)((char*)&((t*)0)->f - (char*)0))

int
main(void)
{
	SIZE(FT_Vector);
	SIZE(FT_Bitmap);
	SIZE(FT_Bitmap_Size);
	SIZE(FT_Glyph_Metrics);
	SIZE(FT_Size_Metrics);
	SIZE(FT_SizeRec);
	SIZE(FT_CharMapRec);
	SIZE(FT_GlyphSlotRec);
	SIZE(FT_FaceRec);
	SIZE(TT_Header);
	SIZE(FT_Generic);
	SIZE(FT_BBox);
	SIZE(FT_Outline);

	OFF(FT_Bitmap, rows);
	OFF(FT_Bitmap, width);
	OFF(FT_Bitmap, pitch);
	OFF(FT_Bitmap, buffer);
	OFF(FT_Bitmap, num_grays);
	OFF(FT_Bitmap, pixel_mode);
	OFF(FT_Bitmap_Size, height);
	OFF(FT_Bitmap_Size, width);
	OFF(FT_Bitmap_Size, size);
	OFF(FT_Size_Metrics, x_ppem);
	OFF(FT_Size_Metrics, x_scale);
	OFF(FT_Size_Metrics, ascender);
	OFF(FT_Size_Metrics, descender);
	OFF(FT_Size_Metrics, height);
	OFF(FT_Size_Metrics, max_advance);
	OFF(FT_SizeRec, face);
	OFF(FT_SizeRec, generic);
	OFF(FT_SizeRec, metrics);
	OFF(FT_SizeRec, internal);
	OFF(FT_CharMapRec, face);
	OFF(FT_CharMapRec, encoding);
	OFF(FT_CharMapRec, platform_id);
	OFF(FT_CharMapRec, encoding_id);
	OFF(FT_GlyphSlotRec, library);
	OFF(FT_GlyphSlotRec, face);
	OFF(FT_GlyphSlotRec, next);
	OFF(FT_GlyphSlotRec, glyph_index);
	OFF(FT_GlyphSlotRec, generic);
	OFF(FT_GlyphSlotRec, metrics);
	OFF(FT_GlyphSlotRec, linearHoriAdvance);
	OFF(FT_GlyphSlotRec, linearVertAdvance);
	OFF(FT_GlyphSlotRec, advance);
	OFF(FT_GlyphSlotRec, format);
	OFF(FT_GlyphSlotRec, bitmap);
	OFF(FT_GlyphSlotRec, bitmap_left);
	OFF(FT_GlyphSlotRec, bitmap_top);
	OFF(FT_GlyphSlotRec, outline);
	OFF(FT_GlyphSlotRec, num_subglyphs);
	OFF(FT_GlyphSlotRec, subglyphs);
	OFF(FT_GlyphSlotRec, control_data);
	OFF(FT_GlyphSlotRec, control_len);
	OFF(FT_GlyphSlotRec, lsb_delta);
	OFF(FT_GlyphSlotRec, rsb_delta);
	OFF(FT_GlyphSlotRec, other);
	OFF(FT_GlyphSlotRec, internal);
	OFF(FT_FaceRec, num_faces);
	OFF(FT_FaceRec, face_index);
	OFF(FT_FaceRec, face_flags);
	OFF(FT_FaceRec, style_flags);
	OFF(FT_FaceRec, num_glyphs);
	OFF(FT_FaceRec, family_name);
	OFF(FT_FaceRec, style_name);
	OFF(FT_FaceRec, num_fixed_sizes);
	OFF(FT_FaceRec, available_sizes);
	OFF(FT_FaceRec, num_charmaps);
	OFF(FT_FaceRec, charmaps);
	OFF(FT_FaceRec, generic);
	OFF(FT_FaceRec, bbox);
	OFF(FT_FaceRec, units_per_EM);
	OFF(FT_FaceRec, ascender);
	OFF(FT_FaceRec, descender);
	OFF(FT_FaceRec, height);
	OFF(FT_FaceRec, max_advance_width);
	OFF(FT_FaceRec, underline_position);
	OFF(FT_FaceRec, glyph);
	OFF(FT_FaceRec, size);
	OFF(FT_FaceRec, charmap);
	OFF(FT_FaceRec, driver);
	OFF(FT_FaceRec, memory);
	OFF(FT_FaceRec, stream);
	OFF(FT_FaceRec, sizes_list);
	OFF(FT_FaceRec, autohint);
	OFF(FT_FaceRec, extensions);
	OFF(FT_FaceRec, internal);
	OFF(TT_Header, Table_Version);
	OFF(TT_Header, Units_Per_EM);
	OFF(TT_Header, Lowest_Rec_PPEM);
	return 0;
}
