/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

/*
 * The Gnucash Grid drawing functions
 *
 * The gnucash grid used to be a fully fledged object. However it has no
 * intelligence of its own, so instead I have reduced it to just a complex
 * drawing function.
 * It specializes in drawing the non-editable parts of a gnucash sheet, that is
 * - the rows and columns with proper backgrounds and borders
 * - the text for each cell in the table
 * - dividing lines (red and blue)
 *
 * This could have been addedi directly into gnucash_sheet.c, but is kept
 * separate to not oversize that file.
 */

#include "config.h"

#include <string.h>

#include "gnucash-sheet.h"
#include "gnucash-sheetP.h"
#include "gnucash-color.h"
#include "gnucash-style.h"

/*
 * Sets virt_row, virt_col to the block coordinates for the
 * block containing pixel (x, y).  Also sets o_x, o_y, to the
 * pixel coordinates of the origin of the block.  Returns
 * TRUE if a block is found, FALSE if (x,y) is not
 * in any block.
 *
 * All coordinates are with respect to the canvas origin.
 */
static SheetBlock *
find_block_by_pixel (GnucashSheet *sheet,
                     gint x, gint y,
                     VirtualCellLocation *vcell_loc)
{
    SheetBlock *block;
    VirtualCellLocation vc_loc = { 1, 0 };

    g_return_val_if_fail(y >= 0, NULL);
    g_return_val_if_fail(x >= 0, NULL);

    do
    {
        block = gnucash_sheet_get_block (sheet, vc_loc);
        if (!block)
            return NULL;

        if (block->visible &&
                y >= block->origin_y &&
                y < block->origin_y + block->style->dimensions->height)
        {
            if (vcell_loc)
                vcell_loc->virt_row = vc_loc.virt_row;
            break;
        }
        vc_loc.virt_row++;
    }
    while (vc_loc.virt_row < sheet->num_virt_rows);

    if (vc_loc.virt_row == sheet->num_virt_rows)
        return NULL;

    do
    {
        block = gnucash_sheet_get_block (sheet, vc_loc);
        if (!block)
            return NULL;

        if (block->visible &&
                x >= block->origin_x &&
                x < block->origin_x + block->style->dimensions->width)
        {
            if (vcell_loc)
                vcell_loc->virt_col = vc_loc.virt_col;
            break;
        }
        vc_loc.virt_col++;
    }
    while (vc_loc.virt_col < sheet->num_virt_cols);

    if (vc_loc.virt_col == sheet->num_virt_cols)
        return NULL;

    return block;
}

static gboolean
find_cell_by_pixel (GnucashSheet *sheet, gint x, gint y,
                    VirtualLocation *virt_loc)
{
    SheetBlock *block;
    SheetBlockStyle *style;
    CellDimensions *cd;
    gint row = 0;
    gint col = 0;

    g_return_val_if_fail (virt_loc != NULL, FALSE);

    block = gnucash_sheet_get_block (sheet, virt_loc->vcell_loc);
    if (block == NULL)
        return FALSE;

    /* now make x, y relative to the block origin */
    x -= block->origin_x;
    y -= block->origin_y;

    style = block->style;
    if (style == NULL)
        return FALSE;

    do
    {
        cd = gnucash_style_get_cell_dimensions (style, row, 0);

        if (y >= cd->origin_y && y < cd->origin_y + cd->pixel_height)
            break;

        row++;
    }
    while (row < style->nrows);

    if (row == style->nrows)
        return FALSE;

    do
    {
        cd = gnucash_style_get_cell_dimensions (style, row, col);

        if (x >= cd->origin_x && x < cd->origin_x + cd->pixel_width)
            break;

        col++;
    }
    while (col < style->ncols);

    if (col == style->ncols)
        return FALSE;

    if (virt_loc)
        virt_loc->phys_row_offset = row;
    if (virt_loc)
        virt_loc->phys_col_offset = col;

    return TRUE;
}

gboolean
gnucash_sheet_find_loc_by_pixel (GnucashSheet *sheet, gint x, gint y,
                                 VirtualLocation *virt_loc)
{
    SheetBlock *block;

    if (virt_loc == NULL)
        return FALSE;

    block = find_block_by_pixel (sheet, x, y,
            &virt_loc->vcell_loc);
    if (block == NULL)
        return FALSE;

    return find_cell_by_pixel (sheet, x, y, virt_loc);
}

static void
get_cell_borders (GnucashSheet *sheet, VirtualLocation virt_loc,
                  PhysicalCellBorders *borders)
{
    VirtualLocation v_loc;
    PhysicalCellBorders neighbor;

    gnucash_sheet_get_borders (sheet, virt_loc, borders);

    /* top */
    v_loc = virt_loc;
    if (gnc_table_move_vertical_position (sheet->table, &v_loc, -1))
    {
        gnucash_sheet_get_borders (sheet, v_loc, &neighbor);
        borders->top = MAX (borders->top, neighbor.bottom);
    }

    /* bottom */
    v_loc = virt_loc;
    if (gnc_table_move_vertical_position (sheet->table, &v_loc, 1))
    {
        gnucash_sheet_get_borders (sheet, v_loc, &neighbor);
        borders->bottom = MAX (borders->bottom, neighbor.top);
    }

    /* left */
    v_loc = virt_loc;
    v_loc.phys_col_offset--;
    if (gnc_table_virtual_loc_valid (sheet->table, v_loc, TRUE))
    {
        gnucash_sheet_get_borders (sheet, v_loc, &neighbor);
        borders->left = MAX (borders->left, neighbor.right);
    }

    /* right */
    v_loc = virt_loc;
    v_loc.phys_col_offset++;
    if (gnc_table_virtual_loc_valid (sheet->table, v_loc, TRUE))
    {
        gnucash_sheet_get_borders (sheet, v_loc, &neighbor);
        borders->right = MAX (borders->right, neighbor.left);
    }
}


#ifdef READONLY_LINES_WITH_CHANGED_FG_COLOR
/** For a given byte value, multiply the difference to 0xFF by a rational number,
specified by numerator and denominator. This is some simple integer arithmetics
for the case when we don't even need a conversion to floating point and
backwards. */
static guint8 inc_intensity_byte(guint8 input, int numerator, int denominator)
{
    guint8 result_inv, result;
    guint8 input_inv = 0xff - input;
    result_inv = (input_inv * numerator) / denominator;
    result = 0xff - result_inv;
    return result;
}

/** For a given RGB value, increase the color intensity for each of the three
colors indentically by 10 percent (i.e. make them "less black" and "more gray")
and return this changed RGB value. */
static guint32 inc_intensity_10percent(guint32 argb)
{
    guint32 result =
            (inc_intensity_byte((argb & 0x00FF0000) >> 16, 8, 10) << 16)
            + (inc_intensity_byte((argb & 0x0000FF00) >> 8, 8, 10) << 8)
            + (inc_intensity_byte(argb & 0x000000FF, 8, 10));
    return result;
}
#endif

/** For a given byte value, multiply the value by a rational number,
specified by numerator and denominator. This is some simple integer arithmetics
for the case when we don't even need a conversion to floating point and
backwards. */
static guint8 dec_intensity_byte(guint8 input, int numerator, int denominator)
{
    guint8 result;
    result = (input * numerator) / denominator;
    return result;
}

/** For a given RGB value, decrease the color intensity for each of the three
colors indentically by 10 percent and return this changed RGB value. */
static guint32 dec_intensity_10percent(guint32 argb)
{
    // Multiply each single byte by 9/10 i.e. by 0.9 which decreases the
    // intensity by 10 percent.
    guint32 result =
            (dec_intensity_byte((argb & 0x00FF0000) >> 16, 9, 10) << 16)
            + (dec_intensity_byte((argb & 0x0000FF00) >> 8, 9, 10) << 8)
            + (dec_intensity_byte(argb & 0x000000FF, 9, 10));
    return result;
}

/* Actual drawing routines */

G_INLINE_FUNC void
draw_cell_line (cairo_t *cr, GdkRGBA *bg_color,
                double x1, double y1, double x2, double y2,
                PhysicalCellBorderLineStyle style);

void
draw_cell_line (cairo_t *cr, GdkRGBA *bg_color,
                double x1, double y1, double x2, double y2,
                PhysicalCellBorderLineStyle style)
{
    GdkRGBA *fg_color;

    switch (style)
    {
        case CELL_BORDER_LINE_NONE:
            fg_color = bg_color;
            break;

        case CELL_BORDER_LINE_LIGHT:
            fg_color = &gn_light_gray;
            break;

        case CELL_BORDER_LINE_NORMAL:
        case CELL_BORDER_LINE_HEAVY:
            fg_color = &gn_black;
            break;

        case CELL_BORDER_LINE_HIGHLIGHT:
            fg_color = &gn_red;
            break;

        default:
            return;
    }

    cairo_set_line_width (cr, 1.0);
    cairo_set_source_rgb (cr, fg_color->red, fg_color->green, fg_color->blue);
    cairo_move_to (cr, x1, y1);
    cairo_line_to (cr, x2, y2);
    cairo_stroke (cr);
}

static void
draw_hatching (cairo_t *cr,
               double x, double y, G_GNUC_UNUSED double width, double height)
{
    GdkRGBA *fg_color;
    double h_x = x + 2.5;
    double h_y = y + 2.5;
    double h_size = height / 3 - 1;

    cairo_set_line_width (cr, 1.0);
    fg_color = &gn_light_gray;
    cairo_set_source_rgb (cr, fg_color->red, fg_color->green, fg_color->blue);

    cairo_rectangle (cr, h_x, h_y, h_size, h_size);

    cairo_move_to (cr, h_x, h_y);
    cairo_rel_line_to (cr, h_size, h_size);

    cairo_rel_move_to (cr, -h_x, 0);
    cairo_rel_line_to (cr, h_size, -h_size);
    cairo_stroke (cr);
}

static void
draw_divider_line (cairo_t *cr, VirtualLocation virt_loc,
                   int div_row, int n_phys_rows, GdkRGBA *fg_color,
                   double x, double y, double width, double height)
{
    double offset;
    if (div_row < 0)
        return;

    /* Test if divider line should be drawn before the current row */
    if ((virt_loc.phys_row_offset == 0) &&
        (virt_loc.vcell_loc.virt_row == div_row))
        offset = 0.0;
    /* Test if divider line should be drawn after the current row */
    else if ((virt_loc.phys_row_offset == n_phys_rows - 1) &&
        (virt_loc.vcell_loc.virt_row == div_row - 1))
        offset = height;
    else
        return;

    cairo_set_source_rgb (cr, fg_color->red, fg_color->green, fg_color->blue);

    cairo_set_line_width (cr, 3.0);
    cairo_move_to (cr, x, y + 0.5 + offset);
    cairo_rel_line_to (cr, width, 0);
    cairo_stroke (cr);
}

static void
draw_cell (GnucashSheet *sheet,
           SheetBlock *block,
           VirtualLocation virt_loc,
           cairo_t *cr,
           int x, int y, int width, int height)
{
    Table *table = sheet->table;
    PhysicalCellBorders borders;
    const char *text;
    PangoLayout *layout;
    PangoContext *context;
    PangoFontDescription *font;
    PangoRectangle logical_rect;
    GdkRGBA *bg_color, *fg_color;
    GdkRectangle rect;
    gboolean hatching;
    guint32 argb, color_type;
    int x_offset;

    if (sheet->use_theme_colors)
    {
        color_type = gnc_table_get_gtkrc_bg_color (table, virt_loc,
                     &hatching);
        bg_color = get_gtkrc_color(sheet, color_type);
    }
    else
    {
        argb = gnc_table_get_bg_color (table, virt_loc, &hatching);
        // Are we in a read-only row? Then make the background color somewhat more gray.
        if ((virt_loc.phys_row_offset == (block->style->nrows - 1))
                && (table->model->dividing_row_upper >= 0)
                && (virt_loc.vcell_loc.virt_row < table->model->dividing_row_upper))
        {
            argb = dec_intensity_10percent(argb);
        }
        bg_color = gnucash_color_argb_to_gdk (argb);
    }

    cairo_set_source_rgb (cr, bg_color->red, bg_color->green, bg_color->blue);
    cairo_rectangle (cr, x, y, width, height);
    cairo_fill (cr);

    get_cell_borders (sheet, virt_loc, &borders);

    /* top */
    draw_cell_line (cr, bg_color,
                    (borders.top >= borders.left ? x : x + 1.0),
                    y + 0.5,
                    (borders.top >= borders.right ?
                     x + width : x + width - 1),
                    y + 0.5,
                    borders.top);

    /* bottom */
    draw_cell_line (cr, bg_color,
                    (borders.bottom >= borders.left ? x : x + 1),
                    y + height + 0.5,
                    (borders.bottom >= borders.right ?
                     x + width : x + width - 1),
                    y + height + 0.5,
                    borders.bottom);

    /* left */
    draw_cell_line (cr, bg_color,
                    x + 0.5,
                    (borders.left > borders.top ? y : y + 1),
                    x + 0.5,
                    (borders.left > borders.bottom ?
                     y + height : y + height - 1),
                    borders.left);

    /* right */
    draw_cell_line (cr, bg_color,
                    x + width + 0.5,
                    (borders.right > borders.top ? y : y + 1),
                    x + width + 0.5,
                    (borders.right > borders.bottom ?
                     y + height : y + height - 1),
                    borders.right);

    if (hatching)
        draw_hatching (cr,
                               x, y, width, height);

    /* dividing line upper (red) */
    fg_color = &gn_red;
    draw_divider_line(cr, virt_loc,
                           table->model->dividing_row_upper, block->style->nrows,
                           fg_color, x, y, width, height);

    /* dividing line (blue) */
    fg_color = &gn_blue;
    draw_divider_line(cr, virt_loc,
                           table->model->dividing_row, block->style->nrows,
                           fg_color, x, y, width, height);

    /* dividing line lower (blue) */
    draw_divider_line(cr, virt_loc,
                           table->model->dividing_row_lower, block->style->nrows,
                           fg_color, x, y, width, height);

    text = gnc_table_get_entry (table, virt_loc);

    layout = gtk_widget_create_pango_layout (GTK_WIDGET (sheet), text);
    // We don't need word wrap or line wrap
    pango_layout_set_width (layout, -1);
    context = pango_layout_get_context (layout);
    font = pango_font_description_copy (pango_context_get_font_description (context));

    if (sheet->use_theme_colors)
    {
        color_type = gnc_table_get_gtkrc_fg_color (table, virt_loc);
        fg_color = get_gtkrc_color(sheet, color_type);
    }
    else
    {
        argb = gnc_table_get_fg_color (table, virt_loc);
#ifdef READONLY_LINES_WITH_CHANGED_FG_COLOR
        // Are we in a read-only row? Then make the foreground color somewhat less black
        if ((virt_loc.phys_row_offset == (block->style->nrows - 1))
                && (table->model->dividing_row_upper >= 0)
                && (virt_loc.vcell_loc.virt_row < table->model->dividing_row_upper))
        {
            argb = inc_intensity_10percent(argb);
        }
#endif
        fg_color = gnucash_color_argb_to_gdk (argb);
    }

    /* If this is the currently open transaction and
       there is no text in this cell */
    if ((table->current_cursor_loc.vcell_loc.virt_row ==
            virt_loc.vcell_loc.virt_row) &&
            (!text || strlen(text) == 0))
    {
        text = gnc_table_get_label (table, virt_loc);
        if ((text == NULL) || (*text == '\0'))
            goto exit;
        fg_color = &gn_light_gray;
        pango_layout_set_text (layout, text, strlen (text));
        pango_font_description_set_style (font, PANGO_STYLE_ITALIC);
        pango_context_set_font_description (context, font);
    }
    cairo_set_source_rgb (cr, fg_color->red, fg_color->green, fg_color->blue);

    if ((text == NULL) || (*text == '\0'))
    {
        goto exit;
    }

    pango_layout_get_pixel_extents(layout,
                                   NULL,
                                   &logical_rect);

    rect.x      = x + CELL_HPADDING;
    rect.y      = y + CELL_VPADDING;
    rect.width  = MAX (0, width - (2 * CELL_HPADDING));
    rect.height = height - 2;

    cairo_save (cr);
    cairo_rectangle (cr, rect.x, rect.y, rect.width, rect.height);
    cairo_clip (cr);

    switch (gnc_table_get_align (table, virt_loc))
    {
    default:
    case CELL_ALIGN_LEFT:
        x_offset = 0;
        break;

    case CELL_ALIGN_RIGHT:
        x_offset = width - 2 * CELL_HPADDING - logical_rect.width;
        break;

    case CELL_ALIGN_CENTER:
        if (logical_rect.width > width - 2 * CELL_HPADDING)
            x_offset = 0;
        else
            x_offset = (width - 2 * CELL_HPADDING -
                        logical_rect.width) / 2;
        break;
    }



    cairo_move_to (cr, rect.x + x_offset, rect.y + 1);
    pango_cairo_show_layout (cr, layout);

    cairo_restore (cr);

exit:
    pango_font_description_set_style (font, PANGO_STYLE_NORMAL);
    pango_context_set_font_description (context, font);
    pango_font_description_free (font);
    g_object_unref (layout);

    if (sheet->use_theme_colors)
    {
        gdk_rgba_free(bg_color);
        gdk_rgba_free(fg_color);
    }
}

static void
draw_block (GnucashSheet *sheet,
            SheetBlock *block,
            VirtualLocation virt_loc,
            cairo_t *cr,
            int x, int y, int width, int height)
{
    CellDimensions *cd;
    gint x_paint;
    gint y_paint;
    gint w, h;

    for ( virt_loc.phys_row_offset = 0;
            virt_loc.phys_row_offset < block->style->nrows ;
            virt_loc.phys_row_offset++ )
    {
        for ( virt_loc.phys_col_offset = 0;
                virt_loc.phys_col_offset < block->style->ncols ;
                virt_loc.phys_col_offset++ )
        {
            cd = gnucash_style_get_cell_dimensions
                 (block->style,
                  virt_loc.phys_row_offset,
                  virt_loc.phys_col_offset);

            x_paint = block->origin_x + cd->origin_x - x;
            if (x_paint > width)
                break;

            y_paint = block->origin_y + cd->origin_y - y;
            if (y_paint > height)
                return;

            h = cd->pixel_height;
            w = cd->pixel_width;

            if (w == 0)
                continue;

            if (x_paint + w < 0)
                continue;

            if (y_paint + h < 0)
                continue;

            draw_cell (sheet, block, virt_loc, cr,
                       x_paint, y_paint, w, h);
        }
    }
}

gboolean
gnucash_sheet_draw_internal (GnucashSheet* sheet, cairo_t* cr,
                             GtkAllocation* alloc)
{
    VirtualLocation virt_loc;
    SheetBlock *sheet_block;
    int x = 0;
    int y = 0;
    int width = alloc->width;
    int height = alloc->height;
    GtkAdjustment * adj;

    adj = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE (sheet));
    x = (gint) gtk_adjustment_get_value(adj);
    adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE (sheet));
    y = (gint) gtk_adjustment_get_value(adj);

    if (x < 0 || y < 0)
        return FALSE;

    /* compute our initial values where we start drawing */
    sheet_block = find_block_by_pixel (sheet, x, y,
                  &virt_loc.vcell_loc);
    if (!sheet_block || !sheet_block->style)
        return FALSE;

    for ( ; virt_loc.vcell_loc.virt_row < sheet->num_virt_rows;
            virt_loc.vcell_loc.virt_row++ )
    {
        while (TRUE)
        {
            sheet_block = gnucash_sheet_get_block
                          (sheet, virt_loc.vcell_loc);

            if (!sheet_block || !sheet_block->style)
                return TRUE;

            if (sheet_block->visible)
                break;

            virt_loc.vcell_loc.virt_row++;
        }

        if (y + height < sheet_block->origin_y)
            return TRUE;

        draw_block (sheet, sheet_block, virt_loc, cr,
                    x, y, width, height);
    }

    return TRUE;
}


void
gnucash_sheet_draw_cursor (GnucashCursor *cursor, cairo_t *cr)
{
    GnucashCursorCell *cc = &(cursor->cell);
    GdkRGBA *fg_color;
    int x = 0;
    int y = 0;
    GtkAdjustment * adj;

    adj = gtk_scrollable_get_hadjustment(GTK_SCROLLABLE (cursor->sheet));
    x = (gint) gtk_adjustment_get_value(adj);
    adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE (cursor->sheet));
    y = (gint) gtk_adjustment_get_value(adj);

    fg_color = &gn_black;

    /* draw the rectangle around the entire active
     *    virtual *row */
    cairo_rectangle (cr, cursor->x - x + 0.5, cursor->y - y + 0.5,
                     cursor->w - 1.0, cursor->h - 1.0);
    cairo_move_to (cr, cursor->x - x, cursor->y - y + cursor->h - 1.5);
    cairo_rel_line_to (cr, cursor->w, 0);
    cairo_set_source_rgb (cr, fg_color->red, fg_color->green, fg_color->blue);
    cairo_set_line_width (cr, 1.0);
    cairo_stroke (cr);

    cairo_rectangle (cr, cc->x - x + 0.5, cursor->y + cc->y - y + 0.5,
                     cc->w - 1.0, cc->h - 1.0);
    cairo_set_source_rgb (cr, fg_color->red, fg_color->green, fg_color->blue);
    cairo_set_line_width (cr, 1.0);
    cairo_stroke (cr);
}
