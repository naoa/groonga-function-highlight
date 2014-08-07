/* 
  Copyright(C) 2014 Naoya Murakami <naoya@createfield.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <groonga/plugin.h>

#ifdef __GNUC__
# define GNUC_UNUSED __attribute__((__unused__))
#else
# define GNUC_UNUSED
#endif

static grn_rc
grn_pat_tag_keys(grn_ctx *ctx,
                 const char *string, unsigned int string_length,
                 grn_obj *keywords, unsigned int max_hits,
                 const char **open_tags, unsigned int *open_tag_lens,
                 const char **close_tags, unsigned int *close_tag_lens,
                 unsigned int n_tags, grn_obj *result)
{
  grn_pat_scan_hit hits[max_hits];
  const char *rest;

  while (string_length > 0) {
    unsigned int i, nhits;
    unsigned int previous = 0;

    nhits = grn_pat_scan(ctx, (grn_pat *)keywords,
                         string , string_length,
                         hits, sizeof(hits) / sizeof(*hits), &rest);

    for (i = 0; i < nhits; i++) {
      if (hits[i].offset - previous > 0) {
        GRN_TEXT_PUT(ctx, result,
                     string + previous, hits[i].offset - previous);
      }
      if (n_tags > 1) {
        GRN_TEXT_PUT(ctx, result,
                     open_tags[hits[i].id - 1], open_tag_lens[hits[i].id - 1]);
        GRN_TEXT_PUT(ctx, result,
                     string + hits[i].offset, hits[i].length);
        GRN_TEXT_PUT(ctx, result,
                     close_tags[hits[i].id - 1], close_tag_lens[hits[i].id - 1]);
      } else {
        GRN_TEXT_PUT(ctx, result,
                     open_tags[0], open_tag_lens[0]);
        GRN_TEXT_PUT(ctx, result,
                     string + hits[i].offset, hits[i].length);
        GRN_TEXT_PUT(ctx, result,
                     close_tags[0], close_tag_lens[0]);
      }
      previous = hits[i].offset + hits[i].length;
    }
    if (string_length - previous > 0) {
      GRN_TEXT_PUT(ctx, result,
                   string + previous, string_length - previous);
    }
    string_length -= rest - string;
    string = rest;
  }
  return GRN_SUCCESS;
}

static grn_obj *
func_highlight_full(grn_ctx *ctx, GNUC_UNUSED int nargs, GNUC_UNUSED grn_obj **args,
                    grn_user_data *user_data)
{
  grn_obj *highlight = NULL;

  if (nargs > 6 && nargs % 3 == 0) {
    grn_obj *string = args[0];
    unsigned int max_hits = GRN_UINT32_VALUE(args[1]);

    grn_rc rc;
    unsigned int i;
    unsigned int n_tags = (nargs - 3) / 3;

    const char *open_tags[n_tags];
    unsigned int open_tag_lens[n_tags];
    const char *close_tags[n_tags];
    unsigned int close_tag_lens[n_tags];

    grn_obj *keywords;

    keywords = grn_table_create(ctx, NULL, 0, NULL,
                                GRN_OBJ_TABLE_PAT_KEY,
                                grn_ctx_at(ctx, GRN_DB_SHORT_TEXT),
                                NULL);

    if (GRN_TEXT_LEN(args[2])) {
      grn_obj * normalizer;
      normalizer = grn_ctx_get(ctx, GRN_TEXT_VALUE(args[2]), GRN_TEXT_LEN(args[2]));
      //TODO: checks normalizer object
      grn_obj_set_info(ctx, keywords, GRN_INFO_NORMALIZER, normalizer);
      grn_obj_unlink(ctx, normalizer);
    }

    grn_obj result;
    unsigned int tagged_len;

    tagged_len = GRN_TEXT_LEN(string);

    for (i = 1; i <= n_tags; i++) {
      grn_table_add(ctx, keywords,
                    GRN_TEXT_VALUE(args[i * 3]), GRN_TEXT_LEN(args[i * 3]), NULL);

      open_tags[i - 1] = GRN_TEXT_VALUE(args[i * 3 + 1]);
      open_tag_lens[i - 1] = GRN_TEXT_LEN(args[i * 3 + 1]);
      close_tags[i - 1] = GRN_TEXT_VALUE(args[i * 3 + 2]);
      close_tag_lens[i - 1] = GRN_TEXT_LEN(args[i * 3 + 2]);

      tagged_len += open_tag_lens[i - 1] + close_tag_lens[i - 1];
    }

    GRN_TEXT_INIT(&result, 0);
    // tagged_lenは、各タグが1回ヒットした場合のタグ長を足しこんだにすぎない。
    // 正確なバッファ長を知るためには、一度スキャンしないといけない。
    // snippetと同じようにexecとget_resultを分けるかを検討
    grn_bulk_space(ctx, &result, tagged_len);
    GRN_BULK_REWIND(&result);

    rc = grn_pat_tag_keys(ctx,
                          GRN_TEXT_VALUE(string), GRN_TEXT_LEN(string),
                          keywords, max_hits,
                          open_tags, open_tag_lens,
                          close_tags, close_tag_lens,
                          n_tags, &result);

    if (rc == GRN_SUCCESS) {
      if (GRN_TEXT_LEN(&result) < 4096) {
        highlight = grn_plugin_proc_alloc(ctx, user_data, GRN_DB_SHORT_TEXT, 0);
      } else if (GRN_TEXT_LEN(&result) < 65536) {
        highlight = grn_plugin_proc_alloc(ctx, user_data, GRN_DB_TEXT, 0);
      } else {
        highlight = grn_plugin_proc_alloc(ctx, user_data, GRN_DB_LONG_TEXT, 0);
      }
      if (highlight) {
        GRN_TEXT_SET(ctx, highlight, GRN_TEXT_VALUE(&result), GRN_TEXT_LEN(&result));
      }
    }
    grn_obj_unlink(ctx, keywords);
    grn_obj_unlink(ctx, &result);
  }

  if (!highlight) {
    highlight = grn_plugin_proc_alloc(ctx, user_data, GRN_DB_VOID, 0);
  }

  return highlight;
}

grn_rc
GRN_PLUGIN_INIT(GNUC_UNUSED grn_ctx *ctx)
{
  return GRN_SUCCESS;
}

grn_rc
GRN_PLUGIN_REGISTER(grn_ctx *ctx)
{
  grn_proc_create(ctx, "highlight_full", -1, GRN_PROC_FUNCTION,
                  func_highlight_full, NULL, NULL, 0, NULL);

  return ctx->rc;
}

grn_rc
GRN_PLUGIN_FIN(GNUC_UNUSED grn_ctx *ctx)
{
  return GRN_SUCCESS;
}
