/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * lapiz-style-scheme-manager.h
 *
 * Copyright (C) 2007 - Paolo Borelli
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * $Id: lapiz-source-style-manager.h 5598 2007-04-15 13:16:24Z pborelli $
 */

#ifndef __LAPIZ_STYLE_SCHEME_MANAGER_H__
#define __LAPIZ_STYLE_SCHEME_MANAGER_H__

#include <ctksourceview/ctksource.h>

G_BEGIN_DECLS

CtkSourceStyleSchemeManager *
		 lapiz_get_style_scheme_manager		(void);

/* Returns a sorted list of style schemes */
GSList		*lapiz_style_scheme_manager_list_schemes_sorted
							(CtkSourceStyleSchemeManager *manager);

/*
 * Non exported functions
 */
gboolean	 _lapiz_style_scheme_manager_scheme_is_lapiz_user_scheme
							(CtkSourceStyleSchemeManager *manager,
							 const gchar                 *scheme_id);

const gchar	*_lapiz_style_scheme_manager_install_scheme
							(CtkSourceStyleSchemeManager *manager,
							 const gchar                 *fname);

gboolean	 _lapiz_style_scheme_manager_uninstall_scheme
							(CtkSourceStyleSchemeManager *manager,
							 const gchar                 *id);

G_END_DECLS

#endif /* __LAPIZ_STYLE_SCHEME_MANAGER_H__ */
